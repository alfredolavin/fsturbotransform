// fsturbotransform-gui: the QML front end of fsturbotransform. It drives the same engine as the
// command line (src/renamer.hpp) through TransformController.
//
//   fsturbotransform-gui [DIRECTORY]
//   fsturbotransform-gui --screenshot=FILE [--width=W --height=H] [DIRECTORY]   (saves the window, then quits)

#include <KColorScheme>
#include <KLocalizedQmlContext>
#include <KSharedConfig>
#include <KLocalizedString>
#include <QApplication>
#include <QStyle>
#include <QCommandLineParser>
#include <QDir>
#include <QFontDatabase>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include "console_format.hpp"
#include "fira_code_font.hpp"
#include "icon_provider.hpp"
#include "transform_controller.hpp"

int main(int argc, char* argv[]) {
    // org.kde.desktop draws the controls with the Plasma theme; it needs a QApplication.
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    QApplication app(argc, argv);
    // With Kvantum as the QStyle, org.kde.desktop paints fields, check boxes and spin boxes dark while the text
    // colour comes from the (light) KDE scheme: unreadable. Breeze instead, unless the user forces a style.
    if (QApplication::style() != nullptr && QApplication::style()->name().startsWith(QStringLiteral("kvantum")) &&
        qEnvironmentVariableIsEmpty("QT_STYLE_OVERRIDE"))
    {
        QApplication::setStyle(QStringLiteral("Breeze"));
        // Kvantum also set its own (dark) palette: back to the desktop scheme's, which Kirigami uses too
        // (the icon provider recolours Breeze icons with this palette).
        QApplication::setPalette(KColorScheme::createApplicationPalette(KSharedConfig::openConfig()));
    }
    QApplication::setApplicationName(QStringLiteral("fsturbotransform"));
    QApplication::setApplicationDisplayName(QStringLiteral("FS-Turbo-Transformer"));
    QApplication::setApplicationVersion(QStringLiteral("2.0"));
    QApplication::setDesktopFileName(QStringLiteral("fsturbotransform-gui"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/qt/qml/FsTurbo/icons/app.png")));
    if (QQuickStyle::name().isEmpty()) QQuickStyle::setFallbackStyle(QStringLiteral("Fusion"));

    // The same Fira Code the terminal dashboard draws with, baked into the binary (#embed).
    QFontDatabase::addApplicationFontFromData(
        QByteArray::fromRawData(reinterpret_cast<const char*>(fsturbo::fira_code_ttf), static_cast<qsizetype>(fsturbo::fira_code_ttf_len)));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Rename, re-case and flatten directory trees"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption screenshot(QStringLiteral("screenshot"), QStringLiteral("Save the window to <file> and quit."), QStringLiteral("file"));
    const QCommandLineOption width(QStringLiteral("width"), QStringLiteral("Window width."), QStringLiteral("px"));
    const QCommandLineOption height(QStringLiteral("height"), QStringLiteral("Window height."), QStringLiteral("px"));
    const QCommandLineOption run_preview(QStringLiteral("preview"), QStringLiteral("Start a preview (dry run) right away."));
    const QCommandLineOption console_format(QStringLiteral("console-format"), QStringLiteral("Open the console format dialog."));
    parser.addOptions({screenshot, width, height, run_preview, console_format});
    parser.addPositionalArgument(QStringLiteral("directory"), QStringLiteral("The directory to transform."));
    parser.process(app);

    QQmlApplicationEngine engine;
    // i18n() for the shared controls of plasmoids/shared (the console format dialog)
    KLocalizedString::setApplicationDomain("fsturbotransform");
    KLocalization::setupLocalizedContext(&engine);
    engine.addImageProvider(QStringLiteral("icon"), new fsturbo::gui::IconProvider);
    engine.addImageProvider(QStringLiteral("console"), new fsturbo::gui::ConsolePreviewProvider);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("FsTurbo", "Main");
    if (engine.rootObjects().isEmpty()) return 1;

    auto* controller = engine.singletonInstance<fsturbo::gui::TransformController*>("FsTurbo", "TransformController");
    if (const QStringList args = parser.positionalArguments(); !args.isEmpty() && controller != nullptr)
        controller->setTargetDir(QDir(args.first()).absolutePath());
    if (parser.isSet(run_preview) && controller != nullptr) QTimer::singleShot(400, controller, [controller] { controller->preview(); });

    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (window != nullptr && parser.isSet(width)) window->setWidth(parser.value(width).toInt());
    if (window != nullptr && parser.isSet(height)) window->setHeight(parser.value(height).toInt());
    if (window != nullptr && parser.isSet(console_format))
        QTimer::singleShot(300, window, [window] { QMetaObject::invokeMethod(window, "openConsoleFormat"); });
    if (window != nullptr && parser.isSet(screenshot)) {
        // Long enough for validation, icons and a --preview run to settle.
        // Every visible window: FILE, then FILE-1.png, FILE-2.png … for dialogs.
        QTimer::singleShot(parser.isSet(run_preview) ? 2500 : 1500, window, [window, file = parser.value(screenshot)] {
            window->grabWindow().save(file);
            int n = 0;
            for (QWindow* w : QGuiApplication::topLevelWindows())
                if (auto* q = qobject_cast<QQuickWindow*>(w); q && q != window && q->isVisible())
                    q->grabWindow().save(QString(file).replace(QStringLiteral(".png"), QStringLiteral("-%1.png").arg(++n)));
            QCoreApplication::quit();
        });
    }
    return QApplication::exec();
}

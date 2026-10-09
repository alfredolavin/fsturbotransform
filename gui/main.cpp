// fsturbotransform-gui: the QML front end of fsturbotransform. It drives the same engine as the
// command line (src/renamer.hpp) through TransformController.
//
//   fsturbotransform-gui [DIRECTORY]
//   fsturbotransform-gui --screenshot=FILE [--width=W --height=H] [DIRECTORY]   (saves the window, then quits)

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFontDatabase>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include "fira_code_font.hpp"
#include "icon_provider.hpp"
#include "transform_controller.hpp"

int main(int argc, char* argv[]) {
    // org.kde.desktop draws the controls with the Plasma theme; it needs a QApplication.
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    QApplication app(argc, argv);
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
    parser.addOptions({screenshot, width, height, run_preview});
    parser.addPositionalArgument(QStringLiteral("directory"), QStringLiteral("The directory to transform."));
    parser.process(app);

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("icon"), new fsturbo::gui::IconProvider);
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
    if (window != nullptr && parser.isSet(screenshot)) {
        // Long enough for validation, icons and a --preview run to settle.
        QTimer::singleShot(parser.isSet(run_preview) ? 2500 : 1200, window, [window, file = parser.value(screenshot)] {
            window->grabWindow().save(file);
            QCoreApplication::quit();
        });
    }
    return QApplication::exec();
}

#ifndef GUI_ICON_PROVIDER_HPP
#define GUI_ICON_PROVIDER_HPP

// image://icon/<name>: the bundled SVG icons (gui/icons, see SOURCES.md) rendered at the size
// asked for. Breeze icons draw with the ".ColorScheme-*" classes; they are recoloured with the
// application palette, as KDE's icon loader does, so monochrome ones stay visible on dark themes.
// Anything after a '?' in the name only serves to make QML reload the image when the palette
// changes.

#include <QFile>
#include <QGuiApplication>
#include <QPainter>
#include <QPalette>
#include <QQuickImageProvider>
#include <QRegularExpression>
#include <QSvgRenderer>

namespace fsturbo::gui {

class IconProvider final : public QQuickImageProvider {
public:
    IconProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    QImage requestImage(const QString& id, QSize* size, const QSize& requested) override {
        const QString name = id.section(u'?', 0, 0);
        QFile file(QStringLiteral(":/qt/qml/FsTurbo/icons/%1.svg").arg(name));
        const QSize out_size = requested.isValid() && !requested.isEmpty() ? requested : QSize(64, 64);
        if (size != nullptr) *size = out_size;
        QImage image(out_size, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        if (!file.open(QIODevice::ReadOnly)) return image;

        QString svg = QString::fromUtf8(file.readAll());
        const QPalette palette = QGuiApplication::palette();
        const auto recolour = [&](QStringView cls, const QColor& colour) {
            static const QString pattern = QStringLiteral(R"((\.ColorScheme-%1\s*\{\s*color:\s*)#[0-9a-fA-F]{3,8})");
            svg.replace(QRegularExpression(pattern.arg(cls)), QStringLiteral("\\1") + colour.name());
        };
        recolour(u"Text", palette.color(QPalette::WindowText));
        recolour(u"Background", palette.color(QPalette::Window));
        recolour(u"Highlight", palette.color(QPalette::Highlight));
        recolour(u"Accent", palette.color(QPalette::Accent));

        QSvgRenderer renderer(svg.toUtf8());
        QPainter painter(&image);
        renderer.render(&painter, QRectF(QPointF(0, 0), out_size));
        return image;
    }
};

} // namespace fsturbo::gui

#endif // GUI_ICON_PROVIDER_HPP

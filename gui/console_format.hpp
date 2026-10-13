#ifndef GUI_CONSOLE_FORMAT_HPP
#define GUI_CONSOLE_FORMAT_HPP

// The mini-console's format (src/console_style.hpp) for QML: the values of console.conf, written
// back as they change, and image://console/…, a terminal frame drawn with them (the same renderer
// as the terminal dashboard). The terminal and the GUI's console view read the same file.
//
//   ConsoleFormat.values.background        the stored value (colours: a colorspec string)
//   ConsoleFormat.set("background", v)    store (saved after a short pause)

#include <QFileSystemWatcher>
#include <QObject>
#include <QQuickImageProvider>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include "console_style.hpp"

namespace fsturbo::gui {

class ConsoleFormat : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QVariantMap values READ values NOTIFY changed)
    Q_PROPERTY(QVariantMap defaults READ defaults CONSTANT)
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(QString path READ path CONSTANT)
    Q_PROPERTY(QStringList monospaceFamilies READ monospaceFamilies CONSTANT)

public:
    explicit ConsoleFormat(QObject* parent = nullptr);
    ~ConsoleFormat() override;

    // The one instance (the QML singleton), for the image provider.
    static ConsoleFormat* instance();

    QVariantMap values() const;
    QVariantMap defaults() const;
    int revision() const { return revision_; }
    QString path() const;
    QStringList monospaceFamilies() const;

    // Stores one option (bools and numbers as given, colours as colorspec strings).
    Q_INVOKABLE void set(const QString& key, const QVariant& value);
    // Every option back to its default.
    Q_INVOKABLE void reset();

    // The style, with the active terminal palette set from it (system colours from kdeglobals).
    const style::ConsoleStyle& style() const { return style_; }
    void activate() const;

Q_SIGNALS:
    void changed();

private:
    void reload();

    style::ConsoleStyle style_;
    int revision_ = 0;
    QTimer save_timer_;
    QFileSystemWatcher watcher_; // the file edited by hand or by another instance
    bool saving_ = false;
};

// image://console/<anything>: the preview frame, requestedSize wide (cells of 20 px).
class ConsolePreviewProvider final : public QQuickImageProvider {
public:
    ConsolePreviewProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString& id, QSize* size, const QSize& requested) override;
};

} // namespace fsturbo::gui

#endif // GUI_CONSOLE_FORMAT_HPP

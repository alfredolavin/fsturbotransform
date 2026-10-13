#include "console_format.hpp"

#include <QFileInfo>
#include <QFontDatabase>
#include <QImage>
#include <cstring>
#include "console_preview.hpp"

namespace fsturbo::gui {

namespace {

ConsoleFormat* g_instance = nullptr;

QString qstr(std::string_view s) { return QString::fromUtf8(s.data(), static_cast<qsizetype>(s.size())); }

QVariant typed(const style::Option& o, const std::string& stored) {
    switch (o.kind) {
        case style::Kind::Bool: return stored == "true" || stored == "1" || stored == "yes" || stored == "on";
        case style::Kind::Int: return std::atoi(stored.c_str());
        default: return qstr(stored);
    }
}

} // namespace

ConsoleFormat::ConsoleFormat(QObject* parent) : QObject(parent) {
    g_instance = this;
    save_timer_.setSingleShot(true);
    save_timer_.setInterval(250);
    connect(&save_timer_, &QTimer::timeout, this, [this] {
        saving_ = true;
        style_.save(style::ConsoleStyle::default_path());
        saving_ = false;
        if (watcher_.files().isEmpty()) watcher_.addPath(path());
    });
    connect(&watcher_, &QFileSystemWatcher::fileChanged, this, [this] {
        if (saving_ || save_timer_.isActive()) return;
        reload();
        // An editor that replaces the file drops it from the watch list.
        if (QFileInfo::exists(path()) && watcher_.files().isEmpty()) watcher_.addPath(path());
    });
    reload();
    if (QFileInfo::exists(path())) watcher_.addPath(path());
}

ConsoleFormat::~ConsoleFormat() {
    if (save_timer_.isActive()) style_.save(style::ConsoleStyle::default_path());
    if (g_instance == this) g_instance = nullptr;
}

ConsoleFormat* ConsoleFormat::instance() { return g_instance; }

void ConsoleFormat::reload() {
    style_ = style::ConsoleStyle::load(style::ConsoleStyle::default_path());
    ++revision_;
    Q_EMIT changed();
}

QVariantMap ConsoleFormat::values() const {
    QVariantMap out;
    for (const style::Option& o : style::kOptions) out.insert(qstr(o.key), typed(o, style_.get(o.key)));
    return out;
}

QVariantMap ConsoleFormat::defaults() const {
    QVariantMap out;
    for (const style::Option& o : style::kOptions) out.insert(qstr(o.key), typed(o, std::string(o.fallback)));
    return out;
}

QString ConsoleFormat::path() const { return QString::fromStdString(style::ConsoleStyle::default_path().string()); }

QStringList ConsoleFormat::monospaceFamilies() const {
    QStringList out;
    for (const QString& family : QFontDatabase::families())
        if (QFontDatabase::isFixedPitch(family) && !QFontDatabase::isPrivateFamily(family) && family != "Fira Code") out.append(family);
    return out;
}

void ConsoleFormat::set(const QString& key, const QVariant& value) {
    const std::string k = key.toStdString();
    const style::Option* o = style::find_option(k);
    if (o == nullptr) return;
    const std::string v = o->kind == style::Kind::Bool ? (value.toBool() ? "true" : "false")
                          : o->kind == style::Kind::Int ? std::to_string(value.toInt())
                                                        : value.toString().simplified().toStdString(); // one line in the file
    if (style_.get(k) == v) return;
    style_.set(k, v);
    ++revision_;
    Q_EMIT changed();
    save_timer_.start();
}

void ConsoleFormat::reset() {
    style_ = style::ConsoleStyle();
    ++revision_;
    Q_EMIT changed();
    save_timer_.start();
}

void ConsoleFormat::activate() const { style::active() = style::ConsolePalette::from(style_, style::SystemColors::load()); }

QImage ConsolePreviewProvider::requestImage(const QString&, QSize* size, const QSize& requested) {
    const int width = requested.width() > 0 ? requested.width() : 760;
    const int cell_h = requested.height() > 0 ? std::max(12, requested.height() / 9) : 20; // 9 rows: 6 lines and the chrome
    if (ConsoleFormat* format = ConsoleFormat::instance()) format->activate();
    const sixel::Canvas canvas = ui::render_console_preview(width, cell_h, 6);
    QImage image(canvas.width(), canvas.height(), QImage::Format_RGBA8888);
    const auto px = canvas.pixels();
    std::memcpy(image.bits(), px.data(), px.size_bytes());
    if (size != nullptr) *size = image.size();
    return image;
}

} // namespace fsturbo::gui

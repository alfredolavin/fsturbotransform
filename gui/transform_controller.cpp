#include "transform_controller.hpp"

#include <atomic>
#include <filesystem>
#include <mutex>
#include <QDir>
#include <QLocale>
#include <QRegularExpression>
#include "regex_tokens.hpp"
#include "renamer.hpp"

namespace fsturbo::gui {

namespace {

std::string utf8(const QString& s) { return s.toStdString(); }
QString qstr(std::string_view s) { return QString::fromUtf8(s.data(), static_cast<qsizetype>(s.size())); }

// The UTF-16 index in `spec` of the PCRE2 error that `error` reports ("... (at offset N)"), or -1.
int error_position(const std::string& spec, const std::string& error, bool arrows) {
    static const QRegularExpression at(QStringLiteral(R"(\(at offset (\d+)\)$)"));
    const QRegularExpressionMatch m = at.match(qstr(error));
    if (!m.hasMatch()) return -1;
    const std::size_t offset = m.captured(1).toULongLong();
    const std::size_t byte = arrows ? syntax::spec_offset_of_pattern_error(spec, offset) : 1 + offset;
    return static_cast<int>(qstr(std::string_view(spec).substr(0, std::min(byte, spec.size()))).size());
}

} // namespace

// Collects what the engine reports on the worker thread; the GUI thread takes it in poll().
class GuiSink final : public ProgressSink {
public:
    std::atomic<bool> cancel{false};
    std::atomic<std::size_t> current{0}, total{0};

    void begin_phase(std::string_view phase, std::size_t count) override {
        const std::lock_guard lock(mutex_);
        phase_ = qstr(phase);
        total = count;
        current = 0;
    }

    void step(std::size_t at, std::string_view action, std::string_view source, std::string_view dest, bool changed) override {
        current = at;
        if (!changed) return;
        const auto kind = action == "FLATTEN" ? ChangeRow::Kind::Flatten : action == "DIR RENAME" ? ChangeRow::Kind::Directory : ChangeRow::Kind::File;
        const std::lock_guard lock(mutex_);
        rows_.push_back({kind, qstr(source), qstr(dest)});
    }

    void error(std::string_view message) override {
        const std::lock_guard lock(mutex_);
        rows_.push_back({ChangeRow::Kind::Error, qstr(message), {}});
    }

    bool cancelled() const override { return cancel; }

    // The rows reported since the last call, and the current phase.
    std::vector<ChangeRow> take(QString& phase) {
        const std::lock_guard lock(mutex_);
        phase = phase_;
        return std::exchange(rows_, {});
    }

private:
    std::mutex mutex_;
    std::vector<ChangeRow> rows_;
    QString phase_;
};

TransformController::TransformController(QObject* parent)
    : QObject(parent), settings_(QStringLiteral("fsturbotransform"), QStringLiteral("gui")) {
    validate_timer_.setSingleShot(true);
    validate_timer_.setInterval(180);
    connect(&validate_timer_, &QTimer::timeout, this, [this] {
        validate();
        save_settings();
    });
    poll_timer_.setInterval(50);
    connect(&poll_timer_, &QTimer::timeout, this, &TransformController::poll);
    load_settings();
    validate();
}

TransformController::~TransformController() {
    if (sink_) sink_->cancel = true;
    if (worker_.joinable()) worker_.join();
}

void TransformController::load_settings() {
    QSettings& s = settings_;
    target_dir_ = s.value("targetDir", QDir::homePath()).toString();
    case_style_ = s.value("caseStyle", 1).toInt();
    regex_ = s.value("regex").toString();
    flatten_ = s.value("flatten", false).toBool();
    flatten_regex_ = s.value("flattenRegex").toString();
    excludes_ = s.value("excludes").toStringList();
    includes_ = s.value("includes").toStringList();
    gitignore_files_ = s.value("gitignoreFiles").toStringList();
    target_gitignore_ = s.value("targetGitignore", true).toBool();
    recursive_ = s.value("recursive", true).toBool();
    overwrite_ = s.value("overwrite", false).toBool();
    sample_name_ = s.value("sampleName", QStringLiteral("My Holiday Photo 2024-07-15.JPG")).toString();
}

void TransformController::save_settings() {
    QSettings& s = settings_;
    s.setValue("targetDir", target_dir_);
    s.setValue("caseStyle", case_style_);
    s.setValue("regex", regex_);
    s.setValue("flatten", flatten_);
    s.setValue("flattenRegex", flatten_regex_);
    s.setValue("excludes", excludes_);
    s.setValue("includes", includes_);
    s.setValue("gitignoreFiles", gitignore_files_);
    s.setValue("targetGitignore", target_gitignore_);
    s.setValue("recursive", recursive_);
    s.setValue("overwrite", overwrite_);
    s.setValue("sampleName", sample_name_);
}

bool TransformController::canRun() const {
    return !running_ && target_error_.isEmpty() && regex_error_.isEmpty() && flatten_regex_error_.isEmpty();
}

RenameOptions TransformController::build_options(bool dry_run, QStringList* unreadable) const {
    RenameOptions opts;
    opts.target_dir = std::filesystem::path(utf8(target_dir_));
    opts.depth_origin = opts.target_dir; // `depth` counts from the chosen directory, not from where the GUI was started
    opts.case_style = static_cast<CaseStyle>(std::clamp(case_style_, 0, static_cast<int>(CaseStyle::Title)));
    opts.case_style_set = opts.case_style != CaseStyle::None;
    opts.regex_rename = utf8(regex_.trimmed());
    opts.flatten = flatten_;
    opts.flatten_regex = utf8(flatten_regex_.trimmed());
    opts.dry_run = dry_run;
    opts.recursive = recursive_;
    opts.overwrite = overwrite_;
    for (const QString& p : excludes_) opts.filter.add_exclude(utf8(p));
    for (const QString& p : includes_) opts.filter.add_include(utf8(p));
    std::vector<std::filesystem::path> files;
    for (const QString& f : gitignore_files_)
        if (!f.trimmed().isEmpty()) files.emplace_back(utf8(f));
    for (const auto& bad : load_gitignores(opts, files, target_gitignore_))
        if (unreadable != nullptr) unreadable->append(qstr(bad.string()));
    return opts;
}

void TransformController::validate() {
    std::error_code ec;
    target_error_ = target_dir_.trimmed().isEmpty()                                  ? tr("Choose the directory to transform")
                    : !std::filesystem::is_directory(utf8(target_dir_), ec)          ? tr("This directory does not exist")
                                                                                     : QString();

    const std::string flatten_spec = utf8(flatten_regex_.trimmed());
    flatten_regex_error_.clear();
    flatten_regex_error_position_ = -1;
    if (!flatten_spec.empty()) {
        const RegexSpec spec = RegexSpec::parse(flatten_spec, false);
        if (!spec.valid) {
            flatten_regex_error_ = qstr(spec.error);
            flatten_regex_error_position_ = error_position(flatten_spec, spec.error, true);
        }
    }

    // The rename spec may hold JavaScript: V8 is not started here while a run uses it on the worker.
    if (!running_) {
        RenameOptions opts;
        opts.case_style = static_cast<CaseStyle>(std::clamp(case_style_, 0, static_cast<int>(CaseStyle::Title)));
        opts.regex_rename = utf8(regex_.trimmed());
        const TransformerEngine engine(std::move(opts));
        regex_error_ = qstr(engine.config_error());
        regex_error_.remove(QRegularExpression(QStringLiteral(R"(^Invalid --regex '.*': )")));
        regex_error_position_ = regex_error_.isEmpty() ? -1 : error_position(utf8(regex_.trimmed()), engine.config_error(), true);

        sample_result_.clear();
        sample_error_.clear();
        if (regex_error_.isEmpty() && !sample_name_.isEmpty()) {
            const std::string name = utf8(sample_name_);
            const EntryContext ctx{1, 1, ttf::utf8_length(name), 0};
            if (const auto renamed = engine.transform_name(name, ctx); !renamed) sample_error_ = qstr(renamed.error());
            else if (!TransformerEngine::is_valid_name(*renamed)) sample_error_ = tr("“%1” is not a valid file name").arg(qstr(*renamed));
            else sample_result_ = qstr(*renamed);
        }
    }
    Q_EMIT validationChanged();
}

QVariantMap TransformController::checkFilter(const QString& pattern) const {
    const std::string text = utf8(pattern.trimmed());
    if (!is_regex_spec(text)) return {{"error", QString()}, {"position", -1}};
    const std::size_t last = text.rfind('/');
    const auto compiled = Regex::compile(std::string_view(text).substr(1, last - 1), std::string_view(text).substr(last + 1));
    if (compiled) return {{"error", QString()}, {"position", -1}};
    return {{"error", qstr(compiled.error())}, {"position", error_position(text, compiled.error(), false)}};
}

QString TransformController::caseExample(int style, const QString& text) const {
    if (style <= 0) return text;
    return qstr(CaseConverter::transform_dynamic(utf8(text), static_cast<CaseStyle>(style)));
}

void TransformController::start(bool dry_run) {
    if (running_) return;
    validate();
    if (!canRun()) return;
    if (worker_.joinable()) worker_.join();

    QStringList unreadable;
    RenameOptions opts = build_options(dry_run, &unreadable);
    changes_.clear();
    std::vector<ChangeRow> notes;
    for (const QString& file : unreadable) notes.push_back({ChangeRow::Kind::Error, tr("Cannot read the gitignore file %1").arg(file), {}});
    changes_.append(std::move(notes));

    running_ = true;
    preview_run_ = dry_run;
    has_results_ = true;
    was_cancelled_ = false;
    phase_.clear();
    progress_current_ = progress_total_ = 0;
    stats_.clear();
    Q_EMIT runStateChanged();
    Q_EMIT progressChanged();
    Q_EMIT statsChanged();
    Q_EMIT validationChanged(); // canRun

    sink_ = std::make_unique<GuiSink>();
    worker_ = std::jthread([this, sink = sink_.get(), opts = std::move(opts)]() mutable {
        TransformerEngine engine(std::move(opts));
        const ExecutionStats s = engine.run(*sink);
        const QVariantMap stats{
            {"scannedDirs", qulonglong(s.scanned_dirs)},     {"scannedFiles", qulonglong(s.scanned_files)},
            {"renamedDirs", qulonglong(s.renamed_dirs)},     {"renamedFiles", qulonglong(s.renamed_files)},
            {"flattenedFiles", qulonglong(s.flattened_files)}, {"excludedItems", qulonglong(s.excluded_items)},
            {"errors", qulonglong(s.errors)},                 {"durationMs", s.duration_ms},
        };
        const bool cancelled = sink->cancelled();
        QMetaObject::invokeMethod(this, [this, stats, cancelled] { finish_run(stats, cancelled); }, Qt::QueuedConnection);
    });
    poll_timer_.start();
}

void TransformController::cancel() {
    if (running_ && sink_) sink_->cancel = true;
}

void TransformController::poll() {
    if (!sink_) return;
    QString phase;
    changes_.append(sink_->take(phase));
    const double current = static_cast<double>(sink_->current.load()), total = static_cast<double>(sink_->total.load());
    if (phase != phase_ || current != progress_current_ || total != progress_total_) {
        phase_ = phase;
        progress_current_ = current;
        progress_total_ = total;
        Q_EMIT progressChanged();
    }
}

void TransformController::finish_run(const QVariantMap& stats, bool cancelled) {
    poll_timer_.stop();
    if (worker_.joinable()) worker_.join();
    poll();
    sink_.reset();
    running_ = false;
    was_cancelled_ = cancelled;
    stats_ = stats;
    Q_EMIT runStateChanged();
    Q_EMIT statsChanged();
    validate(); // deferred while the worker owned V8; also refreshes canRun

    const qulonglong changed = stats["renamedDirs"].toULongLong() + stats["renamedFiles"].toULongLong() + stats["flattenedFiles"].toULongLong();
    const qulonglong errors = stats["errors"].toULongLong();
    const QString changes = changed == 1 ? tr("1 change") : tr("%1 changes").arg(QLocale().toString(changed));
    QString summary = preview_run_ ? tr("Preview: %1 would be made").arg(changes) : tr("%1 made").arg(changes);
    if (errors > 0) summary += errors == 1 ? tr(", 1 error") : tr(", %1 errors").arg(QLocale().toString(errors));
    if (cancelled) summary += tr(" (stopped)");
    Q_EMIT finished(summary);
}

} // namespace fsturbo::gui

#ifndef GUI_TRANSFORM_CONTROLLER_HPP
#define GUI_TRANSFORM_CONTROLLER_HPP

// The GUI's model of one fsturbotransform run: the options (the command line's, as properties),
// their live validation, a sample name tried with them, and preview (dry run) / apply runs of
// TransformerEngine on a worker thread, whose progress and changes it publishes to QML.

#include <memory>
#include <thread>
#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include "change_model.hpp"

namespace fsturbo {
struct RenameOptions;
}

namespace fsturbo::gui {

class GuiSink;

class TransformController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Options (see `fsturbotransform --help`)
    Q_PROPERTY(QString targetDir READ targetDir WRITE setTargetDir NOTIFY optionsChanged)
    Q_PROPERTY(int caseStyle READ caseStyle WRITE setCaseStyle NOTIFY optionsChanged)
    Q_PROPERTY(QString regex READ regex WRITE setRegex NOTIFY optionsChanged)
    Q_PROPERTY(bool flatten READ flatten WRITE setFlatten NOTIFY optionsChanged)
    Q_PROPERTY(QString flattenRegex READ flattenRegex WRITE setFlattenRegex NOTIFY optionsChanged)
    Q_PROPERTY(QStringList excludes READ excludes WRITE setExcludes NOTIFY optionsChanged)
    Q_PROPERTY(QStringList includes READ includes WRITE setIncludes NOTIFY optionsChanged)
    Q_PROPERTY(QStringList gitignoreFiles READ gitignoreFiles WRITE setGitignoreFiles NOTIFY optionsChanged)
    Q_PROPERTY(bool targetGitignore READ targetGitignore WRITE setTargetGitignore NOTIFY optionsChanged)
    Q_PROPERTY(bool recursive READ recursive WRITE setRecursive NOTIFY optionsChanged)
    Q_PROPERTY(bool overwrite READ overwrite WRITE setOverwrite NOTIFY optionsChanged)
    Q_PROPERTY(QString sampleName READ sampleName WRITE setSampleName NOTIFY optionsChanged)

    // Validation of the options and the sample name's result
    Q_PROPERTY(QString targetError READ targetError NOTIFY validationChanged)
    Q_PROPERTY(QString regexError READ regexError NOTIFY validationChanged)
    Q_PROPERTY(int regexErrorPosition READ regexErrorPosition NOTIFY validationChanged)
    Q_PROPERTY(QString flattenRegexError READ flattenRegexError NOTIFY validationChanged)
    Q_PROPERTY(int flattenRegexErrorPosition READ flattenRegexErrorPosition NOTIFY validationChanged)
    Q_PROPERTY(QString sampleResult READ sampleResult NOTIFY validationChanged)
    Q_PROPERTY(QString sampleError READ sampleError NOTIFY validationChanged)
    Q_PROPERTY(bool canRun READ canRun NOTIFY validationChanged)

    // The current (or last) run
    Q_PROPERTY(bool running READ running NOTIFY runStateChanged)
    Q_PROPERTY(bool previewRun READ previewRun NOTIFY runStateChanged)
    Q_PROPERTY(bool hasResults READ hasResults NOTIFY runStateChanged)
    Q_PROPERTY(bool wasCancelled READ wasCancelled NOTIFY runStateChanged)
    Q_PROPERTY(QString phase READ phase NOTIFY progressChanged)
    Q_PROPERTY(double progressCurrent READ progressCurrent NOTIFY progressChanged)
    Q_PROPERTY(double progressTotal READ progressTotal NOTIFY progressChanged)
    Q_PROPERTY(QVariantMap stats READ stats NOTIFY statsChanged)
    Q_PROPERTY(ChangeModel* changes READ changes CONSTANT)

public:
    explicit TransformController(QObject* parent = nullptr);
    ~TransformController() override;

    QString targetDir() const { return target_dir_; }
    void setTargetDir(const QString& v) { set(target_dir_, v); }
    int caseStyle() const { return case_style_; }
    void setCaseStyle(int v) { set(case_style_, v); }
    QString regex() const { return regex_; }
    void setRegex(const QString& v) { set(regex_, v); }
    bool flatten() const { return flatten_; }
    void setFlatten(bool v) { set(flatten_, v); }
    QString flattenRegex() const { return flatten_regex_; }
    void setFlattenRegex(const QString& v) { set(flatten_regex_, v); }
    QStringList excludes() const { return excludes_; }
    void setExcludes(const QStringList& v) { set(excludes_, v); }
    QStringList includes() const { return includes_; }
    void setIncludes(const QStringList& v) { set(includes_, v); }
    QStringList gitignoreFiles() const { return gitignore_files_; }
    void setGitignoreFiles(const QStringList& v) { set(gitignore_files_, v); }
    bool targetGitignore() const { return target_gitignore_; }
    void setTargetGitignore(bool v) { set(target_gitignore_, v); }
    bool recursive() const { return recursive_; }
    void setRecursive(bool v) { set(recursive_, v); }
    bool overwrite() const { return overwrite_; }
    void setOverwrite(bool v) { set(overwrite_, v); }
    QString sampleName() const { return sample_name_; }
    void setSampleName(const QString& v) { set(sample_name_, v); }

    QString targetError() const { return target_error_; }
    QString regexError() const { return regex_error_; }
    int regexErrorPosition() const { return regex_error_position_; }
    QString flattenRegexError() const { return flatten_regex_error_; }
    int flattenRegexErrorPosition() const { return flatten_regex_error_position_; }
    QString sampleResult() const { return sample_result_; }
    QString sampleError() const { return sample_error_; }
    bool canRun() const;

    bool running() const { return running_; }
    bool previewRun() const { return preview_run_; }
    bool hasResults() const { return has_results_; }
    bool wasCancelled() const { return was_cancelled_; }
    QString phase() const { return phase_; }
    double progressCurrent() const { return progress_current_; }
    double progressTotal() const { return progress_total_; }
    QVariantMap stats() const { return stats_; }
    ChangeModel* changes() { return &changes_; }

    // Dry run: fills the change list with what apply() would do.
    Q_INVOKABLE void preview() { start(true); }
    // Renames and flattens for real.
    Q_INVOKABLE void apply() { start(false); }
    // Stops the running run after the entry in progress.
    Q_INVOKABLE void cancel();
    // {error, position} for one -e / -i pattern (a glob is always valid).
    Q_INVOKABLE QVariantMap checkFilter(const QString& pattern) const;
    // The case style's name applied to `text` (for the combo box's entries).
    Q_INVOKABLE QString caseExample(int style, const QString& text) const;
    Q_INVOKABLE QString pathFromUrl(const QUrl& url) const { return url.toLocalFile(); }
    Q_INVOKABLE QUrl urlFromPath(const QString& path) const { return QUrl::fromLocalFile(path); }

Q_SIGNALS:
    void optionsChanged();
    void validationChanged();
    void runStateChanged();
    void progressChanged();
    void statsChanged();
    // A run is over; `summary` says how it went in one sentence.
    void finished(const QString& summary);

private:
    template <typename T>
    void set(T& member, const T& value) {
        if (member == value) return;
        member = value;
        Q_EMIT optionsChanged();
        validate_timer_.start();
    }

    void load_settings();
    void save_settings();
    void validate();
    void start(bool dry_run);
    void poll();
    void finish_run(const QVariantMap& stats, bool cancelled);
    // The engine's options from the properties; unreadable .gitignore files go to `unreadable`.
    RenameOptions build_options(bool dry_run, QStringList* unreadable) const;

    QSettings settings_;
    QTimer validate_timer_; // validation (it may start V8) waits for a pause in the typing
    QTimer poll_timer_;     // copies the worker's progress into the properties

    QString target_dir_;
    int case_style_ = 1; // lowercase, the command line's default
    QString regex_;
    bool flatten_ = false;
    QString flatten_regex_;
    QStringList excludes_, includes_, gitignore_files_;
    bool target_gitignore_ = true;
    bool recursive_ = true;
    bool overwrite_ = false;
    QString sample_name_;

    QString target_error_, regex_error_, flatten_regex_error_, sample_result_, sample_error_;
    int regex_error_position_ = -1, flatten_regex_error_position_ = -1;

    bool running_ = false, preview_run_ = true, has_results_ = false, was_cancelled_ = false;
    QString phase_;
    double progress_current_ = 0, progress_total_ = 0;
    QVariantMap stats_;
    ChangeModel changes_;

    std::unique_ptr<GuiSink> sink_;
    std::jthread worker_; // declared last: joined before the sink it reports to goes away
};

} // namespace fsturbo::gui

#endif // GUI_TRANSFORM_CONTROLLER_HPP

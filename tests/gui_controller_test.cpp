// Integration test of the GUI's TransformController (no window): a preview must list the changes
// and touch nothing, an apply must make them, and an invalid regex must block both.
// Run: ctest --test-dir build -R gui_controller
#include <filesystem>
#include <fstream>
#include <print>
#include <QCoreApplication>
#include <QEventLoop>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include "transform_controller.hpp"

namespace fs = std::filesystem;
using fsturbo::gui::TransformController;

namespace {

int failures = 0;

void check(bool ok, std::string_view what) {
    if (!ok) ++failures;
    std::println("{} {}", ok ? "ok  " : "FAIL", what);
}

// Runs one preview/apply and waits for TransformController::finished.
QString run(TransformController& c, bool apply) {
    QString summary;
    QEventLoop loop;
    QObject::connect(&c, &TransformController::finished, &loop, [&](const QString& s) {
        summary = s;
        loop.quit();
    });
    QTimer::singleShot(10000, &loop, &QEventLoop::quit);
    apply ? c.apply() : c.preview();
    loop.exec();
    return summary;
}

void touch(const fs::path& p) {
    fs::create_directories(p.parent_path());
    std::ofstream(p).put('x');
}

} // namespace

int main(int argc, char* argv[]) {
    QStandardPaths::setTestModeEnabled(true); // QSettings go to a test location
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    const fs::path root = dir.path().toStdString();
    touch(root / "Some Dir" / "My File.TXT");
    touch(root / "build" / "main.o");
    touch(root / "Report 2023.PDF");

    TransformController c;
    c.setTargetDir(dir.path());
    c.setCaseStyle(3); // snake_case
    c.setRegex(QString());
    c.setFlatten(false);
    c.setFlattenRegex(QString());
    c.setExcludes({QStringLiteral("build")});
    c.setIncludes({});
    c.setGitignoreFiles({});
    c.setRecursive(true);
    c.setOverwrite(false);
    c.setSampleName(QStringLiteral("Hello World.TXT"));
    QCoreApplication::processEvents();
    QTimer::singleShot(300, &app, &QCoreApplication::quit);
    app.exec(); // let the debounced validation run

    check(c.sampleResult() == "hello_world.txt", "sample name is converted (" + c.sampleResult().toStdString() + ")");

    const QString preview = run(c, false);
    check(c.changes()->count() == 3, std::format("preview lists 3 changes (got {})", c.changes()->count()));
    check(fs::exists(root / "Some Dir" / "My File.TXT"), "preview touches nothing");
    check(c.stats()["excludedItems"].toInt() == 1, "the excluded directory is counted");
    check(preview.contains("3 changes"), "preview summary: " + preview.toStdString());

    const QString applied = run(c, true);
    check(fs::exists(root / "some_dir" / "my_file.txt") && fs::exists(root / "report_2023.pdf"), "apply renames files and directories");
    check(fs::exists(root / "build" / "main.o"), "apply leaves the excluded directory alone");
    check(applied.contains("3 changes made"), "apply summary: " + applied.toStdString());

    c.setRegex(QStringLiteral("/(abc/x/"));
    QTimer::singleShot(300, &app, &QCoreApplication::quit);
    app.exec();
    check(!c.regexError().isEmpty() && !c.canRun(), "an invalid regex blocks the run: " + c.regexError().toStdString());
    check(c.regexErrorPosition() >= 1, std::format("the error has a position ({})", c.regexErrorPosition()));

    std::println("{}", failures == 0 ? "gui_controller: all passed" : "gui_controller: FAILED");
    return failures == 0 ? 0 : 1;
}

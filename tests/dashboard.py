#!/usr/bin/env python3
"""Layout tests for the live mini-console and the final report: the console takes the whole height,
the report (laid out in columns) is shown only when it is at most 20% of the console, and
--no-report gives the console everything. Runs the binary in a pty of a fixed size with the small
VT emulator of vt.py and checks where everything ended up.
Usage: tests/dashboard.py [path/to/fsturbotransform]
"""
import os
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vt  # noqa: E402

BIN = os.path.realpath(sys.argv[1] if len(sys.argv) > 1 else "./fsturbotransform")
passed = failed = 0


def check(name, ok, detail=""):
    global passed, failed
    if ok:
        passed += 1
    else:
        failed += 1
        print(f"FAIL {name}\n  {detail}")


def make_tree(root, collision=False):
    os.makedirs(os.path.join(root, "sub"))
    for i in range(60):
        open(os.path.join(root, f"File_{i:03d}.TXT"), "w").close()
    for i in range(20):
        open(os.path.join(root, "sub", f"Other_{i:02d}.MD"), "w").close()
    if collision:  # renaming X.TXT to x.txt collides with the existing file: one error
        open(os.path.join(root, "x.txt"), "w").close()
        open(os.path.join(root, "X.TXT"), "w").close()


def windows(screen):
    """[(kind, first row, last row)] of the boxed windows on the screen (ANSI modes)."""
    found, lines, i = [], screen.lines(), 0
    while i < len(lines):
        if lines[i].startswith("┌"):
            j = i
            while j < len(lines) and not lines[j].startswith("└"):
                j += 1
            kind = "console" if "MINI-TERMINAL" in lines[i] else "report" if "EXECUTION REPORT" in lines[i] else "?"
            found.append((kind, i, j))
            i = j
        i += 1
    return found


def run_ansi(rows, cols, *flags, collision=False):
    with tempfile.TemporaryDirectory() as root:
        make_tree(root, collision)
        return vt.run([BIN, "--color=terminal", "--dry-run", "--no-gitignore", *flags, root], rows, cols)


def check_full_screen(name, screen, expect_report):
    """The frame fills the screen exactly: progress row, console, [report], one free cursor row."""
    wins = windows(screen)
    kinds = [w[0] for w in wins]
    check(f"{name}: windows", kinds == (["console", "report"] if expect_report else ["console"]), f"got {kinds}\n{screen.dump()}")
    if not wins or wins[0][0] != "console":
        return
    top, bottom = wins[0][1], wins[0][2]
    # The frame is sized for the widest report (with its "Errors" row), so a run without errors can
    # leave one row of what was on screen before (the banner's trailing blank line) above it.
    progress = next((i for i, l in enumerate(screen.lines()) if "▕" in l), None)
    check(f"{name}: the whole frame is visible, nothing scrolled off", progress is not None and progress <= 1 and top == progress + 1, screen.dump())
    last = wins[-1][2]
    check(f"{name}: last window ends on the penultimate row", last == screen.rows - 2, f"ends at {last}\n{screen.dump()}")
    check(f"{name}: cursor on the last row", screen.r == screen.rows - 1, f"cursor row {screen.r}")
    if expect_report and len(wins) == 2:
        console_rows, report_rows = bottom - top + 1, wins[1][2] - wins[1][1] + 1
        check(f"{name}: report directly below the console", wins[1][1] == bottom + 1, screen.dump())
        check(f"{name}: report is at most 20% of the console ({report_rows} of {console_rows})", 5 * report_rows <= console_rows, "")


# --- ANSI ------------------------------------------------------------------------------------

scr, code = run_ansi(24, 100)
check_full_screen("24 rows, default", scr, expect_report=False)
console = windows(scr)[0] if windows(scr) else None
check("24 rows: the console is as tall as the screen allows", console and console[2] - console[1] + 1 == 24 - 2, scr.dump())
check("24 rows: exit status", code == 0, str(code))

scr, code = run_ansi(50, 100)
check_full_screen("50 rows, default", scr, expect_report=True)
wins = windows(scr)
report_rows = wins[1][2] - wins[1][1] + 1 if len(wins) == 2 else 0
check("report uses several columns (fewest rows)", 0 < report_rows <= 5, f"report is {report_rows} rows\n{scr.dump()}")
text = "\n".join(scr.lines())
check("report content", all(s in text for s in ("Scanned Files", "Renamed Dirs", "Execution Time", "Embedded Font", "DRY RUN")), text)

scr, _ = run_ansi(50, 100, "--no-report")
check_full_screen("50 rows, --no-report", scr, expect_report=False)
console = windows(scr)[0] if windows(scr) else None
check("--no-report: the console takes the whole height", console and console[2] - console[1] + 1 == 50 - 2, scr.dump())
check("--no-report: no report", "EXECUTION REPORT" not in "\n".join(scr.lines()), "")

# The report appears exactly when 5 * P <= console window rows, i.e. with at least 6P + 2 rows.
minimum = 6 * report_rows + 2
scr, _ = run_ansi(minimum, 100)
check_full_screen(f"{minimum} rows (the least that fits the report)", scr, expect_report=True)
scr, _ = run_ansi(minimum - 1, 100)
check_full_screen(f"{minimum - 1} rows (one too few)", scr, expect_report=False)

for cols in (60, 80, 140):
    scr, _ = run_ansi(45, cols)
    shown = any(w[0] == "report" for w in windows(scr))
    check_full_screen(f"45x{cols}", scr, expect_report=shown)

scr, code = run_ansi(50, 100, collision=True)
check_full_screen("50 rows with an error", scr, expect_report=True)
check("error row in the report, exit status 1", "Errors" in "\n".join(scr.lines()) and code == 1, f"exit {code}\n{scr.dump()}")

# Output that is not a terminal has no live frame: the report is printed on its own, in columns.
with tempfile.TemporaryDirectory() as root:
    make_tree(root)
    out = subprocess.run([BIN, "--color=terminal", "--dry-run", "--no-gitignore", root], capture_output=True, text=True).stdout
    out = re.sub(r"\x1b\[[0-9;?]*[A-Za-z]|\x1b[78]", "", out)
    lines = out.splitlines()
    top = next((i for i, l in enumerate(lines) if l.startswith("┌─ EXECUTION REPORT")), None)
    bottom = next((i for i, l in enumerate(lines) if l.startswith("└") and top is not None and i > top), None)
    check("piped output: report printed in columns", top is not None and bottom is not None and bottom - top + 1 <= 6, out)
    out = subprocess.run([BIN, "--color=terminal", "--dry-run", "--no-gitignore", "--no-report", root], capture_output=True, text=True).stdout
    check("piped output: --no-report", "EXECUTION REPORT" not in out, out)

# --- Sixel -----------------------------------------------------------------------------------


def run_sixel(rows, cols, *flags, registers=1024):
    with tempfile.TemporaryDirectory() as root:
        make_tree(root)
        return vt.run([BIN, "--color=full", "--dry-run", "--no-gitignore", *flags, root], rows, cols, cell_w=10, cell_h=20, registers=registers)


scr, code = run_sixel(50, 100)
images = scr.images
check("sixel: exit status and images", code == 0 and len(images) >= 3, f"exit {code}, images {images}")
if len(images) >= 3:
    report, frame = images[-1], images[-2]
    frames = [i for i in images[1:-1]]
    check("sixel: every frame is drawn at the same place", all(f["top"] == frame["top"] and f["rows"] == frame["rows"] for f in frames), str(frames))
    check("sixel: the frame starts at the top (one safety row at most)", frame["top"] <= 1, str(frame))
    check("sixel: the report sits right below the frame", report["top"] == frame["top"] + frame["rows"], f"{frame} {report}")
    check("sixel: nothing leaves the screen and the cursor row is free", report["top"] + report["rows"] <= scr.rows - 1 and scr.r <= scr.rows - 1, scr.dump())
    check("sixel: report is at most 20% of the console", 5 * report["rows"] <= frame["rows"] - 1, f"{frame} {report}")
    check("sixel: report uses several columns", report["rows"] <= 6, str(report))

scr, code = run_sixel(50, 100, "--no-report")
images = scr.images
frame = images[-1] if images else None
check("sixel --no-report: the frame fills the screen but one row and a spare", frame and frame["top"] == 0 and frame["rows"] == 50 - 2, str(images))
check("sixel --no-report: no report image", frame and all(i["rows"] == frame["rows"] or i["top"] < 0 for i in images), str(images))

scr, _ = run_sixel(24, 100)
frame = scr.images[-1] if scr.images else None
check("sixel 24 rows: the frame fills the screen but one row and a spare", frame and frame["top"] == 0 and frame["rows"] == 24 - 2, str(scr.images))

# Colour: the program asks the terminal how many Sixel colour registers it has and uses them all.
# The banner (a gradient with a glowing icon) has thousands of distinct colours, so it always uses
# every register; images with fewer colours are sent exactly, with no quantization.
for registers, expected in ((1024, 1024), (256, 256), (None, 256), (64, 64), (4096, 1024)):
    scr, code = run_sixel(50, 100, registers=registers)
    banner = scr.images[0] if scr.images else None
    check(f"sixel colours: terminal reports {registers} registers", code == 0 and banner and banner["colors"] == expected, f"banner {banner}, exit {code}")
    if registers == 1024:
        frames = [i for i in scr.images[1:] if i["rows"] > 5]
        check("sixel colours: every image fits the registers", all(i["colors"] <= 1024 for i in scr.images), str([i["colors"] for i in scr.images]))

# --- Kitty graphics ----------------------------------------------------------------------------


def run_kitty(rows, cols, *flags, collision=False, **terminal):
    with tempfile.TemporaryDirectory() as root:
        make_tree(root, collision)
        return vt.run([BIN, "--color=full", "--graphics=kitty", "--dry-run", "--no-gitignore", *flags, root], rows, cols, kitty=True, **terminal)


scr, code = run_kitty(50, 100, keep_pixels=True)
images = scr.images
check("kitty: exit status and images", code == 0 and len(images) >= 3 and all(i.get("kitty") for i in images), f"exit {code}, {[(i.get('id'), i.get('rows')) for i in images]}")
if len(images) >= 3 and all(i.get("kitty") for i in images):
    check("kitty: every image is RGBA and its decompressed size matches", all(i["format"] == 32 and i["bytes"] == i["expected"] for i in images), str([(i["bytes"], i["expected"]) for i in images]))
    check("kitty: quiet (q=2) and never moves the cursor (C=1)", all(i["quiet"] == "2" and not i["cursor_moves"] for i in images), str([(i["quiet"], i["cursor_moves"]) for i in images]))
    banner, report = images[0], images[-1]
    frames = [i for i in images if i["id"] == 2]
    check("kitty: stable ids (banner 1, frame 2, report 3), one placement each", banner["id"] == 1 and report["id"] == 3 and len(frames) >= 2 and all(i["placement"] == 1 for i in images), str([(i["id"], i["placement"]) for i in images]))
    check("kitty: every frame redraws the same cells", all(f["top"] == frames[0]["top"] and f["rows"] == frames[0]["rows"] and f["cols"] == frames[0]["cols"] for f in frames), str(frames[0]))
    frame = frames[-1]
    check("kitty: no spare row: the frame fills the screen above the report", frame["top"] <= 1 and report["top"] == frame["top"] + frame["rows"] and report["top"] + report["rows"] <= scr.rows - 1,
          f"frame {frame['top']}+{frame['rows']}, report {report['top']}+{report['rows']}, screen {scr.rows}")
    check("kitty: report is at most 20% of the console and in columns", 5 * report["rows"] <= frame["rows"] - 1 and report["rows"] <= 6, f"frame {frame['rows']} rows, report {report['rows']}")
    # Real alpha: the frame is transparent outside its windows and anti-aliased at their edges.
    alphas = set(frame["pixels"][3::4])
    check("kitty: real transparency (alpha 0, 255 and in between)", 0 in alphas and 255 in alphas and any(0 < a < 255 for a in alphas), f"{len(alphas)} distinct alpha values")
    check("kitty: the frame is as big as its cells", frame["width"] == min(scr.cols - 1, 100) * scr.cell_w and frame["height"] == frame["rows"] * scr.cell_h, str((frame["width"], frame["height"])))

scr, _ = run_kitty(50, 100, "--no-report")
frames = [i for i in scr.images if i["id"] == 2]
check("kitty --no-report: the frame fills the screen but one row", frames and frames[-1]["top"] == 0 and frames[-1]["rows"] == 50 - 1 and not any(i["id"] == 3 for i in scr.images), str([(i["id"], i["top"], i["rows"]) for i in scr.images]))

scr, code = run_kitty(50, 100, collision=True)
check("kitty: the frame is deleted before log lines are flushed", scr.kitty_deletes >= 1 and code == 1, f"{scr.kitty_deletes} deletes, exit {code}")

# Choosing the protocol: asked explicitly, or by what the terminal says (Kitty first), or $FSTURBO_GRAPHICS.
def protocol_of(scr):
    return None if not scr.images else "kitty" if scr.images[0].get("kitty") else "sixel"


with tempfile.TemporaryDirectory() as root:
    make_tree(root)
    base = [BIN, "--dry-run", "--no-gitignore", root]
    for label, argv, terminal, env, expected in (
        ("auto: terminal answers Kitty and Sixel -> Kitty", ["--color=auto"], dict(kitty=True, sixel=True), None, "kitty"),
        ("auto: only Sixel -> Sixel", ["--color=auto"], dict(kitty=False, sixel=True), None, "sixel"),
        ("auto: neither -> text", ["--color=auto"], dict(kitty=False, sixel=False), None, None),
        ("--graphics=sixel overrides a Kitty terminal", ["--color=auto", "--graphics=sixel"], dict(kitty=True, sixel=True), None, "sixel"),
        ("--graphics=kitty works in a terminal that answers nothing", ["--color=auto", "--graphics=kitty"], dict(kitty=False, sixel=False, registers=None), None, "kitty"),
        ("$FSTURBO_GRAPHICS=kitty", ["--color=auto"], dict(kitty=False, sixel=False), {"FSTURBO_GRAPHICS": "kitty"}, "kitty"),
        ("--graphics beats $FSTURBO_GRAPHICS", ["--color=auto", "--graphics=sixel"], dict(kitty=False, sixel=False), {"FSTURBO_GRAPHICS": "kitty"}, "sixel"),
        ("--color=full with a Kitty terminal", ["--color=full"], dict(kitty=True, sixel=False), None, "kitty"),
        ("--color=terminal never draws images", ["--color=terminal", "--graphics=kitty"], dict(kitty=True), None, None),
    ):
        scr, code = vt.run([base[0], *argv, *base[1:]], 50, 100, env=env, **terminal)
        check(f"protocol: {label}", protocol_of(scr) == expected and code == 0, f"got {protocol_of(scr)}, exit {code}")

# Detection (--color=auto): what the terminal answers decides whether graphics are used. This is the
# case a Konsole user hits: its answer carries the colour-register reply before DA1, and the DA1 reader
# once tripped over it, so auto mode silently fell back to text.
def run_auto(rows, cols, **terminal):
    with tempfile.TemporaryDirectory() as root:
        make_tree(root)
        return vt.run([BIN, "--color=auto", "--dry-run", "--no-gitignore", root], rows, cols, **terminal)


for label, terminal, graphics in (
    ("Sixel, registers answered (Konsole)", dict(registers=256), True),
    ("Sixel, registers not answered", dict(registers=None), True),
    ("no Sixel in the DA1 answer", dict(sixel=False), False),
):
    scr, code = run_auto(50, 100, **terminal)
    check(f"auto detection: {label}", code == 0 and bool(scr.images) == graphics, f"exit {code}, images {len(scr.images)}\n{scr.dump() if not graphics else ''}")

print(f"{passed} passed, {failed} failed")
sys.exit(1 if failed else 0)

#!/usr/bin/env bash
# Regression tests for the regex engine (PCRE2: lookbehind, UTF-8, flags, filters) and for the -r
# expression replacement mode (named groups, \<name>, \<name=>{expr}, index / nameIndex /
# nameLength / depth). Everything runs with --dry-run.
# Usage: tests/regex.sh [path/to/fsturbotransform]
#   The expression tests need libfsturbo_js.so (libnode-dev); without it they are skipped.
set -u
BIN=$(realpath "${1:-./fsturbotransform}")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
pass=0
fail=0
CWD=.   # where the program is started, relative to the fixture tree

# tree <file>...: recreates the fixture ("dir/" suffix = empty directory)
tree() {
    rm -rf "$work/tree" && mkdir -p "$work/tree"
    local f
    for f in "$@"; do
        if [[ $f == */ ]]; then mkdir -p "$work/tree/$f"; else mkdir -p "$work/tree/$(dirname "$f")" && touch "$work/tree/$f"; fi
    done
}

cli() { (cd "$work/tree/$CWD" && "$BIN" --color=none --dry-run --verbose --no-gitignore "$@" . 2>&1); }
run() { cli -r "$1"; }
actions() { sed -n -e 's/^ *\[RENAME\] \(FILE\|DIR\) RENAME //p' -e 's/^ *\[FLATTEN\] \[FLATTEN\] /flatten /p'; }
report() { # report <description> <options> <expected> <got>
    printf 'FAIL %s\n  options: %s\n  expected:\n%s\n  got:\n%s\n' "$1" "$2" "$(sed 's/^/    /' <<<"$3")" "$(sed 's/^/    /' <<<"$4")"
    fail=$((fail + 1))
}

# with <description> <expected "old → new" / "flatten old → new" lines> <options...>
with() {
    local desc=$1 want got
    want=$(printf '%s\n' "$2" | sort)
    shift 2
    got=$(cli "$@" | actions | sort)
    if [[ $got == "$want" ]]; then pass=$((pass + 1)); else report "$desc" "$*" "$want" "$got"; fi
}

# expect <description> <regex> <expected lines>: with -r <regex>
expect() { with "$1" "$3" -r "$2"; }

# fails <description> <regex> <fragment of the error message>: must exit 1 with that message
fails() {
    local out rc
    out=$(run "$2")
    rc=$?
    if [[ $rc -eq 1 && $out == *"ERROR"* && $out == *"$3"* ]]; then pass=$((pass + 1)); else report "$1" "-r $2" "(exit 1 and an error containing: $3)" "exit $rc: $out"; fi
}

# --- the regex engine (no JavaScript needed) -----------------------------------------------

tree img_007.JPG img_12.png Keep.TXT skip_One.TXT Two.TXT "ñandú ÉCOLE.txt" docs/a.txt docs/sub/b.txt
expect "lookbehind" '/(?<=img_)\d+/N/' \
    "img_007.JPG → img_N.JPG
img_12.png → img_N.png"
expect "negative lookbehind" '/(?<!img_)\d{2}(?=\.)/N/' \
    "img_007.JPG → img_0N.JPG"
expect "bounded variable-length lookbehind" '/(?<=img_0{0,2})\d(?=\.)/N/' \
    "img_007.JPG → img_00N.JPG"
expect "named back-reference \\k<name>, optional" '/(?<a>\d)(\d)\k<a>?/[\<a>|$2]/' \
    "img_007.JPG → img_[0|0]7.JPG
img_12.png → img_[1|2].png"
expect "\\K keeps what precedes it" '/\d+\K\.png/-x.png/' \
    "img_12.png → img_12-x.png"
expect "'.' consumes a whole UTF-8 character" '/^(?<c>.)(?=and)/<\<c>>/' \
    "ñandú ÉCOLE.txt → <ñ>andú ÉCOLE.txt"
with "-e takes a lookbehind regex" "img_007.JPG → img_007.jpg
Keep.TXT → keep.txt
ñandú ÉCOLE.txt → ñandú École.txt
Two.TXT → two.txt" -e '/(?<=skip_)One/' -l --exclude '**/*.png' --exclude 'docs' -e 'docs/**'
with "--flatten-regex takes a lookbehind regex" "flatten docs/a.txt → a.txt
flatten docs/sub/b.txt → b.txt" -f --flatten-regex '/(?<=^docs\/)\w/' --exclude '*' --include '**/*.txt' --include 'docs/**'
tree "Report.TXT" "a b.txt"
expect "no flag: case-sensitive" '/report\.txt$/X/' ""
expect "flag i" '/report\.txt$/X/i' "Report.TXT → X"
expect "flag x ignores blanks and allows comments" '/ r e p o r t   # the name
  \. t x t $/X/xi' "Report.TXT → X"
expect "flags s and m are accepted" '/^report/X/sim' "Report.TXT → X.TXT"
fails "an unbounded lookbehind is refused with PCRE2's own message" '/(?<=a+)b/x/' "lookbehind"
fails "invalid regex shows the offset" '/a(?/x/' "missing closing parenthesis"
fails "duplicate group name" '/(?<a>x)(?<a>y)/z/' "same name"

# --- JavaScript expressions (need the V8 bridge) -------------------------------------------

probe=$(mkdir -p "$work/empty" && cd "$work/empty" && "$BIN" --color=none -d -r '/(?<n=>1)/\<n=>{n}/' . 2>&1)
if [[ $probe == *libfsturbo_js* ]]; then
    echo "SKIP: libfsturbo_js.so is not available (needs libnode-dev); expression tests not run"
    echo "$pass passed, $fail failed"
    [[ $fail -eq 0 ]]
    exit
fi

tree "report 12-05.txt" "Other 30-04 (copy).md"
expect "round(dia+mes*1.5) on the nested named groups" \
    '/(?<fecha=>(?<dia=>\d\d)-(?<mes=>\d\d))/\<fecha=>{round(dia+mes*1.5)}/' \
    "Other 30-04 (copy).md → Other 36 (copy).md
report 12-05.txt → report 20.txt"
expect "a '/' (division) inside the braces is not a field separator; val is the referenced group" \
    '/(?<dia=>\d\d)-(?<mes=>\d\d)/\<dia=>{dia/2}_\<mes=>{val*2}/' \
    "Other 30-04 (copy).md → Other 15_8 (copy).md
report 12-05.txt → report 6_10.txt"
expect "numbers vs strings: dia is a number, \$dia the raw text, fecha stays a string" \
    '/(?<fecha=>(?<dia=>\d\d)-(?<mes=>\d\d))/[\<fecha=>{$dia + "|" + $fecha + "|" + (typeof dia) + (typeof fecha)}]/' \
    "Other 30-04 (copy).md → Other [30|30-04|numberstring] (copy).md
report 12-05.txt → report [12|12-05|numberstring].txt"
expect "\<=var>: the group's value under another name" \
    '/(?<n=>\d\d)-\d\d/\<n=x>{x+1}/' \
    "Other 30-04 (copy).md → Other 31 (copy).md
report 12-05.txt → report 13.txt"

tree a/alpha.txt a/readme.md a/deep/readme.md a/sub/ b/alpha.txt b/readme.md top.txt
expect "index counts files and directories apart, in path order" \
    '/^(?<base>[^.]+)/\<base=>{index}-\<base>/' \
    "a → 1-a
a/deep → 2-deep
a/sub → 3-sub
b → 4-b
a/alpha.txt → 1-alpha.txt
a/deep/readme.md → 2-readme.md
a/readme.md → 3-readme.md
b/alpha.txt → 4-alpha.txt
b/readme.md → 5-readme.md
top.txt → 6-top.txt"
expect "nameIndex among same-named entries" \
    '/^(?<base>readme)/\<base=>{nameIndex}/' \
    "a/deep/readme.md → 1.md
a/readme.md → 2.md
b/readme.md → 3.md"
expect "nameLength is the length of the whole name; depth is 0 directly inside the working directory" \
    '/^(?<base>[^.]+)/\<base>@\<base=>{depth}d\<base=>{nameLength}/' \
    "a → a@0d1
a/deep → deep@1d4
a/sub → sub@1d3
b → b@0d1
a/alpha.txt → alpha@1d9.txt
a/deep/readme.md → readme@2d9.md
a/readme.md → readme@1d9.md
b/alpha.txt → alpha@1d9.txt
b/readme.md → readme@1d9.md
top.txt → top@0d7.txt"
CWD=a expect "depth is measured from the directory the program was started in" \
    '/^(?<b>[^.]+)/\<b=>{depth}_\<b>/' \
    "deep → 0_deep
sub → 0_sub
alpha.txt → 0_alpha.txt
deep/readme.md → 1_readme.md
readme.md → 0_readme.md"

tree photo_7.jpg note_42.txt plain.txt
expect "string methods" \
    '/(?<w>[a-z]+)_(?<n=>\d+)/\<w=>{w.toUpperCase()}-\<n=>{String(n).padStart(4,"0")}/' \
    "note_42.txt → NOTE-0042.txt
photo_7.jpg → PHOTO-0007.jpg"
expect "named references, case modifiers and expressions mix" \
    '/(?<w>[a-z]+)_(?<n=>\d+)/\<w>_\U\<w>\E_\<n=>{n*n}/' \
    "note_42.txt → note_NOTE_1764.txt
photo_7.jpg → photo_PHOTO_49.jpg"
expect "a zero-width group inserts" \
    '/^(?<p=>)/\<p=>{index}_/' \
    "note_42.txt → 1_note_42.txt
photo_7.jpg → 2_photo_7.jpg
plain.txt → 3_plain.txt"
expect "all of Math, template literals" \
    '/(?<n=>\d+)/\<n=>{`${n}x${hypot(3,4)}-${max(n,10)}-${PI.toFixed(2)}-${Math.floor(n/4)}`}/' \
    "note_42.txt → note_42x5-42-3.14-10.txt
photo_7.jpg → photo_7x5-10-3.14-1.jpg"
tree n2.5.txt n-3.txt n1e2.txt n1e999.txt n.5.txt
expect "number syntax: decimals, signs, exponents; out of range stays text" \
    '/^n(?<a=>[-+.\d][\d.e+-]*?)(?<b>\.txt)$/\<a=>{typeof a + ":" + a * 2}\<b>/' \
    "n2.5.txt → number:5.txt
n-3.txt → number:-6.txt
n1e2.txt → number:200.txt
n1e999.txt → string:Infinity.txt
n.5.txt → number:1.txt"
tree photo_7.jpg note_42.txt plain.txt
expect "a group that did not take part is undefined" \
    '/^(?<l>[a-z]+)(?<n=>_\d+)?/\<l=>{l + (n === undefined ? "!" : "")}/' \
    "note_42.txt → note.txt
photo_7.jpg → photo.jpg
plain.txt → plain!.txt"
expect "the classic \$N / \\U replacement still works" \
    '/([a-z]+)_(\d+)/$2-\U$1/i' \
    "note_42.txt → 42-NOTE.txt
photo_7.jpg → 7-PHOTO.jpg"

fails "syntax errors are reported before anything is touched" '/(?<n=>\d+)/\<n=>{n +* 2}/' "SyntaxError"
fails "unknown variable is a per-file error" '/(?<n=>\d+)/\<n=>{nn + 1}/' "nn is not defined"
fails "NaN is refused" '/(?<n=>\d+)/\<n=>{sqrt(-n)}/' "NaN"
fails "undefined is refused" '/(?<n=>\d+)/\<n=>{undefined}/' "must be a number, string or boolean"
fails "unknown group" '/(?<n=>\d+)/\<zzz=>{1}/' "'zzz'"
fails "missing brace block" '/(?<n=>\d+)/\<n=>1/' "expected '{'"
fails "unclosed brace block" '/(?<n=>\d+)/\<n=>{1+/' "never closed"
fails "empty expression" '/(?<n=>\d+)/\<n=>{ }/' "empty expression"
fails "group named like a built-in" '/(?<index>\d+)/\<index=>{1}/' "clashes"
fails "invalid regex" '/(\d+/z/' "invalid regular expression"
fails "a computed name with a slash is refused" '/^(?<l>[a-z])/\<l=>{String.fromCharCode(47)}/' "not a valid file name"

echo "$pass passed, $fail failed"
[[ $fail -eq 0 ]]

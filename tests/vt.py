#!/usr/bin/env python3
"""Runs a command in a pty of a given size and emulates just enough of a VT terminal to see where
everything ends up on screen: printable text, CR/LF with scrolling, cursor movement and erase,
DECSC/DECRC, and Sixel images (only their position and height are tracked). It also answers the
queries the program's terminal probe sends (background colour, cell size, DA1 with Sixel).

Library use:  screen, status = vt.run(argv, rows, cols, cell_w=10, cell_h=20, registers=1024)
              registers is the Sixel colour register count it reports (None: it does not answer)
Command line: tests/vt.py ROWS COLS COMMAND...   (prints the final screen)
"""
import codecs
import fcntl
import math
import os
import re
import select
import struct
import sys
import termios
import time
import unicodedata


class Screen:
    def __init__(self, rows, cols, cell_w, cell_h, registers=1024):
        self.rows, self.cols, self.cell_w, self.cell_h = rows, cols, cell_w, cell_h
        self.registers = registers
        self.grid = [[" "] * cols for _ in range(rows)]
        self.r = self.c = 0
        self.saved = (0, 0)
        self.wrap_pending = False
        self.scrolled = 0
        self.images = []  # {"top", "left", "rows", "height", "colors"}: top is relative to the screen, may go negative
        self.replies = b""
        self._state = "n"
        self._buf = ""
        self._dcs_parts = []
        self._decoder = codecs.getincrementaldecoder("utf-8")("replace")

    # --- output -------------------------------------------------------------------------
    def _scroll(self):
        self.grid.pop(0)
        self.grid.append([" "] * self.cols)
        self.scrolled += 1
        for image in self.images:
            image["top"] -= 1

    def _line_feed(self):
        if self.r == self.rows - 1:
            self._scroll()
        else:
            self.r += 1

    def _put(self, ch):
        width = 2 if unicodedata.east_asian_width(ch) in "WF" else 1
        if self.wrap_pending or self.c + width > self.cols:
            self.c = 0
            self._line_feed()
            self.wrap_pending = False
        self.grid[self.r][self.c] = ch
        if width == 2:
            self.grid[self.r][self.c + 1] = ""
        self.c += width
        if self.c >= self.cols:
            self.c = self.cols - 1
            self.wrap_pending = True

    def feed(self, data):
        text = self._decoder.decode(data)
        i, n = 0, len(text)
        while i < n:
            ch = text[i]
            i += 1
            state = self._state
            if state == "n":
                if ch == "\x1b":
                    self._state = "e"
                elif ch == "\r":
                    self.c, self.wrap_pending = 0, False
                elif ch == "\n":
                    self._line_feed()
                elif ch == "\b":
                    self.c = max(0, self.c - 1)
                elif ch >= " " and ch != "\x7f":
                    self._put(ch)
            elif state == "e":
                self._state, self._buf = "n", ""
                if ch == "[":
                    self._state = "c"
                elif ch == "]":
                    self._state = "o"
                elif ch == "P":
                    self._state, self._dcs_parts = "d", []
                elif ch == "7":
                    self.saved = (self.r, self.c)
                elif ch == "8":
                    self.r, self.c = self.saved
                    self.wrap_pending = False
            elif state == "c":
                if "@" <= ch <= "~":
                    self._csi(self._buf, ch)
                    self._state, self._buf = "n", ""
                else:
                    self._buf += ch
            elif state == "o":
                if ch == "\x07":
                    self._osc(self._buf)
                    self._state, self._buf = "n", ""
                elif ch == "\x1b":
                    self._state = "oe"
                else:
                    self._buf += ch
            elif state == "oe":
                self._osc(self._buf)
                self._state, self._buf = "n", ""
            elif state == "d":  # the DCS payload (a Sixel image) runs to ESC \
                end = text.find("\x1b", i - 1)
                if end < 0:
                    self._dcs_parts.append(text[i - 1:])
                    i = n
                else:
                    self._dcs_parts.append(text[i - 1:end])
                    i = end + 1
                    self._state = "de"
            elif state == "de":
                self._dcs("".join(self._dcs_parts))
                self._dcs_parts = []
                self._state = "n"

    # --- sequences ----------------------------------------------------------------------
    def _csi(self, params, final):
        private = params.startswith("?")
        nums = [int(p) if p.isdigit() else None for p in params.lstrip("?>").split(";")]
        first = nums[0] if nums and nums[0] is not None else None
        n = first if first else 1
        if final == "A":
            self.r = max(0, self.r - n)
        elif final == "B":
            self.r = min(self.rows - 1, self.r + n)
        elif final == "C":
            self.c = min(self.cols - 1, self.c + n)
        elif final == "D":
            self.c = max(0, self.c - n)
        elif final == "G":
            self.c = min(self.cols - 1, n - 1)
        elif final in "Hf":
            self.r = min(self.rows - 1, (first or 1) - 1)
            self.c = min(self.cols - 1, ((nums[1] if len(nums) > 1 and nums[1] else 1)) - 1)
        elif final == "J":
            mode = first or 0
            if mode == 0:
                for col in range(self.c, self.cols):
                    self.grid[self.r][col] = " "
                for row in range(self.r + 1, self.rows):
                    self.grid[row] = [" "] * self.cols
            elif mode == 2:
                self.grid = [[" "] * self.cols for _ in range(self.rows)]
        elif final == "K":
            mode = first or 0
            cols = range(self.c, self.cols) if mode == 0 else range(0, self.cols) if mode == 2 else range(0, self.c + 1)
            for col in cols:
                self.grid[self.r][col] = " "
        elif final == "t" and first == 16:
            self.replies += f"\x1b[6;{self.cell_h};{self.cell_w}t".encode()
        elif final == "S" and private and nums[:3] == [1, 1, 0] and self.registers:
            self.replies += f"\x1b[?1;0;{self.registers}S".encode()
        elif final == "c" and not private and (first is None or first == 0):
            self.replies += b"\x1b[?62;4;22c"  # a Sixel-capable terminal

    def _osc(self, text):
        if text == "11;?":
            self.replies += b"\x1b]11;rgb:0e0e/0e0e/1a1a\x1b\\"

    def _dcs(self, text):
        m = re.match(r'^[\d;]*q"(\d+);(\d+);(\d+);(\d+)', text)
        if m:
            height = int(m.group(4))
            self.images.append({"top": self.r, "left": self.c, "rows": math.ceil(height / self.cell_h), "height": height, "colors": len(re.findall(r"#\d+;2;", text))})

    # --- inspection ---------------------------------------------------------------------
    def lines(self):
        return ["".join(row).rstrip() for row in self.grid]

    def dump(self):
        out = [f"{i:3d}│{line}" for i, line in enumerate(self.lines())]
        out.append(f"cursor row {self.r}, scrolled {self.scrolled}, images {[(i['top'], i['rows']) for i in self.images]}")
        return "\n".join(out)


def run(argv, rows, cols, cell_w=10, cell_h=20, cwd=None, timeout=30, registers=1024):
    """Returns (Screen, exit status)."""
    screen = Screen(rows, cols, cell_w, cell_h, registers)
    master, slave = os.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, cols * cell_w, rows * cell_h))
    pid = os.fork()
    if pid == 0:
        os.setsid()
        fcntl.ioctl(slave, termios.TIOCSCTTY, 0)
        for fd in (0, 1, 2):
            os.dup2(slave, fd)
        os.close(master)
        if cwd:
            os.chdir(cwd)
        os.environ["TERM"] = "xterm-256color"
        try:
            os.execvp(argv[0], argv)
        finally:
            os._exit(127)
    os.close(slave)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        ready, _, _ = select.select([master], [], [], 0.2)
        if ready:
            try:
                data = os.read(master, 65536)
            except OSError:
                break
            if not data:
                break
            screen.feed(data)
            if screen.replies:
                os.write(master, screen.replies)
                screen.replies = b""
    _, status = os.waitpid(pid, 0)
    os.close(master)
    return screen, os.waitstatus_to_exitcode(status)


if __name__ == "__main__":
    scr, code = run(sys.argv[3:], int(sys.argv[1]), int(sys.argv[2]))
    print(scr.dump())
    print("exit", code)

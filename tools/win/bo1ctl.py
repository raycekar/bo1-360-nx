"""Drive the game's virtual test controllers over \\\\.\\pipe\\bo1ctl (input_script_pads > 0).

CLI:
  py -I bo1ctl.py [--pipe bo1ctl] ping
  py -I bo1ctl.py press <pad> <BUTTON>[+BUTTON] [ms]      tap buttons (default 120 ms)
  py -I bo1ctl.py stick <pad> <L|R> <x> <y> <ms>          hold a stick (-1..1) for ms
  py -I bo1ctl.py trig <pad> <L|R> <ms>                   hold a trigger for ms
  py -I bo1ctl.py raw <command line...>                   send any line, print the answer
  py -I bo1ctl.py run <script.txt>                        run a script of the lines above, one per line,
                                                          plus "wait <ms>" and "# comments"
Pads are numbered 0-3 (player 1 = 0). "all" means every pad.
"""
import sys
import time

BUTTONS = {
    "UP": 0x0001, "DOWN": 0x0002, "LEFT": 0x0004, "RIGHT": 0x0008,
    "START": 0x0010, "BACK": 0x0020, "LS": 0x0040, "RS": 0x0080,
    "LB": 0x0100, "RB": 0x0200, "A": 0x1000, "B": 0x2000, "X": 0x4000, "Y": 0x8000,
}


class Ctl:
    def __init__(self, pipe="bo1ctl", pads=4, timeout=60.0):
        path = r"\\.\pipe" + "\\" + pipe
        deadline = time.time() + timeout
        while True:
            try:
                self.f = open(path, "r+b", buffering=0)
                break
            except OSError:
                if time.time() > deadline:
                    raise
                time.sleep(0.5)
        self.pads = pads
        self.state = [[0, 0, 0, 0, 0, 0, 0] for _ in range(4)]  # buttons lt rt lx ly rx ry

    def cmd(self, line):
        self.f.write((line + "\n").encode())
        out = b""
        while not out.endswith(b"\n"):
            c = self.f.read(1)
            if not c:
                raise ConnectionError("pipe closed (game exited?)")
            out += c
        return out.decode().strip()

    def _send(self, pad):
        s = self.state[pad]
        return self.cmd("pad %d %x %d %d %d %d %d %d" % (pad, *s))

    def _pads(self, pad):
        return range(self.pads) if pad == "all" else [int(pad)]

    def set(self, pad, buttons=None, lt=None, rt=None, l=None, r=None):
        for p in self._pads(pad):
            s = self.state[p]
            if buttons is not None:
                s[0] = buttons
            if lt is not None:
                s[1] = lt
            if rt is not None:
                s[2] = rt
            if l is not None:
                s[3], s[4] = int(l[0] * 32767), int(l[1] * 32767)
            if r is not None:
                s[5], s[6] = int(r[0] * 32767), int(r[1] * 32767)
            self._send(p)

    def press(self, pad, names, ms=120):
        mask = 0
        for n in names.upper().split("+"):
            mask |= BUTTONS[n]
        self.set(pad, buttons=mask)
        time.sleep(ms / 1000)
        self.set(pad, buttons=0)
        time.sleep(0.08)

    def stick(self, pad, which, x, y, ms):
        self.set(pad, **({"l": (x, y)} if which.upper() == "L" else {"r": (x, y)}))
        time.sleep(ms / 1000)
        self.set(pad, **({"l": (0, 0)} if which.upper() == "L" else {"r": (0, 0)}))

    def trig(self, pad, which, ms):
        self.set(pad, **({"lt": 255} if which.upper() == "L" else {"rt": 255}))
        time.sleep(ms / 1000)
        self.set(pad, **({"lt": 0} if which.upper() == "L" else {"rt": 0}))

    def line(self, text):
        a = text.split()
        if not a or a[0].startswith("#"):
            return None
        op = a[0].lower()
        if op == "wait":
            time.sleep(int(a[1]) / 1000)
        elif op == "press":
            self.press(a[1], a[2], int(a[3]) if len(a) > 3 else 120)
        elif op == "stick":
            self.stick(a[1], a[2], float(a[3]), float(a[4]), int(a[5]))
        elif op == "trig":
            self.trig(a[1], a[2], int(a[3]))
        elif op == "raw":
            return self.cmd(" ".join(a[1:]))
        else:
            return self.cmd(text.strip())
        return "ok"


def main(argv):
    pipe = "bo1ctl"
    if len(argv) > 2 and argv[0] == "--pipe":
        pipe, argv = argv[1], argv[2:]
    if not argv:
        print(__doc__)
        return 2
    c = Ctl(pipe)
    if argv[0] == "run":
        with open(argv[1], encoding="utf-8") as f:
            for ln in f:
                r = c.line(ln)
                if r and r != "ok":
                    print(ln.strip(), "->", r)
    else:
        print(c.line(" ".join(argv)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

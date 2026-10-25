# Executa phileas en un pseudoterminal, envia tecles i imprimeix la pantalla final.
#
#   tic -o tools/terminfo tools/simple.ti
#   TERMINFO=tools/terminfo python3 -I tools/screen.py ./phileas fitxer.md 'text' '\x13'
#
# Amb el terminfo «simple» (només cup, el i clear) la sortida de ncurses és fàcil
# d'interpretar. Les línies amb «~» marquen els caràcters en vermell (errors
# d'ortografia). TERMX canvia el TERM (p. ex. TERMX=linux, sense TERMINFO).
import os, pty, sys, time, select, struct, fcntl, termios, re
S = os.path.dirname(os.path.abspath(__file__))
TI = os.environ.get('TERMINFO', S + '/terminfo')
ROWS, COLS = 24, 80
prog, args, keys = sys.argv[1], sys.argv[2].split(), sys.argv[3:]
pid, fd = pty.fork()
if pid == 0:
    os.environ['TERM'] = os.environ.get('TERMX', 'simple')
    os.environ['TERMINFO'] = TI
    os.execv(prog, [prog, '-a', '0'] + args)
fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack('HHHH', ROWS, COLS, 0, 0))
scr = [[' '] * COLS for _ in range(ROWS)]
red = [[0] * COLS for _ in range(ROWS)]
cur_red = 0
y = x = 0
buf = ''
def feed(s):
    global y, x, buf, cur_red
    s = buf + s; buf = ''
    i = 0
    while i < len(s):
        c = s[i]
        if c == '\x1b':
            m = re.match(r'\x1b\[([0-9;?]*)([A-Za-z])', s[i:])
            if not m:
                if len(s) - i < 12: buf = s[i:]; return
                i += 1; continue
            p, f = m.group(1), m.group(2)
            if f == 'H':
                a = [int(v) for v in p.split(';')] if p else [1, 1]
                y, x = a[0] - 1, a[1] - 1
            elif f == 'm':
                for v in (p or '0').split(';'):
                    if v in ('0', '39'): cur_red = 0
                    elif v == '31': cur_red = 1
                    elif v.startswith('3') and len(v) == 2: cur_red = 0
            elif f == 'J':
                for r in range(y, ROWS): scr[r] = [' '] * COLS
            elif f == 'K':
                for cc in range(x, COLS): scr[y][cc] = ' '
            i += m.end(); continue
        if c == '\r': x = 0
        elif c == '\n': y = min(y + 1, ROWS - 1)
        elif c == '\b': x = max(x - 1, 0)
        elif c >= ' ':
            if x >= COLS: x = 0; y = min(y + 1, ROWS - 1)
            scr[y][x] = c; red[y][x] = cur_red; x += 1
        i += 1
def rd(t):
    end = time.time() + t
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.05)
        if r:
            try: feed(os.read(fd, 65536).decode('utf-8', 'replace'))
            except OSError: return
rd(0.5)
for k in keys:
    os.write(fd, k.encode().decode('unicode_escape').encode('latin1')); rd(0.25)
for r in range(ROWS):
    print("".join(scr[r]).rstrip())
    if any(red[r]): print("".join("~" if v else " " for v in red[r]).rstrip())
print("CURSOR fila %d" % y)
os.kill(pid, 9); os.waitpid(pid, 0)

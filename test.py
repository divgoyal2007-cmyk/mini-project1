#!/usr/bin/env python3
"""
C-Shell test suite - Parts A through E.

Drives the shell through a pseudo-terminal, which is the only way to test
Ctrl-C / Ctrl-Z / Ctrl-D and terminal ownership.  A piped harness cannot do it:
with stdin as a pipe there is no controlling terminal, so the driver keys never
become signals and every Part E test would silently pass or silently fail.

    python3 test_shell.py [path/to/shell.out]

Default binary path is ./shell.out relative to this file.
"""
import os, sys, pty, re, time, select, subprocess, shutil, signal

SHELL = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else './shell.out')
SANDBOX = os.path.abspath('./_suite_sandbox')

PROMPT = re.compile(r'<[^<>\n]*@[^<>\n]*>')
results = []          # (part, name, passed, detail)


# ───────────────────────── harness ─────────────────────────
def clean(text, echo=None):
    text = text.replace('\r\n', '\n').replace('\r', '')
    text = PROMPT.sub('', text)
    if echo:
        lines = text.split('\n')
        for i, l in enumerate(lines):
            if l.strip() == echo.strip():
                lines.pop(i)
                break
        text = '\n'.join(lines)
    return text


class Sh:
    """One shell process attached to its own pty."""
    def __init__(self, cwd=SANDBOX):
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.chdir(cwd)
            os.execv(SHELL, [SHELL])
        self.alive = True
        self.startup = self.drain(0.35)

    def drain(self, t):
        end, out = time.time() + t, ''
        while time.time() < end:
            r, _, _ = select.select([self.fd], [], [], 0.05)
            if r:
                try:
                    d = os.read(self.fd, 65536)
                except OSError:
                    self.alive = False
                    break
                if not d:
                    self.alive = False
                    break
                out += d.decode('utf-8', 'replace')
        return out

    def raw(self, data, settle=0.35):
        try:
            os.write(self.fd, data)
        except OSError:
            self.alive = False
            return ''
        return self.drain(settle)

    def cmd(self, line, settle=0.45):
        return clean(self.raw((line + '\n').encode(), settle), line)

    def running(self):
        try:
            pid, _ = os.waitpid(self.pid, os.WNOHANG)
            return pid == 0
        except ChildProcessError:
            return False

    def exited(self, wait=1.5):
        """True once the shell has really terminated. Polls, because drain()
        returns the instant the pty hits EOF - which is fractionally before the
        process becomes reapable."""
        end = time.time() + wait
        while time.time() < end:
            if not self.running():
                return True
            time.sleep(0.05)
        return False

    def wait_for(self, needle, timeout=8.0):
        """Drain until `needle` appears; returns (text, seconds) or (text, None)."""
        start, acc = time.time(), ''
        while time.time() - start < timeout:
            acc += self.drain(0.1)
            if needle in clean(acc):
                return clean(acc), time.time() - start
        return clean(acc), None

    def close(self):
        try:
            os.kill(self.pid, signal.SIGKILL)
        except OSError:
            pass
        end = time.time() + 2.0
        while time.time() < end:
            try:
                if os.waitpid(self.pid, os.WNOHANG)[0] != 0:
                    break
            except ChildProcessError:
                break
            time.sleep(0.02)
        try:
            os.close(self.fd)
        except OSError:
            pass


def one(cmds, settle=0.45, tail=0.5):
    """Run a list of command strings in a fresh shell, return combined output."""
    sh = Sh()
    out = ''
    for c in cmds:
        out += sh.cmd(c, settle)
    out += clean(sh.drain(tail))
    sh.close()
    return out


VERBOSE = True

def check(part, name, ok, detail=''):
    ok = bool(ok)
    results.append((part, name, ok, detail))
    if VERBOSE:
        print(f'  {"pass" if ok else "FAIL"}  {name}', flush=True)
        if not ok and detail:
            print(f'        {detail}', flush=True)


def expect(part, name, cmds, needle, mode='in', settle=0.45, tail=0.5):
    out = one(cmds if isinstance(cmds, list) else [cmds], settle, tail)
    if mode == 'in':
        ok = needle in out
    elif mode == 'out':
        ok = needle not in out
    elif mode == 're':
        ok = re.search(needle, out) is not None
    else:
        raise ValueError(mode)
    check(part, name, ok, '' if ok else f'want[{mode}] {needle!r} | got {out.strip()[:160]!r}')


# ───────────────────────── fixtures ─────────────────────────
def build_sandbox():
    shutil.rmtree(SANDBOX, ignore_errors=True)
    os.makedirs(SANDBOX)
    w = lambda p, s: open(os.path.join(SANDBOX, p), 'w').write(s)

    w('multiline.txt', 'line one\nline two\nline three\n')
    w('readme.txt', '# OSN Shell Project\n\nPart A B C\n')
    w('blank_lines.txt', 'Line 1\n\nLine 3\n\n\nLine 6\n')
    w('nonewline.txt', 'single line no newline')
    w('empty.txt', '')
    w('alpha.txt', 'aaa\n')
    w('beta.txt', 'bbb\n')
    w('large_log.txt', ''.join(f'Log entry number {i} padding\n' for i in range(1, 1001)))
    w('.top_hidden.txt', 'hidden\n')

    for d in ('nest1', 'nest1/nest2', 'nest1/nest2/nest3', 'deep', 'deep/target_dir'):
        os.makedirs(os.path.join(SANDBOX, d), exist_ok=True)
    for f in ('nest1/f1.txt', 'nest1/nest2/f2.txt', 'nest1/nest2/nest3/f3.txt',
              'nest1/.hidden_in_nest.txt'):
        open(os.path.join(SANDBOX, f), 'w').write('x\n')

    w('local_script.sh', '#!/bin/sh\necho "local script executed"\n')
    os.chmod(os.path.join(SANDBOX, 'local_script.sh'), 0o755)
    w('non_exec.sh', '#!/bin/sh\necho nope\n')
    os.chmod(os.path.join(SANDBOX, 'non_exec.sh'), 0o644)


def sandbox_file(p):
    return os.path.join(SANDBOX, p)


def read(p):
    try:
        return open(sandbox_file(p)).read()
    except OSError:
        return None


# ════════════════════════ PART A ════════════════════════
def part_A():
    P = 'A  lexer/grammar'
    expect(P, 'shell starts and runs a command', ['echo ok'], 'ok')
    sh = Sh()
    m = re.search(r'<([^@<>]+)@([^:<>]+):([^<>]*)>', sh.startup)
    check(P, 'prompt is <user@host:path>', m is not None, f'startup = {sh.startup!r}')
    check(P, 'prompt contracts the home directory to ~',
          m is not None and m.group(3).startswith('~'),
          f'path shown = {m.group(3)!r}' if m else 'no prompt matched')
    sh.cmd('hop nest1')
    m2 = re.search(r'<[^@<>]+@[^:<>]+:([^<>]*)>', sh.raw(b'\n', 0.4))
    check(P, 'prompt tracks the working directory',
          m2 is not None and m2.group(1).endswith('nest1'),
          f'path shown = {m2.group(1)!r}' if m2 else 'no prompt matched')
    sh.close()

    expect(P, 'quoted pipe stays literal',        'echo "hello | sort"', 'hello | sort')
    expect(P, 'quoted redirection stays literal', 'echo "a > b"', 'a > b')
    expect(P, 'quoted semicolon stays literal',   'echo "a ; b"', 'a ; b')
    expect(P, 'quoted ampersand stays literal',   'echo "a & b"', 'a & b')
    expect(P, 'escaped pipe is a word',           r'echo a \| b', 'a | b')
    expect(P, 'escaped space joins arguments',    r'echo first\ second third', 'first second third')
    expect(P, 'single quotes suppress escapes',   r"echo 'a \b c'", r'a \b c')
    expect(P, 'double quotes unescape \\"',       r'echo "say \"hi\""', 'say "hi"')
    expect(P, 'empty quoted arg is passed',       'echo a "" b', 'a  b')

    expect(P, 'unclosed double quote',   'echo "unclosed',       'cshell: invalid syntax')
    expect(P, 'unclosed single quote',   "echo 'unclosed",       'cshell: invalid syntax')
    expect(P, 'trailing backslash',      'echo trailing\\',      'cshell: invalid syntax')
    expect(P, 'leading pipe',            '| grep a',             'cshell: invalid syntax')
    expect(P, 'leading redirection',     '> out.txt',            'cshell: invalid syntax')
    expect(P, 'trailing semicolon',      'echo hello ;',         'cshell: invalid syntax')
    expect(P, 'redirection without target', 'cat <',             'cshell: invalid syntax')
    expect(P, 'pipe into redirection',   'echo hi | > out.txt',  'cshell: invalid syntax')
    expect(P, 'double ampersand',        'echo hi & &',          'cshell: invalid syntax')
    expect(P, 'pipe then semicolon',     'cat multiline.txt | ; cat', 'cshell: invalid syntax')
    expect(P, 'blank line is not an error', '   ', 'cshell:', 'out')

    # A rejected line must have no side effects at all.
    one(['> side_effect.txt'])
    check(P, 'rejected line creates no file',
          not os.path.exists(sandbox_file('side_effect.txt')),
          'side_effect.txt was created despite the syntax error')


# ════════════════════════ PART B1 ════════════════════════
def part_B1():
    P = 'B1 hop'
    sh = Sh()
    sh.cmd('hop nest1')
    out = sh.raw(b'\n', 0.4)
    check(P, 'hop into subdirectory changes prompt', 'nest1' in out,
          f'prompt did not show nest1: {clean(out)!r}')
    sh.cmd('hop ..')
    out = sh.raw(b'\n', 0.4)
    check(P, 'hop .. returns to parent', 'nest1' not in out, clean(out)[:120])
    sh.cmd('hop nest1')
    sh.cmd('hop -')
    out = sh.raw(b'\n', 0.4)
    check(P, 'hop - returns to previous', 'nest1' not in out, clean(out)[:120])
    sh.close()

    expect(P, 'hop nonexistent target', 'hop definitely_not_here_12345', 'hop: no such directory')
    expect(P, 'hop . is a silent no-op', 'hop .', 'cshell:', 'out')
    expect(P, 'hop ~ is silent', 'hop ~', 'no such directory', 'out')

    # Frecency: visit deep/target_dir, leave, then hop by bare substring.
    sh = Sh()
    sh.cmd('hop deep/target_dir')
    sh.cmd('hop ~')
    sh.cmd('hop target_dir')
    out = sh.raw(b'\n', 0.4)
    check(P, 'frecency jump by substring', 'target_dir' in out,
          f'did not land in target_dir: {clean(out)!r}')
    sh.close()

    sh = Sh()
    sh.cmd('hop nest1 nest2')
    out = sh.raw(b'\n', 0.4)
    check(P, 'multiple args hop sequentially', 'nest2' in out, clean(out)[:120])
    sh.close()


# ════════════════════════ PART B2 ════════════════════════
def part_B2():
    P = 'B2 reveal'
    expect(P, 'default listing',            'reveal', 'multiline.txt')
    expect(P, 'default hides dotfiles',     'reveal', '.top_hidden.txt', 'out')
    expect(P, '-a shows hidden',            'reveal -a', '.top_hidden.txt')
    expect(P, '-t recurses',                'reveal -t', 'nest1/nest2/f2.txt')
    expect(P, '-t marks directories with /','reveal -t', 'nest1/')
    expect(P, '-t omits . and ..',          'reveal -t', 'nest1/.\n', 'out')
    expect(P, '-ta combines',               'reveal -ta', '.hidden_in_nest.txt')
    expect(P, 'repeated flags idempotent',  'reveal -ta -aaaa -tttt', '.hidden_in_nest.txt')
    expect(P, 'invalid flag',               'reveal -z', 'reveal: invalid syntax')
    expect(P, 'two path arguments',         'reveal nest1 nest1/nest2', 'reveal: invalid syntax')
    expect(P, 'file target is an error',    'reveal multiline.txt', 'reveal: no such directory')
    expect(P, 'missing target is an error', 'reveal no_such_dir_xyz', 'reveal: no such directory')
    expect(P, 'reveal ~ works',             'reveal ~', 'multiline.txt')

    out = one(['reveal'])
    names = [l for l in out.split('\n') if l.strip()]
    check(P, 'entries are sorted lexicographically', names == sorted(names),
          f'unsorted: {names[:8]}')


# ════════════════════════ PART B3 ════════════════════════
def part_B3():
    P = 'B3 peek'
    expect(P, 'plain file output',          'peek multiline.txt', 'line one')
    expect(P, '-n numbers only non-blank',  'peek -n blank_lines.txt', '3 Line 6')
    expect(P, '-n leaves blank lines unnumbered', 'peek -n blank_lines.txt', '2 Line 3')
    expect(P, '-r reverses',                'peek -r multiline.txt', 'line three\nline two\nline one')
    expect(P, '-rn numbers count down',     'peek -rn multiline.txt', '1 line one')
    expect(P, '-r keeps blank lines',       'peek -r blank_lines.txt', 'Line 6\n\n\nLine 3')
    expect(P, '-r on a 1000-line file',     'peek -r large_log.txt', 'Log entry number 1000')
    expect(P, '-r reaches the first line',  'peek -r large_log.txt', 'Log entry number 1 padding')
    expect(P, 'file without trailing newline', 'peek nonewline.txt', 'single line no newline')
    expect(P, 'empty file is silent',       'peek empty.txt', 'peek:', 'out')
    expect(P, 'directory argument',         'peek nest1', 'peek: is a directory')
    expect(P, 'missing file argument',      'peek no_such_file.xyz', 'peek: no such file or directory')
    expect(P, 'invalid flag',               'peek -q multiline.txt', 'peek: invalid syntax')
    expect(P, 'numbering continues across files',
           'peek -n alpha.txt beta.txt', '2 bbb')
    expect(P, 'reads stdin through a pipe', 'echo piped | peek', 'piped')


# ════════════════════════ PART B4 ════════════════════════
def part_B4():
    P = 'B4 locate'
    expect(P, 'executable in cwd',        'locate local_script.sh', 'local_script.sh')
    expect(P, 'non-executable ignored',   'locate non_exec.sh', 'locate: command not found (non_exec.sh)')
    expect(P, 'binary on PATH',           'locate ls', '/bin/ls')
    expect(P, 'no arguments',             'locate', 'locate: invalid syntax')
    expect(P, 'unknown name',             'locate definitely_not_a_binary', 'locate: command not found (definitely_not_a_binary)')
    expect(P, 'multiple targets',         'locate ls cat', '/bin/cat')
    expect(P, 'directory is not executable', 'locate nest1', 'locate: command not found (nest1)')


# ════════════════════════ PART C ════════════════════════
def part_C():
    P = 'C1 resolution'
    expect(P, 'literal relative path',      './local_script.sh', 'local script executed')
    expect(P, 'bare name found in cwd',     'local_script.sh', 'local script executed')
    expect(P, 'absolute path',              '/bin/echo abs_ok', 'abs_ok')
    expect(P, '% skips cwd',                '%local_script.sh', 'cshell: command not found (local_script.sh)')
    expect(P, '% still finds PATH binaries','%ls', 'multiline.txt')
    expect(P, 'directory is not a command', 'nest1', 'cshell: command not found')
    expect(P, 'unknown command message',    'arbitrary_fake_cmd', 'cshell: command not found (arbitrary_fake_cmd)')
    expect(P, 'non-executable file',        './non_exec.sh', 'cshell: command not found')

    P = 'C2 input redirection'
    expect(P, 'single input',               'cat < multiline.txt', 'line one')
    expect(P, 'two inputs concatenate',     'cat < alpha.txt < beta.txt', 'aaa\nbbb')
    expect(P, 'input order is preserved',   'cat < beta.txt < alpha.txt', 'bbb\naaa')
    expect(P, 'missing input file',         'cat < does_not_exist.txt', 'cshell: no such file or directory')
    expect(P, 'missing input aborts command', 'cat < does_not_exist.txt', 'line one', 'out')

    P = 'C3 output redirection'
    one(['echo redirected > o1.txt'])
    check(P, 'single output writes the file', (read('o1.txt') or '').strip() == 'redirected',
          f'o1.txt = {read("o1.txt")!r}')
    one(['echo multi > m1.txt > m2.txt'])
    check(P, 'both files receive full output',
          (read('m1.txt') or '').strip() == 'multi' and (read('m2.txt') or '').strip() == 'multi',
          f'm1={read("m1.txt")!r} m2={read("m2.txt")!r}')
    one(['echo first > ap.txt', 'echo second >> ap.txt'])
    check(P, '>> appends', (read('ap.txt') or '') == 'first\nsecond\n', f'ap.txt = {read("ap.txt")!r}')
    one(['echo one > tr.txt', 'echo two > tr.txt'])
    check(P, '> truncates', (read('tr.txt') or '') == 'two\n', f'tr.txt = {read("tr.txt")!r}')
    one(['cat < multiline.txt > copy.txt'])
    check(P, 'input and output combined', 'line three' in (read('copy.txt') or ''),
          f'copy.txt = {read("copy.txt")!r}')
    expect(P, 'redirection produces no stdout', 'echo quiet > q.txt', 'quiet', 'out')

    P = 'C4 pipelines'
    expect(P, 'two stages',      'cat multiline.txt | grep two', 'line two')
    expect(P, 'three stages',    'cat multiline.txt | sort | head -n 1', 'line one')
    expect(P, 'pipeline terminates (no deadlock)', 'cat large_log.txt | wc -l', '1000', settle=1.2)
    one(['cat < multiline.txt | grep line > pipe_out.txt'])
    check(P, 'pipeline with redirection', 'line two' in (read('pipe_out.txt') or ''),
          f'pipe_out.txt = {read("pipe_out.txt")!r}')
    expect(P, 'bad first stage is isolated', 'fakecmd_stage | sort', 'cshell: command not found (fakecmd_stage)')
    expect(P, 'good stage still runs',  'fakecmd_stage | echo survived', 'survived')
    expect(P, 'builtin inside a pipeline', 'reveal | peek -n', '1 ')


# ════════════════════════ PART D1 ════════════════════════
def part_D1():
    P = 'D1 sequential'
    expect(P, 'both commands run',        'echo first ; echo second', 'first\nsecond')
    expect(P, 'three commands in order',  'echo a ; echo b ; echo c', 'a\nb\nc')
    expect(P, 'stops after command-not-found',
           'echo before ; nada_cmd ; echo after', 'after', 'out')
    expect(P, 'error is reported before stopping',
           'echo before ; nada_cmd ; echo after', 'cshell: command not found (nada_cmd)')
    expect(P, 'non-zero exit does NOT stop',
           'ls /nonexistent_zzz ; echo continued', 'continued')
    expect(P, 'failed pipeline stage does NOT stop',
           'badcmd | sort ; echo continued', 'continued')
    expect(P, 'sequential commands do not interleave',
           'echo one ; echo two', 'one\ntwo')
    # A directory passes access(X_OK), so without an S_ISREG guard it resolves
    # as a command and the sequence wrongly continues.
    expect(P, 'a directory name stops the sequence',
           'nest1 ; echo after_dir', 'after_dir', 'out')


# ════════════════════════ PART D2 ════════════════════════
def part_D2():
    P = 'D2 background'
    out = one(['sleep 0.3 &'], tail=1.2)
    check(P, 'launch line has [n] pid form', re.search(r'\[1\]\s+\d+', out) is not None,
          f'got {out.strip()[:120]!r}')
    check(P, 'reports normal completion', 'exited normally' in out, out.strip()[:160])
    check(P, 'completion names the command', re.search(r'sleep with pid \d+ exited normally', out) is not None,
          out.strip()[:160])

    out = one(['sleep 0.2 &', 'sleep 0.2 &'], tail=1.2)
    check(P, 'job numbers increment', '[1]' in out and '[2]' in out, out.strip()[:160])

    out = one(['sleep 0.15 &', 'sleep 1 &'], settle=0.7, tail=1.6)
    check(P, 'numbers are not reused after completion', '[2]' in out and '[1]' in out.split('[2]')[0],
          out.strip()[:200])

    out = one(['echo one & echo two & echo three &'], tail=1.2)
    check(P, 'multiple & on one line all launch',
          all(t in out for t in ('[1]', '[2]', '[3]')), out.strip()[:200])

    out = one(['sleep 0.3 | cat &'], tail=1.5)
    m1 = re.search(r'\[1\]\s+(\d+)', out)
    check(P, 'pipeline reports the first pid', m1 is not None, out.strip()[:160])
    if m1:
        check(P, 'pipeline completion uses the first command name',
              re.search(r'sleep with pid ' + m1.group(1), out) is not None, out.strip()[:200])
    else:
        check(P, 'pipeline completion uses the first command name', False, 'no launch line')

    # The shell must not block on a background job.
    sh = Sh()
    t0 = time.time()
    sh.cmd('sleep 5 &', 0.3)
    sh.cmd('echo prompt_returned', 0.3)
    elapsed = time.time() - t0
    check(P, 'shell does not block on &', elapsed < 2.0, f'took {elapsed:.2f}s')
    sh.close()

    # Abnormal termination is reported differently.
    sh = Sh()
    out = sh.cmd('sleep 5 &', 0.4)
    m = re.search(r'\[1\]\s+(\d+)', out)
    if m:
        out2 = sh.cmd('ping %1 9', 0.8)
        check(P, 'reports abnormal termination', 'exited abnormally' in out2, out2.strip()[:160])
    else:
        check(P, 'reports abnormal termination', False, 'could not launch background job')
    sh.close()


CTRL_C, CTRL_Z, CTRL_D = b'\x03', b'\x1a', b'\x04'


def proc_state(pid):
    r = subprocess.run(['ps', '-o', 'stat=', '-p', str(pid)],
                       capture_output=True, text=True).stdout.strip()
    return r or '(gone)'


# ════════════════════════ PART E1 ════════════════════════
def part_E1():
    P = 'E1 activities'
    sh = Sh()
    sh.cmd('sleep 5 &', 0.4)
    out = sh.cmd('activities', 0.5)
    check(P, 'group header is "[n] pgid <pgid>"',
          re.search(r'\[1\]\s+pgid\s+\d+', out) is not None, out.strip()[:160])
    check(P, 'process line is "pid name state"',
          re.search(r'\d+\s+sleep\s+(Running|Stopped)', out) is not None, out.strip()[:160])
    check(P, 'a running job shows Running', 'Running' in out, out.strip()[:160])
    sh.close()

    sh = Sh()
    sh.cmd('sleep 5 &', 0.35)
    sh.cmd('sleep 6 &', 0.35)
    out = sh.cmd('activities', 0.5)
    i1, i2 = out.find('[1]'), out.find('[2]')
    check(P, 'groups listed in launch order', i1 != -1 and i2 != -1 and i1 < i2, out.strip()[:200])
    sh.close()

    sh = Sh()
    sh.cmd('sleep 0.2 &', 0.9)
    out = sh.cmd('activities', 0.5)
    check(P, 'finished jobs are removed from the list', '[1]' not in out, out.strip()[:160])
    sh.close()

    sh = Sh()
    sh.cmd('sleep 5 | cat &', 0.5)
    out = sh.cmd('activities', 0.5)
    check(P, 'pipeline lists every stage',
          'sleep' in out and 'cat' in out, out.strip()[:200])
    check(P, 'pipeline members share one pgid',
          len(re.findall(r'\[1\]\s+pgid', out)) == 1, out.strip()[:200])
    sh.close()

    expect(P, 'activities rejects arguments', 'activities extra', 'invalid syntax')


# ════════════════════════ PART E2 ════════════════════════
def part_E2():
    P = 'E2 signals/terminal'
    sh = Sh()
    sh.cmd('sleep 5', 0.5)
    sh.raw(CTRL_C, 0.4)
    out = sh.cmd('echo shell_alive', 0.4)
    check(P, 'Ctrl-C does not kill the shell', 'shell_alive' in out, out.strip()[:160])
    sh.close()

    sh = Sh()
    sh.cmd('sleep 9', 0.5)
    out = clean(sh.raw(CTRL_Z, 0.6))
    check(P, 'Ctrl-Z stops the foreground job',
          re.search(r'\[1\]\s*\+\s*Stopped', out) is not None, out.strip()[:160])
    check(P, 'stop line shows the command', 'sleep 9' in out, out.strip()[:160])
    out2 = sh.cmd('activities', 0.5)
    check(P, 'stopped job appears in activities', 'Stopped' in out2, out2.strip()[:160])
    out3 = sh.cmd('echo still_alive', 0.4)
    check(P, 'Ctrl-Z does not kill the shell', 'still_alive' in out3, out3.strip()[:160])
    sh.close()

    # Ctrl-D with a stopped job must warn, then exit on the second press.
    sh = Sh()
    sh.cmd('sleep 9', 0.5)
    sh.raw(CTRL_Z, 0.6)
    out = clean(sh.raw(CTRL_D, 0.6))
    check(P, 'first Ctrl-D warns about stopped jobs', 'there are stopped jobs' in out, out.strip()[:160])
    check(P, 'shell survives the first Ctrl-D', sh.running(), 'shell exited on the first Ctrl-D')
    sh.raw(CTRL_D, 0.4)
    check(P, 'second Ctrl-D exits', sh.exited(), 'shell still running after two Ctrl-D')
    sh.close()

    sh = Sh()
    sh.raw(CTRL_D, 0.4)
    check(P, 'Ctrl-D on an empty line exits', sh.exited(), 'shell still running')
    sh.close()

    # Ctrl-D with text already typed must keep the text and stay alive.
    sh = Sh()
    sh.raw(b'echo kept_text', 0.4)
    sh.raw(CTRL_D, 0.5)
    check(P, 'Ctrl-D with typed text does not exit', sh.running(), 'shell exited')
    out = sh.raw(b'\n', 0.5)
    check(P, 'typed text is retained', 'kept_text' in clean(out), clean(out).strip()[:160])
    sh.close()

    # On exit the shell must SIGHUP every tracked job.
    sh = Sh()
    out = sh.cmd('sleep 30 &', 0.5)
    m = re.search(r'\[1\]\s+(\d+)', out)
    if m:
        child = int(m.group(1))
        before = proc_state(child)
        sh.raw(CTRL_D, 0.8)
        sh.exited(2.0)          # polls; never blocks forever
        time.sleep(0.5)
        after = proc_state(child)
        check(P, 'background jobs receive SIGHUP on exit',
              before not in ('(gone)',) and after in ('(gone)', 'Z', 'Z+'),
              f'before={before} after={after}')
    else:
        check(P, 'background jobs receive SIGHUP on exit', False, 'could not launch job')
    sh.close()


# ════════════════════════ PART E3 ════════════════════════
def part_E3():
    P = 'E3 resume'
    sh = Sh()
    sh.cmd('sleep 9', 0.5)
    sh.raw(CTRL_Z, 0.6)
    out = sh.cmd('resume %1 bg', 0.5)
    check(P, 'bg prints "[n] + Running <cmd>"',
          re.search(r'\[1\]\s*\+\s*Running', out) is not None and 'sleep 9' in out, out.strip()[:160])
    out2 = sh.cmd('activities', 0.5)
    check(P, 'bg job is still tracked', '[1]' in out2, out2.strip()[:160])
    sh.close()

    sh = Sh()
    sh.cmd('sleep 1', 0.4)
    sh.raw(CTRL_Z, 0.6)
    out = sh.cmd('resume %1 fg', 2.0)
    check(P, 'fg echoes the command line', 'sleep 1' in out, out.strip()[:160])
    out2 = sh.cmd('activities', 0.5)
    check(P, 'finished fg job leaves the table', '[1]' not in out2, out2.strip()[:160])
    sh.close()

    sh = Sh()
    sh.cmd('sleep 30', 0.5)
    sh.raw(CTRL_Z, 0.6)
    sh.raw(b'resume %1 fg --timeout 2\n', 0.05)
    out, elapsed = sh.wait_for('resume: job timed out', 8.0)
    check(P, '--timeout terminates the job', elapsed is not None, out.strip()[:160])
    check(P, '--timeout fires near the requested time',
          elapsed is not None and 1.4 < elapsed < 3.5,
          f'fired after {elapsed:.2f}s' if elapsed else 'never fired')
    out2 = sh.cmd('activities', 0.5)
    check(P, 'timed-out job is removed', '[1]' not in out2, out2.strip()[:160])
    sh.close()

    sh = Sh()
    sh.cmd('sleep 1', 0.4)
    sh.raw(CTRL_Z, 0.6)
    sh.raw(b'resume %1 fg --timeout 10\n', 0.05)
    t0 = time.time()
    out = clean(sh.drain(3.0))
    elapsed = time.time() - t0
    check(P, 'timer is cancelled when the job finishes early',
          'timed out' not in out, f'after {elapsed:.2f}s: {out.strip()[:120]!r}')
    sh.close()

    expect(P, 'unknown job number',   'resume %9 fg', 'resume: no such job')
    expect(P, 'missing % prefix',     'resume 1 fg', 'resume: invalid syntax')
    expect(P, 'missing mode',         'resume %1', 'resume: invalid syntax')
    expect(P, 'bad mode word',        'resume %1 middle', 'resume: invalid syntax')
    expect(P, '--timeout with bg',    'resume %1 bg --timeout 2', 'resume: invalid syntax')
    expect(P, '--timeout without value', 'resume %1 fg --timeout', 'resume: invalid syntax')
    expect(P, 'non-numeric timeout',  'resume %1 fg --timeout abc', 'resume: invalid syntax')


# ════════════════════════ PART E4 ════════════════════════
def part_E4():
    P = 'E4 ping'
    sh = Sh()
    sh.cmd('sleep 9 &', 0.4)
    out = sh.cmd('ping %1 0', 0.4)
    check(P, 'success message echoes target and signal', 'Sent signal 0 to %1' in out, out.strip()[:160])
    out = sh.cmd('ping %1 79', 0.6)
    check(P, 'signal is reduced mod 64 (79 -> 15)', 'Sent signal 79 to %1' in out, out.strip()[:160])
    check(P, 'reduced signal actually terminates', 'exited abnormally' in out, out.strip()[:200])
    sh.close()

    sh = Sh()
    out = sh.cmd('sleep 9 &', 0.4)
    m = re.search(r'\[1\]\s+(\d+)', out)
    if m:
        out2 = sh.cmd(f'ping {m.group(1)} 0', 0.4)
        check(P, 'plain number targets a pid', f'Sent signal 0 to {m.group(1)}' in out2, out2.strip()[:160])
    else:
        check(P, 'plain number targets a pid', False, 'no job launched')
    sh.close()

    expect(P, 'untracked pid rejected',      'ping 1 9', 'ping: no such process found')
    expect(P, 'unknown job rejected',        'ping %7 9', 'ping: no such process found')
    expect(P, 'non-integer signal',          'ping 123 abc', 'ping: invalid syntax')
    expect(P, 'negative signal',             'ping 123 -3', 'ping: invalid syntax')
    expect(P, 'missing signal argument',     'ping 123', 'ping: invalid syntax')
    expect(P, 'too many arguments',          'ping 123 9 extra', 'ping: invalid syntax')
    expect(P, 'signal checked before target','ping 999999 abc', 'ping: invalid syntax')


# ════════════════════════ report ════════════════════════
class SectionTimeout(Exception):
    pass


def _on_alarm(signum, frame):
    raise SectionTimeout()


SECTION_LIMIT = 180     # seconds; generous, but bounded


def main():
    if not os.path.isfile(SHELL) or not os.access(SHELL, os.X_OK):
        print(f'Cannot run: {SHELL} is missing or not executable.')
        print('Build it first (make all), or pass the path: python3 test_shell.py path/to/shell.out')
        sys.exit(2)

    build_sandbox()
    print(f'Testing  {SHELL}')
    print(f'Sandbox  {SANDBOX}')
    print('This takes roughly 3 minutes - most of it is real sleeps and signal timing.')
    print('Run a single section with e.g.:  python3 test_shell.py ./shell.out E2\n', flush=True)

    sections = [('A', part_A), ('B1', part_B1), ('B2', part_B2), ('B3', part_B3),
                ('B4', part_B4), ('C', part_C), ('D1', part_D1), ('D2', part_D2),
                ('E1', part_E1), ('E2', part_E2), ('E3', part_E3), ('E4', part_E4)]

    only = [a.upper() for a in sys.argv[2:] if not a.startswith('-')]
    if only:
        sections = [(k, f) for k, f in sections if k in only]
        if not sections:
            print(f'No sections match {only}. Valid: A B1 B2 B3 B4 C D1 D2 E1 E2 E3 E4')
            sys.exit(2)

    signal.signal(signal.SIGALRM, _on_alarm)
    t_start = time.time()

    for key, fn in sections:
        print(f'\n--- {key} ---', flush=True)
        t0 = time.time()
        signal.alarm(SECTION_LIMIT)
        try:
            fn()
        except SectionTimeout:
            check(key, 'SECTION TIMED OUT', False,
                  f'exceeded {SECTION_LIMIT}s - a test is hanging in {fn.__name__}')
        except Exception as e:
            check(key, 'section crashed', False, repr(e))
        finally:
            signal.alarm(0)
        print(f'    ({time.time() - t0:.1f}s)', flush=True)

    parts, order = {}, []
    for part, name, ok, detail in results:
        if part not in parts:
            parts[part] = []
            order.append(part)
        parts[part].append((name, ok, detail))

    failures = []
    print('=' * 72)
    for part in order:
        rows = parts[part]
        p = sum(1 for _, ok, _ in rows if ok)
        bar = '#' * round(20 * p / len(rows)) + '.' * (20 - round(20 * p / len(rows)))
        print(f'{part:<22} {bar} {p:>3}/{len(rows)}')
        for name, ok, detail in rows:
            if not ok:
                failures.append((part, name, detail))
    print('=' * 72)

    total = len(results)
    passed = sum(1 for _, _, ok, _ in results if ok)
    print(f'TOTAL  {passed}/{total}  ({100*passed//max(total,1)}%)'
          f'   in {time.time() - t_start:.0f}s\n')

    if failures:
        print('FAILURES')
        print('-' * 72)
        for part, name, detail in failures:
            print(f'[{part}] {name}')
            if detail:
                print(f'    {detail}')
        print()
    else:
        print('All checks passed.\n')

    shutil.rmtree(SANDBOX, ignore_errors=True)
    sys.exit(1 if failures else 0)


if __name__ == '__main__':
    main()

"""Runs scripted UI scenarios against the real app, offscreen, on scratch dirs.

A scenario prepares a data dir, then replays a script (see UiDriver, src/core/uidriver.h) and
fails on a wrong screen, a non-zero exit or a QML warning. Run a test file with
`python3 -I tests/ui/<name>_test.py <path to lattis>`.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DEMO = os.path.join(ROOT, "tools", "demo-data")

# Lines that match WARNING_PATTERN but are not bugs. Add the exact text with the reason.
KNOWN_WARNINGS = []

WARNING_PATTERN = re.compile(
    r"TypeError|ReferenceError|Unable to assign|Binding loop|is not a type|Cannot (read|assign|anchor)"
    r"|(qrc:|file://)\S*\.qml:\d+:\d+|^QQml|^qt\.qml|^qml:"
)
FRAME_PATTERN = re.compile(r"first frame after (\d+) ms")

# Offscreen is software-rendered and runs beside other tests; the 300 ms budget is measured with
# tools/screenshot.sh. This only catches a start that has gone wrong.
FIRST_FRAME_LIMIT_MS = 3000


class Dirs:
    def __init__(self, root):
        self.root = root
        self.data = os.path.join(root, "data")
        self.home = os.path.join(root, "home")
        self.runtime = os.path.join(root, "runtime")
        self.flatpak = os.path.join(root, "flatpak")
        for path in (self.data, self.home, self.flatpak):
            os.makedirs(path)
        os.makedirs(self.runtime, mode=0o700)

    def write(self, relative, content):
        path = os.path.join(self.data, relative)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        mode = "wb" if isinstance(content, bytes) else "w"
        with open(path, mode) as f:
            f.write(content)

    def use_demo(self):
        shutil.copytree(DEMO, self.data, dirs_exist_ok=True)


class Scenario:
    """steps: a list of (script, check) runs of the app on the same dirs, in order; check gets the Result."""

    def __init__(self, name, steps, setup=None):
        self.name = name
        self.steps = steps
        self.setup = setup


class Result:
    def __init__(self, code, output, frame_ms):
        self.code = code
        self.output = output
        self.frame_ms = frame_ms


def run_app(binary, dirs, script):
    script_path = os.path.join(dirs.root, "script.txt")
    with open(script_path, "w") as f:
        f.write(script)
    env = {
        "PATH": os.environ.get("PATH", "/usr/bin"),
        "QT_QPA_PLATFORM": "offscreen",
        "QT_FORCE_STDERR_LOGGING": "1",
        "LATTIS_DATA": dirs.data,
        "LATTIS_UI_SCRIPT": script_path,
        "HOME": dirs.home,
        "XDG_CACHE_HOME": os.path.join(dirs.root, "cache"),
        "XDG_STATE_HOME": os.path.join(dirs.root, "state"),
        "XDG_CONFIG_HOME": os.path.join(dirs.root, "config"),
        "XDG_DATA_HOME": os.path.join(dirs.root, "share"),
        "XDG_RUNTIME_DIR": dirs.runtime,
        "FLATPAK_SYSTEM_DIR": dirs.flatpak,
        # No session bus: nothing here may reach the real keyring or other services.
        "DBUS_SESSION_BUS_ADDRESS": "unix:path=/nonexistent",
    }
    try:
        proc = subprocess.run([binary], env=env, capture_output=True, text=True, timeout=90)
        code, output = proc.returncode, proc.stdout + proc.stderr
    except subprocess.TimeoutExpired as e:
        code = -1
        output = ((e.stdout or b"").decode(errors="replace") + (e.stderr or b"").decode(errors="replace")
                  + "\nuitest: FAIL the app didn't exit in 90 s\n")
    match = FRAME_PATTERN.search(output)
    return Result(code, output, int(match.group(1)) if match else None)


def problems(result, expect_clean_exit=True):
    found = []
    if expect_clean_exit and result.code != 0:
        found.append(f"exit code {result.code}")
    for line in result.output.splitlines():
        if line.startswith("uitest: FAIL"):
            found.append(line)
        elif WARNING_PATTERN.search(line) and not any(known in line for known in KNOWN_WARNINGS):
            found.append("warning: " + line.strip())
    if result.code == 0 and "uitest: done" not in result.output:
        found.append("the script didn't finish")
    if result.frame_ms is None:
        found.append("no first frame logged")
    elif result.frame_ms > FIRST_FRAME_LIMIT_MS:
        found.append(f"first frame took {result.frame_ms} ms")
    return found


def run_scenarios(scenarios):
    binary = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(ROOT, "build", "lattis")
    only = sys.argv[2] if len(sys.argv) > 2 else None
    failed = 0
    for scenario in scenarios:
        if only and only not in scenario.name:
            continue
        with tempfile.TemporaryDirectory(prefix="lattis-ui-") as root:
            dirs = Dirs(root)
            if scenario.setup:
                scenario.setup(dirs)
            found = []
            for number, (script, check) in enumerate(scenario.steps, 1):
                result = run_app(binary, dirs, script)
                step_problems = problems(result)
                if check:
                    step_problems += check(result, dirs) or []
                found += [f"run {number}: {p}" for p in step_problems]
                if step_problems:
                    tail = [line for line in result.output.splitlines()
                            if line.startswith("uitest:") and not any(line in p for p in step_problems)]
                    found += ["    " + line for line in tail[-6:]]
                    break
        print(("FAIL " if found else "ok   ") + scenario.name)
        for line in found:
            print("     " + line)
        failed += bool(found)
    print(f"{failed} failed")
    return 1 if failed else 0

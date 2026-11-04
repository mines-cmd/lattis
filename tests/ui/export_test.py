import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios

FIRST = "expect Mario Kart 8 Deluxe\nexpect Pikmin 4\nexpect Hades\n"
FAVORITES = "key Ctrl+Shift+D\nwait 400\n"
# These actions have no keys by default. Binding them opens Settings on the Library page, which a click can't
# reach (the General page has a heading with the same text).
SHORTCUTS = '\n[shortcuts]\nback_up_library = "Ctrl+Alt+1"\nback_up_saves = "Ctrl+Alt+2"\nimport_backup = "Ctrl+Alt+3"\n'
LIBRARY, SAVES, IMPORT = (f"wait 300\nkey Ctrl+Alt+{n}\nwait 800\n" for n in (1, 2, 3))
SAVE_FILE = ".config/Ryujinx/bis/user/save/0001/save.bin"


def archive(dirs):
    return os.path.join(dirs.root, "backup.tar.zst")


def hooks(dirs, start=True):
    dirs.env["LATTIS_BACKUP_TO"] = archive(dirs)
    dirs.env["LATTIS_IMPORT_FROM"] = archive(dirs)
    if start:
        dirs.env["LATTIS_IMPORT_START"] = "1"


def list_archive(path):
    proc = subprocess.run(["tar", "--zstd", "-tf", path], capture_output=True, text=True)
    return proc.stdout.splitlines() if proc.returncode == 0 else None


def demo(dirs):
    dirs.use_demo()
    with open(os.path.join(dirs.data, "settings.toml"), "a") as f:
        f.write(SHORTCUTS)
    hooks(dirs)


def with_saves(dirs):
    demo(dirs)
    program = os.path.join(dirs.bin, "Ryujinx")
    with open(program, "w") as f:
        f.write("#!/bin/sh\n")
    os.chmod(program, 0o755)
    save = os.path.join(dirs.home, SAVE_FILE)
    os.makedirs(os.path.dirname(save))
    with open(save, "w") as f:
        f.write("original")


def read_save(dirs):
    with open(os.path.join(dirs.home, SAVE_FILE)) as f:
        return f.read()


def write_save(dirs, text):
    with open(os.path.join(dirs.home, SAVE_FILE), "w") as f:
        f.write(text)


def made_library_backup(result, dirs):
    names = list_archive(archive(dirs))
    if names is None:
        return ["the library backup isn't a readable .tar.zst"]
    found = []
    if not names or names[0] != "manifest.toml":
        found.append(f"the backup doesn't start with manifest.toml: {names[:1]}")
    if not any(n.startswith("data/library/games/") for n in names):
        found.append("the backup holds no game files")
    if not any(n == "data/settings.toml" for n in names):
        found.append("the backup holds no settings.toml")
    return found


def made_saves_backup(result, dirs):
    names = list_archive(archive(dirs))
    if names is None:
        return ["the saves backup isn't a readable .tar.zst"]
    if not any(n.startswith("emulators/ryubing/saves/") and n.endswith("save.bin") for n in names):
        return [f"the backup holds no Ryubing save: {names}"]
    write_save(dirs, "changed")
    return []


def moved_aside(dirs):
    folders = []
    for base, names, _ in os.walk(dirs.home):
        folders += [n for n in names if ".before-import-" in n]
    return folders


def restored_saves(result, dirs):
    found = []
    if read_save(dirs) != "original":
        found.append(f"the save wasn't restored: {read_save(dirs)!r}")
    if not moved_aside(dirs):
        found.append("the save that was there wasn't moved aside")
    return found


def build_archive(dirs, manifest, with_data=True, blob=0):
    stage = os.path.join(dirs.root, "stage")
    os.makedirs(stage, exist_ok=True)
    with open(os.path.join(stage, "manifest.toml"), "w") as f:
        f.write(manifest)
    members = ["manifest.toml"]
    if with_data:
        os.makedirs(os.path.join(stage, "data"), exist_ok=True)
        with open(os.path.join(stage, "data", "settings.toml"), "w") as f:
            f.write("# from the archive\n")
        if blob:
            with open(os.path.join(stage, "data", "blob.bin"), "wb") as f:
                f.write(os.urandom(blob))
        members.append("data")
    subprocess.run(["tar", "--zstd", "-cf", archive(dirs), "-C", stage] + members, check=True)


def broken(prepare):
    def setup(dirs):
        demo(dirs)
        prepare(dirs)
    return setup


def write_bytes(content):
    def prepare(dirs):
        with open(archive(dirs), "wb") as f:
            f.write(content)
    return prepare


def manifest_text(kind="library", format=1, extra=""):
    return f'kind = "{kind}"\nformat = {format}\napp_version = "9.9"\nlibrary_version = 1\n{extra}'


def library_untouched(result, dirs):
    with open(os.path.join(dirs.data, "settings.toml")) as f:
        return [] if "from the archive" not in f.read() else ["the data folder was replaced by a bad backup"]


def replaced(result, dirs):
    kept = [n for n in os.listdir(dirs.root) if n.startswith("data.before-import-")]
    return [] if kept else ["the old library wasn't moved aside"]


def import_shows(error, setup):
    return Scenario(f"Import of {error[0]} says {error[1]!r} and leaves the library alone",
                    [(FIRST + IMPORT + "wait 600\nexpect-has " + error[1] + "\nkey Escape\nwait 300\n",
                      library_untouched)], setup)


IMPORT_ERRORS = [
    ("a text file", "isn't a backup", broken(write_bytes(b"this is not a backup\n"))),
    ("an empty file", "isn't a backup", broken(write_bytes(b""))),
    ("an archive with no manifest", "doesn't start with manifest.toml",
     broken(lambda d: build_archive(d, "x", with_data=True) or os.remove(archive(d))
            or subprocess.run(["tar", "--zstd", "-cf", archive(d), "-C", os.path.join(d.root, "stage"), "data"], check=True))),
    ("a manifest that isn't TOML", "isn't a Lattis backup", broken(lambda d: build_archive(d, "kind = = broken"))),
    ("an unknown kind", "isn't a Lattis backup", broken(lambda d: build_archive(d, manifest_text(kind="mystery")))),
    ("format 0, from before backups had a format", "isn't a Lattis backup",
     broken(lambda d: build_archive(d, manifest_text(format=0)))),
    ("a manifest with no format", "isn't a Lattis backup", broken(lambda d: build_archive(d, 'kind = "library"\n'))),
    ("a newer format", "newer version of Lattis", broken(lambda d: build_archive(d, manifest_text(format=99)))),
    ("a truncated backup", "damaged", broken(lambda d: (
        build_archive(d, manifest_text(), blob=2000000),
        write_bytes(open(archive(d), "rb").read()[:1000000])(d)))),
]

SCENARIOS = [
    Scenario("Back up library writes a .tar.zst with the manifest first, the games and the settings",
             [(FIRST + LIBRARY + "expect Backed up.\nwait 300\n", made_library_backup)], demo),
    Scenario("a library backup restores the library: a favorite added after the backup is gone",
             [(FIRST + LIBRARY + "expect Backed up.\nwait 300\n", made_library_backup),
              (FIRST + "key Down\nkey Ctrl+D\nwait 600\n" + FAVORITES + "expect Kirby and the Forgotten Land\n" + FAVORITES, None),
              (FIRST + IMPORT + "wait 4000\n", replaced),
              (FIRST + FAVORITES + "expect Hades\nexpect-not Kirby and the Forgotten Land\n", None)], demo),
    Scenario("Import shows what a library backup holds and what it replaces before anything happens",
             [(FIRST + LIBRARY + "expect Backed up.\nwait 300\n", made_library_backup),
              (FIRST + IMPORT + "expect Import library\nexpect Replaces your library\nexpect-has From this library.\n"
               + "key Escape\nwait 300\n" + FIRST, None)], lambda d: (demo(d), d.env.pop("LATTIS_IMPORT_START"))),
    Scenario("Back up saves writes the emulator's saves, and Import restores them and moves the changed ones aside",
             [(FIRST + SAVES + "expect Backed up.\nwait 300\n", made_saves_backup),
              (FIRST + IMPORT + "wait 1500\nexpect-has Import saves\n", restored_saves)], with_saves),
] + [import_shows(e[:2], e[2]) for e in IMPORT_ERRORS]

sys.exit(run_scenarios(SCENARIOS))

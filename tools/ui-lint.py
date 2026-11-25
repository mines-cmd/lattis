#!/usr/bin/env python3
# Checks the QML and the theme defaults for the look rules: sizes, borders, radii, spacing and colors
# come from the theme, and popups use the shared backgrounds. Existing breaks are counted in
# ui-lint-allowed.txt; the check fails when a count grows, and when it shrinks so the list stays exact.
# Usage: ui-lint.py [--update]   (from anywhere; paths are relative to the repo; ctest runs it as ui_lint)
import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ALLOWED = ROOT / "tools" / "ui-lint-allowed.txt"

# Base components whose job is to draw a popup's background.
BACKGROUND_OWNERS = {
    "qml/components/AppDialog.qml",
    "qml/components/MotionPopup.qml",
    "qml/components/PopoverBackground.qml",
    "qml/settings/SettingsComboBox.qml",
    "qml/settings/SettingsMenu.qml",
}
# Items that draw `text` and take Text.AutoText by default, so a string that starts with a tag is drawn as rich text.
TEXT_TYPES = {"Label", "Text", "CaptionLabel", "Muted", "CardLabel", "CardTitle", "DialogHint", "DialogPathHint"}
# The fields that hold a name or a sentence somebody else wrote: a game's, a lookup candidate's, a Steam friend's.
OUTSIDE_TEXT = re.compile(r"\??\.(?:summary|description|gameName|error|applyError|searched|searchedName)\b"
                          r"|\b(?:game|modelData|Library|Metadata|match|current)\??\.(?:title|name|value|detail)\b")
POPUP_TYPES = {"Popup", "MotionPopup", "Dialog", "Drawer", "Menu"}

# Theme settings whose default stays a fixed number, with why. Keep this short.
SCHEMA_EXEMPT = {
    "filters.histogram_bar_radius": "2 px bars; a tier (4 at least) would round them into dots",
    "settings_window.preview_dot_radius": "a 4 px dot; the smallest tier is as wide as the dot",
}

# Settings read by a name built at run time, so no file names them whole. Keep this short.
UNUSED_EXEMPT_PREFIXES = {
    "stat_chip.protondb_": "GameFields.qml picks the color by the rating's name",
}

# A line ends with `// ui-lint: allow <why>` when the literal is data, not styling.
ALLOW = re.compile(r"//\s*ui-lint:\s*allow\s+\S")

NUMBER = r"(-?\d+(?:\.\d+)?)\s*$"
STRING = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')
OPEN = re.compile(r"(?:([\w.]+)\s*:\s*)?([A-Za-z_][\w.]*)?\s*$")

RULES = {
    "plain-text": "text from a game, a lookup or a file bound to a Label or Text that is not plain; use PlainLabel or set textFormat",
    "open-url": "Qt.openUrlExternally keeps the AppImage's environment; use GameRunner.openUrl",
    "popup-background":"a popup draws its own background; use the shared dialog or popover one",
    "border-width": "literal border width; use layout.border_width or a theme setting",
    "font-size": "literal font size; use a type step (@font.*) through a theme setting",
    "radius": "literal radius above 1; use a [radius] tier through a theme setting",
    "spacing": "literal spacing, margin or padding above 2; use layout.spacing or a theme setting",
    "color": "literal color; use Theme.*",
    "schema-type": "text size default doesn't end at a type step in [font]",
    "schema-border": "border width default doesn't end at layout.border_width, hairline_width, ring_width or 0",
    "schema-radius": "radius default doesn't end at a [radius] tier, 0 or 1",
    "schema-unused": "no QML or C++ file reads this theme setting, and no other setting follows it; delete it",
}


def code_of(line):
    line = STRING.sub('""', line)
    cut = line.find("//")
    return line if cut < 0 else line[:cut]


def lint_qml(path, rel, hits):
    stack = []  # (property, type) for each open brace
    texts = []  # for each open brace: [line, an outside `text` binding, whether textFormat is set]
    in_comment = False
    for number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if in_comment:
            end = raw.find("*/")
            if end < 0:
                continue
            raw, in_comment = raw[end + 2:], False
        raw = re.sub(r"/\*.*?\*/", "", raw)
        if "/*" in raw:
            raw, in_comment = raw[:raw.find("/*")], True

        allowed_here = bool(ALLOW.search(raw))
        if "Qt.openUrlExternally" in code_of(raw):
            hits.append(("open-url", rel, number, raw.strip()))

        def hit(rule):
            if not allowed_here:
                hits.append((rule, rel, number, raw.strip()))

        if re.search(r'"#[0-9a-fA-F]{3,8}"|\bQt\.(rgba|hsla|hsva)\(\s*[\d.]+\s*,\s*[\d.]+\s*,\s*[\d.]+', raw):
            hit("color")
        for part in re.split(r"([{};])", code_of(raw)):
            if part == "}":
                if stack:
                    stack.pop()
                    line, source, formatted = texts.pop()
                    if source and not formatted:
                        hits.append(("plain-text", rel, line, source))
            elif part == "{":
                opener = OPEN.search(previous)
                name, kind = (opener.group(1), opener.group(2)) if opener else (None, None)
                if kind and not kind[0].isupper() and kind not in ("border", "font", "anchors"):
                    kind = None
                if (name == "background" and kind in ("Rectangle", "Item") and stack
                        and stack[-1][1] in POPUP_TYPES and rel not in BACKGROUND_OWNERS):
                    hit("popup-background")
                stack.append((name, kind))
                texts.append([number, None, False])
            elif part != ";":
                if stack and stack[-1][1] in TEXT_TYPES:
                    if re.match(r"\s*textFormat\s*:", part):
                        texts[-1][2] = True
                    elif re.match(r"\s*text\s*:", part) and OUTSIDE_TEXT.search(part) and not allowed_here:
                        texts[-1][:2] = [number, raw.strip()]
                check_property(part, stack[-1][1] if stack else None, hit)
            previous = part


def check_property(statement, group, hit):
    prop = re.match(r"\s*([\w.]+)\s*:\s*" + NUMBER, statement)
    if not prop:
        return
    name, value = prop.group(1), float(prop.group(2))
    if group in ("border", "font"):
        name = group + "." + name
    leaf = name.split(".")[-1].lower()
    if name == "border.width" and value != 0:
        hit("border-width")
    elif name in ("font.pixelSize", "font.pointSize"):
        hit("font-size")
    elif leaf == "radius" and value > 1:
        hit("radius")
    elif leaf.endswith(("spacing", "margin", "margins", "padding")) and value > 2:
        hit("spacing")


def lint_schema(hits):
    source = (ROOT / "src" / "theme" / "themeschema.cpp").read_text(encoding="utf-8")
    defaults = {}
    for m in re.finditer(r'\binteger(?:OnRelease)?\(\s*"(\w+)",\s*"(\w+)",\s*"([^"]*)"', source):
        defaults[m.group(1) + "." + m.group(2)] = m.group(3)

    def chain_end(key):
        seen = set()
        while defaults.get(key, "").startswith("@") and key not in seen:
            seen.add(key)
            key = defaults[key][1:]
        return key

    found = set()
    for key in defaults:
        section, name = key.split(".", 1)
        end = chain_end(key)
        value = defaults.get(end)
        if section != "font" and re.search(r"(font|title|header|heading|label)_size$", name):
            if not (end.startswith("font.") or end == "hero.title_max_font_size"):
                found.add(("schema-type", end))
        elif name.endswith("border_width"):
            if end not in ("layout.border_width", "layout.hairline_width", "layout.ring_width") and value != "0":
                found.add(("schema-border", end))
        elif name.endswith("radius") and section != "radius":
            if not end.startswith("radius.") and value not in ("0", "1"):
                found.add(("schema-radius", end))
    for rule, key in found:
        if key in SCHEMA_EXEMPT:
            continue
        hits.append((rule, key, 0, key + " = " + defaults.get(key, "?")))
    lint_unused(source, hits)


def camel(name):
    head, *rest = name.split("_")
    return head + "".join(part.capitalize() for part in rest)


def lint_unused(schema, hits):
    # A setting nothing reads does nothing, and the Customize page still offers it. It counts as read when a QML
    # or C++ file names both its section and its key (as Theme.<section>.<key>, or "section", "key"), or when
    # another setting's default points at it.
    rows = re.findall(r'^\s+(?:color|integer|integerOnRelease|real|boolean|text|choice|list|colors)\(\s*'
                      r'"(\w+)",\s*"(\w+)",\s*"([^"]*)"', schema, re.M)
    pointed_at = {default[1:] for _, _, default in rows if default.startswith("@")}
    files = [p for p in (ROOT / "qml").rglob("*.qml")]
    files += [p for pattern in ("*.cpp", "*.h") for p in (ROOT / "src").rglob(pattern)
              if p.name != "themeschema.cpp"]
    texts = [p.read_text(encoding="utf-8") for p in files]

    def mentioned(section, key):
        names = (camel(section), section), (camel(key), key)
        return any(all(re.search(r"\b(?:%s)\b" % "|".join(map(re.escape, n)), text) for n in names) for text in texts)

    for section, key, _ in rows:
        full = section + "." + key
        if full.startswith(tuple(UNUSED_EXEMPT_PREFIXES)):
            continue
        if full not in pointed_at and not mentioned(section, key):
            hits.append(("schema-unused", full, 0, full))


def main():
    update = "--update" in sys.argv

    hits = []
    for path in sorted((ROOT / "qml").rglob("*.qml")):
        lint_qml(path, path.relative_to(ROOT).as_posix(), hits)
    lint_schema(hits)
    counts = Counter((rule, where) for rule, where, _, _ in hits)

    if update:
        lines = ["# rule  file-or-setting  count; written by tools/ui-lint.py --update, only ever lower these"]
        lines += [f"{rule} {where} {n}" for (rule, where), n in sorted(counts.items())]
        ALLOWED.write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(f"ui-lint: wrote {len(counts)} entries to {ALLOWED.relative_to(ROOT)}")
        return 0

    allowed = Counter()
    for line in ALLOWED.read_text(encoding="utf-8").splitlines():
        if line.strip() and not line.startswith("#"):
            rule, where, n = line.split()
            allowed[(rule, where)] = int(n)

    errors = []
    for (rule, where), n in sorted(counts.items()):
        if n > allowed[(rule, where)]:
            errors.append(f"{where}: {RULES[rule]} ({rule}, {n} found, {allowed[(rule, where)]} allowed)")
            errors += [f"    {line}: {text}" for r, w, line, text in hits if (r, w) == (rule, where) and line]
            errors += [f"    {text}" for r, w, line, text in hits if (r, w) == (rule, where) and not line]
    fixed = [f"{where}: {rule} is down to {counts[(rule, where)]} from {n}"
             for (rule, where), n in sorted(allowed.items()) if counts[(rule, where)] < n]
    if fixed:
        errors.append("Fewer breaks than allowed; run tools/ui-lint.py --update to lower the counts:")
        errors += ["    " + line for line in fixed]
    if errors:
        print("ui-lint: the UI breaks the look rules", file=sys.stderr)
        print("\n".join(errors), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

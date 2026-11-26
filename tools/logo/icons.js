// Renders the app icon at the hicolor theme sizes, with rsvg-convert.
//   node tools/logo/icons.js [out dir]
// Writes resources/logo/icons/lattis-<size>.png: the simple dog in Ember's dog_* colors, filled
// into the same templates Lattis renders its other icon colors from (src/install/appicon.cpp; the
// color arithmetic here follows it step for step). At 24 px and below it is the 16 px drawing.
// From 48 px up it has the margin desktop icons usually have; smaller ones fill the canvas,
// where every pixel counts.
const fs = require("fs");
const os = require("os");
const path = require("path");
const { execFileSync } = require("child_process");

const root = path.join(__dirname, "..", "..");
const logo = path.join(root, "resources", "logo");
const out = process.argv[2] || path.join(logo, "icons");
const margin = 14;

function emberDog() {
  const toml = fs.readFileSync(path.join(root, "resources", "themes", "colors", "Ember.toml"), "utf8");
  const rail = toml.split(/^\[rail\]$/m)[1].split(/^\[/m)[0];
  const colors = {};
  for (const name of ["background", "background_end", "coat", "coat_end", "details"]) {
    const match = rail.match(new RegExp(`^dog_${name} = "#([0-9a-fA-F]{6})"$`, "m"));
    if (!match) throw new Error(`Ember.toml has no literal dog_${name}`);
    const v = parseInt(match[1], 16);
    colors[name] = { r: (v >> 16) & 255, g: (v >> 8) & 255, b: v & 255 };
  }
  return colors;
}

function hex(c) {
  const channel = (v) => Math.round(Math.min(255, Math.max(0, v))).toString(16).padStart(2, "0");
  return `#${channel(c.r)}${channel(c.g)}${channel(c.b)}`;
}

function mix(a, b, t) {
  return { r: a.r + (b.r - a.r) * t, g: a.g + (b.g - a.g) * t, b: a.b + (b.b - a.b) * t };
}

function shade(c, saturation, value) {
  const r = c.r / 255, g = c.g / 255, b = c.b / 255;
  const max = Math.max(r, g, b);
  const d = max - Math.min(r, g, b);
  let h = 0;
  if (d > 0) {
    if (max === r) h = ((g - b) / d + 6) % 6;
    else if (max === g) h = (b - r) / d + 2;
    else h = (r - g) / d + 4;
  }
  const s = Math.min(1, (max > 0 ? d / max : 0) * saturation);
  const v = Math.min(1, max * value);
  const chroma = v * s;
  const x = chroma * (1 - Math.abs((h % 2) - 1));
  const m = v - chroma;
  const sectors = [[chroma, x, 0], [x, chroma, 0], [0, chroma, x], [0, x, chroma], [x, 0, chroma], [chroma, 0, x]];
  const [ro, go, bo] = sectors[Math.min(5, Math.trunc(h))];
  return { r: (ro + m) * 255, g: (go + m) * 255, b: (bo + m) * 255 };
}

function fill(template, colors) {
  return template.replace(/\{\{([^}]*)\}\}/g, (whole, inner) => {
    const words = inner.split(" ").filter(Boolean);
    const color = (name) => {
      if (!(name in colors)) throw new Error(`unknown placeholder ${whole}`);
      return colors[name];
    };
    if (words.length === 1) return hex(color(words[0]));
    if (words.length === 4 && words[0] === "mix") return hex(mix(color(words[1]), color(words[2]), Number(words[3])));
    if (words.length === 4 && words[0] === "shade") return hex(shade(color(words[1]), Number(words[2]), Number(words[3])));
    throw new Error(`unknown placeholder ${whole}`);
  });
}

fs.mkdirSync(out, { recursive: true });
const work = fs.mkdtempSync(path.join(os.tmpdir(), "lattis-icons-"));
const colors = emberDog();
const full = path.join(work, "full.svg");
const small = path.join(work, "small.svg");
const spaced = path.join(work, "spaced.svg");
const drawing = fill(fs.readFileSync(path.join(logo, "dog", "lattis-dog-icon.svg"), "utf8"), colors);
const viewBox = 'viewBox="0 0 256 256"';
if (!drawing.includes(viewBox)) throw new Error(`lattis-dog-icon.svg has no ${viewBox}`);
const side = 256 + 2 * margin;
fs.writeFileSync(full, drawing);
fs.writeFileSync(spaced, drawing.replace(viewBox, `viewBox="${-margin} ${-margin} ${side} ${side}"`));
fs.writeFileSync(small, fill(fs.readFileSync(path.join(logo, "dog", "lattis-dog-icon-16px.svg"), "utf8"), colors));

for (const size of [16, 24, 32, 48, 64, 128, 256, 512]) {
  const dest = path.join(out, `lattis-${size}.png`);
  const source = size >= 48 ? spaced : size >= 32 ? full : small;
  execFileSync("rsvg-convert", ["-w", String(size), "-h", String(size), source, "-o", dest]);
  console.log(dest);
}

fs.rmSync(work, { recursive: true, force: true });

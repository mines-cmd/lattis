// Renders the app icon at the hicolor theme sizes, with rsvg-convert.
//   node tools/logo/icons.js
// Writes resources/logo/icons/lattis-<size>.png: the simple Ember dog, in its 16 px drawing at
// 24 px and below. From 48 px up it has the margin desktop icons usually have; smaller ones
// fill the canvas, where every pixel counts.
const fs = require("fs");
const os = require("os");
const path = require("path");
const { execFileSync } = require("child_process");

const logo = path.join(__dirname, "..", "..", "resources", "logo");
const out = path.join(logo, "icons");
const margin = 14;
fs.mkdirSync(out, { recursive: true });
const work = fs.mkdtempSync(path.join(os.tmpdir(), "lattis-icons-"));
const full = path.join(logo, "dog", "lattis-dog-gradient.svg");
const small = path.join(logo, "dog", "lattis-dog-gradient-16px.svg");
const spaced = path.join(work, "spaced.svg");
const drawing = fs.readFileSync(full, "utf8");
const viewBox = 'viewBox="0 0 256 256"';
if (!drawing.includes(viewBox)) throw new Error(`${full} has no ${viewBox}`);
const side = 256 + 2 * margin;
fs.writeFileSync(spaced, drawing.replace(viewBox, `viewBox="${-margin} ${-margin} ${side} ${side}"`));

for (const size of [16, 24, 32, 48, 64, 128, 256, 512]) {
  const dest = path.join(out, `lattis-${size}.png`);
  const source = size >= 48 ? spaced : size >= 32 ? full : small;
  execFileSync("rsvg-convert", ["-w", String(size), "-h", String(size), source, "-o", dest]);
  console.log(dest);
}

fs.rmSync(work, { recursive: true, force: true });

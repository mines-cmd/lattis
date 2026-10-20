// Renders the app icon at the hicolor theme sizes, with rsvg-convert.
//   node tools/logo/icons.js [colorway]   (default: ember)
// Writes resources/logo/icons/lattis-<size>.png: the full mark from 64 px up, the simple
// variant (no halo, bevel or shadow) below, where the full one's details turn to mush.
const fs = require("fs");
const os = require("os");
const path = require("path");
const { execFileSync } = require("child_process");
const { lattisMark, LATTIS_PALETTES } = require("./mark.js");

const way = process.argv[2] || "ember";
const palette = LATTIS_PALETTES[way];
if (!palette) throw new Error(`Unknown colorway "${way}"; have ${Object.keys(LATTIS_PALETTES).join(", ")}`);

const out = path.join(__dirname, "..", "..", "resources", "logo", "icons");
fs.mkdirSync(out, { recursive: true });
const work = fs.mkdtempSync(path.join(os.tmpdir(), "lattis-icons-"));
const full = path.join(work, "full.svg");
const simple = path.join(work, "simple.svg");
fs.writeFileSync(full, lattisMark(palette, { id: way }));
fs.writeFileSync(simple, lattisMark(palette, { id: way + "s", simple: true }));

for (const size of [16, 24, 32, 48, 64, 128, 256, 512]) {
  const dest = path.join(out, `lattis-${size}.png`);
  execFileSync("rsvg-convert", ["-w", String(size), "-h", String(size), size >= 64 ? full : simple, "-o", dest]);
  console.log(dest);
}

fs.rmSync(work, { recursive: true, force: true });

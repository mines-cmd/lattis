// Exports the lockups as transparent PNGs, rendered with headless Brave/Chromium.
//   node tools/logo/export.js [colorway] [scale]   (defaults: ember 4)
// Writes tools/logo/out/png/lattis-<colorway>-<lockup>.png. Uses the styles in
// template.html and the generator in mark.js, so the PNGs match the showcase page.
const fs = require("fs");
const path = require("path");
const { execFileSync } = require("child_process");
const { LATTIS_PALETTES } = require("./mark.js");

const here = __dirname;
const way = process.argv[2] || "ember";
const scale = process.argv[3] || "4";
if (!LATTIS_PALETTES[way]) throw new Error(`Unknown colorway "${way}"; have ${Object.keys(LATTIS_PALETTES).join(", ")}`);

const browser = ["brave-browser", "chromium", "chromium-browser", "google-chrome"].find((b) => {
  try { execFileSync("which", [b], { stdio: "ignore" }); return true; } catch { return false; }
});
if (!browser) throw new Error("Needs brave-browser, chromium or google-chrome");

const wm = 'Latt<span class="i">ı</span>s';
const glyphOnly = `data-layers='{"body":false,"sheen":false,"rim":false,"halo":false}'`;
// Sizes match the lockup tiles on the showcase page.
const lockups = {
  horizontal: `<div class="art"><span class="mark" style="width:92px;height:92px"></span><span class="wm" style="font-size:4.2rem">${wm}</span></div>`,
  stacked: `<div class="art stack"><span class="mark" style="width:96px;height:96px"></span><span class="wm" style="font-size:2.4rem">${wm}</span></div>`,
  wordmark: `<div class="art"><span class="wm" style="font-size:3.6rem">${wm}</span></div>`,
  // For light backgrounds: the wordmark in ink (.wm.ink), as on the page's "On light" tile.
  "horizontal-light": `<div class="art"><span class="mark" style="width:92px;height:92px"></span><span class="wm ink" style="font-size:4.2rem">${wm}</span></div>`,
  "wordmark-light": `<div class="art"><span class="wm ink" style="font-size:3.6rem">${wm}</span></div>`,
  glyph: `<div class="art"><span class="mark" style="width:110px;height:110px" ${glyphOnly}></span></div>`,
  logo: `<div class="art"><span class="mark" style="width:128px;height:128px"></span></div>`,
  // The small-size variant (no halo, bevel or shadow), same as lattis-<colorway>-simple.svg.
  simple: `<div class="art"><span class="mark" style="width:128px;height:128px" data-simple="1"></span></div>`,
};

const template = fs.readFileSync(path.join(here, "template.html"), "utf8");
const style = template.match(/<style>[\s\S]*?<\/style>/)[0];
const mark = fs.readFileSync(path.join(here, "mark.js"), "utf8");

const work = path.join(here, "out", ".export-work");
fs.mkdirSync(work, { recursive: true });
const out = path.join(here, "out", "png");
fs.mkdirSync(out, { recursive: true });
const profile = path.join(here, "out", ".browser-profile");

for (const [name, body] of Object.entries(lockups)) {
  const html = `<!doctype html><meta charset="utf-8">${style}
<style>html, body { background: transparent !important; margin: 0; }
body { width: 100vw; height: 100vh; display: grid; place-items: center; }
.art { display: flex; align-items: center; justify-content: center; gap: 16px; }</style>
${body}
<script>${mark}
var p = LATTIS_PALETTES[${JSON.stringify(way)}], r = document.documentElement.style;
var map = { top: p.top, mid: p.mid, bottom: p.bottom, rim: p.rim, gtop: p.glyphTop, gbottom: p.glyphBottom, stage: p.stage };
Object.keys(map).forEach(function (k) { r.setProperty("--c-" + k, map[k]); });
document.querySelectorAll(".mark").forEach(function (el, i) {
  el.innerHTML = lattisMark(p, { id: "m" + i, simple: el.dataset.simple === "1", layers: el.dataset.layers ? JSON.parse(el.dataset.layers) : {} });
});
</script>`;
  const page = path.join(work, `${name}.html`);
  const raw = path.join(work, `${name}.png`);
  fs.writeFileSync(page, html);
  const args = [
    // The new headless mode never writes the screenshot here; the old one does.
    "--headless=old", "--no-sandbox", "--no-first-run", "--disable-gpu", "--hide-scrollbars", `--user-data-dir=${profile}`,
    "--window-size=900,500", `--force-device-scale-factor=${scale}`, "--default-background-color=00000000",
    "--virtual-time-budget=3000", `--screenshot=${raw}`, `file://${page}`,
  ];
  // The first run on a fresh profile hangs. Kill that browser (its helpers outlive the
  // timeout), clear the profile lock and retry on the now-initialized profile.
  for (let attempt = 1; !fs.existsSync(raw); attempt++) {
    if (attempt > 3) throw new Error(`No screenshot from ${browser} for ${name}`);
    for (const f of ["SingletonLock", "SingletonSocket", "SingletonCookie"]) fs.rmSync(path.join(profile, f), { force: true });
    try { execFileSync(browser, args, { stdio: "ignore", timeout: 30000 }); }
    catch { try { execFileSync("pkill", ["-f", `--user-data-dir=${profile}`]); } catch {} }
  }
  // Crop to the artwork and leave a small transparent margin.
  const dest = path.join(out, `lattis-${way}-${name}.png`);
  execFileSync("magick", [raw, "-trim", "+repage", "-bordercolor", "none", "-border", String(8 * Number(scale)), dest]);
  console.log(dest);
}

fs.rmSync(work, { recursive: true, force: true });

// Builds the Lattis icon SVGs and the identity showcase page.
//   node tools/logo/build.js [out dir]   (default: tools/logo/out)
// Writes lattis-<colorway>.svg and lattis-<colorway>-simple.svg for every
// palette in mark.js, lattis-identity.html from template.html, and the rail logo
// (resources/logo/lattis-ember-simple.svg).
const fs = require("fs");
const path = require("path");
const { lattisMark, LATTIS_PALETTES } = require("./mark.js");

const here = __dirname;
const out = path.resolve(process.argv[2] || path.join(here, "out"));
fs.mkdirSync(out, { recursive: true });

for (const k of Object.keys(LATTIS_PALETTES)) {
  fs.writeFileSync(path.join(out, `lattis-${k}.svg`), lattisMark(LATTIS_PALETTES[k], { id: k }));
  fs.writeFileSync(path.join(out, `lattis-${k}-simple.svg`), lattisMark(LATTIS_PALETTES[k], { id: k + "s", simple: true }));
}

// Qt's SVG parser warns about feComponentTransfer's feFunc children; the same alpha scale as a feColorMatrix doesn't.
const rail = lattisMark(LATTIS_PALETTES.ember, { id: "rail", simple: true, flatRim: 0.45 })
  .replace('viewBox="0 0 512 512"', 'viewBox="24 24 464 464"')
  .replace('<feComponentTransfer><feFuncA type="linear" slope=".38"/></feComponentTransfer>', '<feColorMatrix type="matrix" values="1 0 0 0 0 0 1 0 0 0 0 0 1 0 0 0 0 0 .38 0"/>');
fs.writeFileSync(path.join(here, "../../resources/logo/lattis-ember-simple.svg"), rail);

const template = fs.readFileSync(path.join(here, "template.html"), "utf8");
const mark = fs.readFileSync(path.join(here, "mark.js"), "utf8");
fs.writeFileSync(path.join(out, "lattis-identity.html"), template.split("/*MARK*/").join(mark));

console.log(`Wrote ${out}`);

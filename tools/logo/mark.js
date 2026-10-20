// Lattis mark generator: the eight-petal lattice on a glossy squircle.
// lattisMark(palette, opts) -> SVG string, viewBox 0 0 512 512.
var LATTIS_PALETTES = {
  ember:  { name: "Ember",  top: "#8b2f17", mid: "#b8441c", bottom: "#e67a3d", rim: "#ffd9c2", glyphTop: "#ffffff", glyphBottom: "#f4d8ca", shade: "#7a2c12", stage: "#3b1409", ground: "#120d0c" },
  indigo: { name: "Indigo", top: "#141a62", mid: "#2c3796", bottom: "#5d6ce3", rim: "#d2d7ff", glyphTop: "#ffffff", glyphBottom: "#d4d7f0", shade: "#1e2675", stage: "#0b1d68", ground: "#0e0f14" },
  azure:  { name: "Azure",  top: "#094479", mid: "#266ec6", bottom: "#38a1fa", rim: "#cfe8ff", glyphTop: "#ffffff", glyphBottom: "#d3e6f8", shade: "#0b3f7e", stage: "#062a5c", ground: "#0b0f14" }
};

function lattisMark(p, opts) {
  opts = opts || {};
  var id = opts.id || "lm" + Math.random().toString(36).slice(2, 8);
  var simple = !!opts.simple;
  var L = Object.assign({ body: true, glyph: true, sheen: true, rim: true, bevel: true, shadow: true, halo: true }, opts.layers || {});
  if (simple) { L.sheen = false; L.halo = false; L.shadow = false; L.bevel = false; }

  // Superellipse squircle, inset 24 so the halo and drop shadow fit.
  function squircle(inset, n) {
    var c = 256, a = 256 - inset, pts = [];
    for (var i = 0; i < 160; i++) {
      var t = (i / 160) * Math.PI * 2, ct = Math.cos(t), st = Math.sin(t);
      var x = c + a * Math.sign(ct) * Math.pow(Math.abs(ct), 2 / n);
      var y = c + a * Math.sign(st) * Math.pow(Math.abs(st), 2 / n);
      pts.push(x.toFixed(2) + "," + y.toFixed(2));
    }
    return "M" + pts.join("L") + "Z";
  }
  var body = squircle(24, 4.2);
  var rimPath = opts.flatRim ? squircle(28, 4.2) : squircle(25.5, 4.2);
  var rimStroke = opts.flatRim ? p.rim + '" stroke-opacity="' + opts.flatRim : "url(#" + id + "r)";

  // Petals: lens shapes. Axis petals point at the centre; corner petals lie on the diagonals.
  var s = 0.92;
  function lens(cx, cy, h, w, a) {
    h *= s; w *= s; cx = 256 + (cx - 256) * s; cy = 256 + (cy - 256) * s;
    var r = ((h * h + w * w) / (2 * w)).toFixed(2);
    return '<path transform="translate(' + cx.toFixed(2) + " " + cy.toFixed(2) + ") rotate(" + a + ')" d="M0,' + (-h).toFixed(2) +
      "A" + r + " " + r + " 0 0 1 0," + h.toFixed(2) + "A" + r + " " + r + " 0 0 1 0," + (-h).toFixed(2) + 'Z"/>';
  }
  var petals = [
    lens(256, 162, 67, 35, 0), lens(256, 350, 67, 35, 0),
    lens(162, 256, 67, 35, 90), lens(350, 256, 67, 35, 90),
    lens(148, 148, 60, 30, -45), lens(364, 364, 60, 30, -45),
    lens(364, 148, 60, 30, 45), lens(148, 364, 60, 30, 45)
  ].join("");

  var d = [];
  d.push('<linearGradient id="' + id + 'b" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="' + p.top + '"/><stop offset=".3" stop-color="' + p.mid + '"/><stop offset=".82" stop-color="' + p.bottom + '"/></linearGradient>');
  d.push('<radialGradient id="' + id + 's" cx=".5" cy="1.05" r=".75"><stop offset="0" stop-color="' + p.rim + '" stop-opacity=".3"/><stop offset="1" stop-color="' + p.rim + '" stop-opacity="0"/></radialGradient>');
  d.push('<linearGradient id="' + id + 'r" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="' + p.rim + '" stop-opacity=".75"/><stop offset=".45" stop-color="' + p.rim + '" stop-opacity=".22"/><stop offset="1" stop-color="' + p.rim + '" stop-opacity=".95"/></linearGradient>');
  d.push('<linearGradient id="' + id + 'g" gradientUnits="userSpaceOnUse" x1="0" y1="100" x2="0" y2="412"><stop offset="0" stop-color="' + p.glyphTop + '"/><stop offset="1" stop-color="' + p.glyphBottom + '"/></linearGradient>');
  d.push('<clipPath id="' + id + 'cb"><path d="' + body + '"/></clipPath>');
  d.push('<clipPath id="' + id + 'cg">' + petals + "</clipPath>");
  d.push('<filter id="' + id + 'fb" x="-20%" y="-20%" width="140%" height="140%"><feGaussianBlur stdDeviation="9"/></filter>');
  d.push('<filter id="' + id + 'fs" x="-20%" y="-20%" width="140%" height="160%"><feGaussianBlur in="SourceAlpha" stdDeviation="6"/><feOffset dy="7"/><feComponentTransfer><feFuncA type="linear" slope=".38"/></feComponentTransfer><feMerge><feMergeNode/><feMergeNode in="SourceGraphic"/></feMerge></filter>');
  d.push('<filter id="' + id + 'fi" x="-10%" y="-10%" width="120%" height="120%"><feGaussianBlur stdDeviation="4"/></filter>');

  var g = [];
  if (L.halo) g.push('<path d="' + body + '" fill="' + p.rim + '" opacity=".22" filter="url(#' + id + 'fb)"/>');
  if (L.body) g.push('<path d="' + body + '" fill="url(#' + id + 'b)"/>');
  if (L.sheen) g.push('<path d="' + body + '" fill="url(#' + id + 's)"/>');
  if (L.rim) {
    if (!simple) g.push('<g clip-path="url(#' + id + 'cb)"><path d="' + body + '" fill="none" stroke="' + p.rim + '" stroke-width="26" opacity=".28" filter="url(#' + id + 'fb)"/></g>');
    g.push('<path d="' + rimPath + '" fill="none" stroke="' + rimStroke + '" stroke-width="' + (simple ? 8 : 3) + '"/>');
  }
  if (L.glyph) g.push('<g fill="url(#' + id + 'g)"' + (L.shadow ? ' filter="url(#' + id + 'fs)"' : "") + ">" + petals + "</g>");
  if (L.glyph && L.bevel) {
    g.push('<g clip-path="url(#' + id + 'cg)" fill="none">' +
      '<g stroke="' + p.shade + '" stroke-width="14" opacity=".13" filter="url(#' + id + 'fi)">' + petals + "</g>" +
      '<g stroke="#ffffff" stroke-width="6">' + petals + "</g></g>");
  }

  return '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512"' + (opts.attrs ? " " + opts.attrs : "") + "><defs>" + d.join("") + "</defs>" + g.join("") + "</svg>";
}

if (typeof module !== "undefined") module.exports = { lattisMark: lattisMark, LATTIS_PALETTES: LATTIS_PALETTES };

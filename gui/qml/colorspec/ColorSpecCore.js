.pragma library

// Shared core of the configurable color (see ColorSpecButton.qml): a fixed color, or one a host picks by
// `src` (theme color, gradient stop …), adjusted in OKLCH (luminosity, chroma, hue offset) and in opacity.
// Stored as JSON {src, color, l, c, h, a}; plain colors ("#rrggbb", "#aarrggbb", KConfig "r,g,b[,a]") read as
// fixed colors, and `h` (hue) and `sys` (a source's own choice, see ColorSpecButton) are left out while unset, so older settings keep working.
// A color can belong to a functional group: `grp` (name), `role`, `rp` (the role's parameter), `eqm` / `eq` (the
// equation's color model and expressions); see ColorHarmony.js. They are kept here and left out while `grp` is empty.
// Colors are {r, g, b, a} in 0..1. Hosts keep their own SOURCES list and baseOf(); the rest lives here.

function clamp(v, lo, hi) {
    return Math.max(lo, Math.min(hi, v));
}

// {r, g, b, a} in 0..1 from "#rrggbb", "#aarrggbb" (Qt) or "r,g,b[,a]" (KConfig, 0..255); null if unknown
function parseColor(str) {
    var s = String(str || "").trim();
    var m = s.match(/^(\d+)\s*,\s*(\d+)\s*,\s*(\d+)(?:\s*,\s*(\d+))?$/);
    if (m)
        return { r: m[1] / 255, g: m[2] / 255, b: m[3] / 255, a: m[4] === undefined ? 1 : m[4] / 255 };
    var h = s.replace(/^#/, "");
    if (/^[0-9a-f]{6}$/i.test(h))
        return { r: parseInt(h.substr(0, 2), 16) / 255, g: parseInt(h.substr(2, 2), 16) / 255, b: parseInt(h.substr(4, 2), 16) / 255, a: 1 };
    if (/^[0-9a-f]{8}$/i.test(h))
        return { r: parseInt(h.substr(2, 2), 16) / 255, g: parseInt(h.substr(4, 2), 16) / 255, b: parseInt(h.substr(6, 2), 16) / 255,
                 a: parseInt(h.substr(0, 2), 16) / 255 };
    return null;
}

function hexOf(c) {
    function x(v) { var s = Math.round(clamp(v, 0, 1) * 255).toString(16); return s.length < 2 ? "0" + s : s; }
    return "#" + x(c.a === undefined ? 1 : c.a) + x(c.r) + x(c.g) + x(c.b);
}

function css(c) {
    return "rgba(" + Math.round(c.r * 255) + "," + Math.round(c.g * 255) + "," + Math.round(c.b * 255) + "," + (c.a === undefined ? 1 : c.a).toFixed(3) + ")";
}

function defaults() {
    return { src: "fixed", color: { r: 1, g: 1, b: 1, a: 1 }, l: 0, c: 0, h: 0, a: 100, sys: "",
             grp: "", role: "primary", rp: null, eqm: "rgb", eq: [] };
}

// `sources`: the ids the host accepts (others read as "fixed")
function parse(str, sources) {
    var spec = defaults();
    var s = String(str || "").trim();
    if (s.charAt(0) === "{") {
        try {
            var o = JSON.parse(s);
            spec.src = sources.indexOf(o.src) >= 0 ? o.src : "fixed";
            spec.color = parseColor(o.color) || spec.color;
            spec.sys = typeof o.sys === "string" ? o.sys : "";
            spec.l = clamp(Number(o.l) || 0, -100, 100);
            spec.c = clamp(Number(o.c) || 0, -100, 100);
            spec.h = clamp(Number(o.h) || 0, -180, 180);
            spec.a = clamp(isNaN(Number(o.a)) ? 100 : Number(o.a), 0, 100);
            spec.grp = typeof o.grp === "string" ? o.grp.trim() : "";
            spec.role = typeof o.role === "string" && o.role ? o.role : "primary";
            spec.rp = o.rp === undefined || o.rp === null || isNaN(Number(o.rp)) ? null : Number(o.rp);
            spec.eqm = typeof o.eqm === "string" && o.eqm ? o.eqm : "rgb";
            spec.eq = Array.isArray(o.eq) ? o.eq.slice(0, 3).map(function (x) { return String(x); }) : [];
            return spec;
        } catch (e) {}
    }
    spec.color = parseColor(s) || spec.color;
    return spec;
}

// A plain color when nothing else is set, so simple settings stay readable in the config file
function stringify(spec) {
    if (spec.src === "fixed" && !spec.l && !spec.c && !spec.h && spec.a >= 100 && !spec.grp)
        return hexOf(spec.color);
    var o = { src: spec.src, color: hexOf(spec.color) };
    if (spec.sys)
        o.sys = spec.sys;
    o.l = Math.round(spec.l);
    o.c = Math.round(spec.c);
    if (spec.h)
        o.h = Math.round(spec.h);
    o.a = Math.round(spec.a);
    if (spec.grp) {
        o.grp = spec.grp;
        o.role = spec.role || "primary";
        if (spec.rp !== null && spec.rp !== undefined)
            o.rp = Math.round(spec.rp * 100) / 100;
        if (spec.role === "equation") {
            o.eqm = spec.eqm || "rgb";
            o.eq = (spec.eq || []).slice(0, 3);
        }
    }
    return JSON.stringify(o);
}

function toLinear(v) { return v <= 0.04045 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4); }
function toGamma(v) { return v <= 0.0031308 ? 12.92 * v : 1.055 * Math.pow(v, 1 / 2.4) - 0.055; }

function rgbToOklab(c) {
    var r = toLinear(c.r), g = toLinear(c.g), b = toLinear(c.b);
    var l = Math.cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
    var m = Math.cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
    var s = Math.cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
    return { L: 0.2104542553 * l + 0.7936177850 * m - 0.0040720420 * s,
             a: 1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
             b: 0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s };
}

// Linear (unclamped) sRGB from OKLab
function oklabToLinear(L, A, B) {
    var l = Math.pow(L + 0.3963377774 * A + 0.2158037573 * B, 3);
    var m = Math.pow(L - 0.1055613458 * A - 0.0638541728 * B, 3);
    var s = Math.pow(L - 0.0894841775 * A - 1.2914855480 * B, 3);
    return [4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
            -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
            -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s];
}

// OKLab -> sRGB; out-of-gamut colors are clipped per channel, like browsers render them
function oklabToRgb(L, A, B, alpha) {
    var rgb = oklabToLinear(L, A, B);
    return { r: toGamma(clamp(rgb[0], 0, 1)), g: toGamma(clamp(rgb[1], 0, 1)), b: toGamma(clamp(rgb[2], 0, 1)), a: alpha };
}

// OKLCH (lightness 0..1, chroma ~0..0.4, hue in degrees) -> sRGB. Chroma is lowered until the color fits
// the sRGB gamut, so the hue and lightness stay true (the most vivid color that can be shown)
function oklch(L, C, hueDeg, alpha) {
    var hr = hueDeg * Math.PI / 180, cs = Math.cos(hr), sn = Math.sin(hr);
    L = clamp(L, 0, 1);
    function fits(c) {
        var rgb = oklabToLinear(L, c * cs, c * sn);
        return rgb[0] >= -0.0005 && rgb[0] <= 1.0005 && rgb[1] >= -0.0005 && rgb[1] <= 1.0005 && rgb[2] >= -0.0005 && rgb[2] <= 1.0005;
    }
    var lo = 0, hi = Math.max(0, C);
    if (!fits(hi)) {
        for (var i = 0; i < 18; ++i) {
            var mid = (lo + hi) / 2;
            if (fits(mid)) lo = mid; else hi = mid;
        }
        hi = lo;
    }
    return oklabToRgb(L, hi * cs, hi * sn, alpha === undefined ? 1 : alpha);
}

// A vivid color for an entity, from the settings `v` = { by, lightness, chroma, hueStart, hueRange }
// (lightness and chroma 0..100, angles in degrees) and where the entity is: `ctx` = { index, count, x, y }
// with x and y (0..1) its center on the screen. `by`: 0 index, 1 horizontal, 2 vertical, 3 diagonal position.
function vivid(v, ctx) {
    var by = Number(v.by) || 0;
    var t = by === 1 ? ctx.x : by === 2 ? ctx.y : by === 3 ? (ctx.x + ctx.y) / 2
          : (ctx.count > 1 ? ctx.index / ctx.count : 0);
    return oklch(clamp(Number(v.lightness), 0, 100) / 100, clamp(Number(v.chroma), 0, 100) / 100 * 0.4,
                 Number(v.hueStart) + Number(v.hueRange) * clamp(t, 0, 1), 1);
}

// `c` adjusted in OKLCH, alpha kept. Luminosity (-100..100) moves lightness toward 0 or 1; chroma (-100..100)
// scales it from gray to double (fading out toward the lightness ends, so ±100 luminosity give pure black /
// white); hue (-180..180 degrees) rotates the hue.
function shade(c, luminosity, chroma, hue) {
    var t = clamp(luminosity || 0, -100, 100) / 100;
    var k = (1 + clamp(chroma || 0, -100, 100) / 100) * (1 - t * t);
    var lab = rgbToOklab(c);
    var L = t < 0 ? lab.L * (1 + t) : lab.L + (1 - lab.L) * t;
    var hr = clamp(hue || 0, -180, 180) * Math.PI / 180;
    var cs = Math.cos(hr), sn = Math.sin(hr);
    return oklabToRgb(L, k * (lab.a * cs - lab.b * sn), k * (lab.a * sn + lab.b * cs), c.a === undefined ? 1 : c.a);
}

// The color a spec stands for, given its base (the color before adjustments)
function apply(spec, base) {
    var out = spec.l || spec.c || spec.h ? shade(base, spec.l, spec.c, spec.h) : base;
    return { r: out.r, g: out.g, b: out.b, a: clamp((base.a === undefined ? 1 : base.a) * spec.a / 100, 0, 1) };
}

// "L+20 C−10 H+30" for the adjustments that are set
function adjustments(s) {
    function part(name, v) { return name + (v > 0 ? "+" : "−") + Math.abs(v); }
    var adj = [];
    if (s.l) adj.push(part("L", s.l));
    if (s.c) adj.push(part("C", s.c));
    if (s.h) adj.push(part("H", s.h));
    return adj.join(" ");
}

// OKLCH (L 0..1, C ~0..0.4, hue in degrees) -> sRGB. Out-of-gamut colors keep their lightness and hue and
// lose chroma until they fit, so vivid colors stay as vivid as the display allows instead of shifting hue.
function fromOklch(L, C, hueDeg, alpha) {
    var hr = hueDeg * Math.PI / 180;
    function lin(c) {
        var A = c * Math.cos(hr), B = c * Math.sin(hr);
        var l = Math.pow(L + 0.3963377774 * A + 0.2158037573 * B, 3);
        var m = Math.pow(L - 0.1055613458 * A - 0.0638541728 * B, 3);
        var s = Math.pow(L - 0.0894841775 * A - 1.2914855480 * B, 3);
        return [4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
                -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
                -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s];
    }
    function inGamut(v) { return v[0] >= -0.0005 && v[0] <= 1.0005 && v[1] >= -0.0005 && v[1] <= 1.0005 && v[2] >= -0.0005 && v[2] <= 1.0005; }
    var c = Math.max(0, C), v = lin(c);
    if (!inGamut(v)) {
        var lo = 0, hi = c;
        for (var i = 0; i < 16; ++i) {
            var mid = (lo + hi) / 2;
            if (inGamut(lin(mid))) lo = mid; else hi = mid;
        }
        v = lin(lo);
    }
    return { r: toGamma(clamp(v[0], 0, 1)), g: toGamma(clamp(v[1], 0, 1)), b: toGamma(clamp(v[2], 0, 1)), a: alpha === undefined ? 1 : alpha };
}

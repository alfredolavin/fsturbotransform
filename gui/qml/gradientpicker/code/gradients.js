.pragma library

// Default gradients (user supplied). Any CSS containing linear-gradient()/radial-gradient() works;
// the name comes from a preceding /* comment */, ".selector {" or "Name:" label.
var defaultCss = "/* --- Original 20 --- */\n.cherry-blossom { --degradado: linear-gradient(180deg, oklch(80% 0.25 350), oklch(45% 0.25 350)); }\n.cyber-blue { --degradado: linear-gradient(180deg, oklch(85% 0.15 230), oklch(40% 0.25 260)); }\n.golden-hour { --degradado: linear-gradient(180deg, oklch(90% 0.2 90), oklch(55% 0.2 40)); }\n.toxic-neon { --degradado: linear-gradient(180deg, oklch(85% 0.25 130), oklch(45% 0.2 150)); }\n.amethyst { --degradado: linear-gradient(180deg, oklch(80% 0.2 300), oklch(40% 0.25 310)); }\n.sunset-glow { --degradado: linear-gradient(180deg, oklch(80% 0.2 45), oklch(45% 0.25 350)); }\n.ocean-breeze { --degradado: linear-gradient(180deg, oklch(85% 0.15 200), oklch(40% 0.18 250)); }\n.mango-tango { --degradado: linear-gradient(180deg, oklch(85% 0.22 80), oklch(50% 0.25 30)); }\n.grape-jelly { --degradado: linear-gradient(180deg, oklch(70% 0.25 315), oklch(35% 0.2 290)); }\n.mint-fresh { --degradado: linear-gradient(180deg, oklch(90% 0.15 160), oklch(45% 0.15 200)); }\n.lava-flow { --degradado: linear-gradient(180deg, oklch(75% 0.25 35), oklch(35% 0.2 15)); }\n.galactic-void { --degradado: linear-gradient(180deg, oklch(70% 0.2 280), oklch(30% 0.18 260)); }\n.peach-fuzz { --degradado: linear-gradient(180deg, oklch(85% 0.15 60), oklch(45% 0.2 15)); }\n.wild-berry { --degradado: linear-gradient(180deg, oklch(75% 0.25 340), oklch(35% 0.2 320)); }\n.emerald-city { --degradado: linear-gradient(180deg, oklch(80% 0.2 140), oklch(40% 0.18 145)); }\n.bubblegum-pop { --degradado: linear-gradient(180deg, oklch(80% 0.2 340), oklch(45% 0.15 220)); }\n.electric-indigo { --degradado: linear-gradient(180deg, oklch(75% 0.2 270), oklch(35% 0.25 285)); }\n.lemon-lime { --degradado: linear-gradient(180deg, oklch(95% 0.2 100), oklch(50% 0.2 135)); }\n.coral-reef { --degradado: linear-gradient(180deg, oklch(80% 0.2 30), oklch(40% 0.2 10)); }\n.aurora-borealis { --degradado: linear-gradient(180deg, oklch(85% 0.15 140), oklch(40% 0.25 300)); }\n\n/* --- New 20 --- */\n.rose-gold { --degradado: linear-gradient(180deg, oklch(85% 0.15 45), oklch(45% 0.15 25)); }\n.midnight-forest { --degradado: linear-gradient(180deg, oklch(80% 0.15 160), oklch(35% 0.15 155)); }\n.blood-moon { --degradado: linear-gradient(180deg, oklch(75% 0.25 25), oklch(30% 0.22 15)); }\n.sapphire-depths { --degradado: linear-gradient(180deg, oklch(85% 0.15 240), oklch(35% 0.2 255)); }\n.cosmic-dust { --degradado: linear-gradient(180deg, oklch(85% 0.15 320), oklch(35% 0.2 300)); }\n.honey-gold { --degradado: linear-gradient(180deg, oklch(90% 0.2 95), oklch(45% 0.18 60)); }\n.neon-flamingo { --degradado: linear-gradient(180deg, oklch(80% 0.25 350), oklch(40% 0.25 340)); }\n.ice-dragon { --degradado: linear-gradient(180deg, oklch(90% 0.12 210), oklch(40% 0.15 240)); }\n.poison-apple { --degradado: linear-gradient(180deg, oklch(75% 0.25 20), oklch(30% 0.2 350)); }\n.solar-flare { --degradado: linear-gradient(180deg, oklch(95% 0.25 90), oklch(45% 0.25 35)); }\n.velvet-plum { --degradado: linear-gradient(180deg, oklch(75% 0.25 325), oklch(30% 0.2 310)); }\n.siren-song { --degradado: linear-gradient(180deg, oklch(85% 0.15 180), oklch(35% 0.15 190)); }\n.tiger-lily { --degradado: linear-gradient(180deg, oklch(85% 0.25 60), oklch(40% 0.2 40)); }\n.cyber-magenta { --degradado: linear-gradient(180deg, oklch(80% 0.28 330), oklch(35% 0.25 320)); }\n.steel-glacier { --degradado: linear-gradient(180deg, oklch(90% 0.08 250), oklch(40% 0.12 260)); }\n.supernova { --degradado: linear-gradient(180deg, oklch(90% 0.15 45), oklch(35% 0.25 310)); }\n.dragon-scale { --degradado: linear-gradient(180deg, oklch(85% 0.22 125), oklch(35% 0.15 145)); }\n.cobalt-strike { --degradado: linear-gradient(180deg, oklch(80% 0.2 260), oklch(30% 0.25 270)); }\n.fairy-floss { --degradado: linear-gradient(180deg, oklch(85% 0.18 340), oklch(40% 0.2 280)); }\n.molten-core { --degradado: linear-gradient(180deg, oklch(95% 0.2 100), oklch(35% 0.25 20)); }\n\n.sun-burst { --degradado: linear-gradient(180deg, oklch(95% 0.25 90), oklch(60% 0.25 70)); }\n.lemon-zest { --degradado: linear-gradient(180deg, oklch(95% 0.2 100), oklch(60% 0.2 80)); }\n.canary-glow { --degradado: linear-gradient(180deg, oklch(95% 0.22 95), oklch(65% 0.2 60)); }\n.honey-drip { --degradado: linear-gradient(180deg, oklch(90% 0.2 90), oklch(50% 0.2 50)); }\n.daffodil-beam { --degradado: linear-gradient(180deg, oklch(95% 0.18 105), oklch(65% 0.15 75)); }\n\n.lime-shock { --degradado: linear-gradient(180deg, oklch(95% 0.25 130), oklch(50% 0.22 140)); }\n.jade-glow { --degradado: linear-gradient(180deg, oklch(85% 0.2 150), oklch(45% 0.18 160)); }\n.acid-mint { --degradado: linear-gradient(180deg, oklch(90% 0.22 140), oklch(50% 0.2 170)); }\n.pine-flash { --degradado: linear-gradient(180deg, oklch(80% 0.2 145), oklch(35% 0.2 145)); }\n.chartreuse-fire { --degradado: linear-gradient(180deg, oklch(95% 0.28 120), oklch(55% 0.25 130)); }\n.li-subtotal { --degradado: linear-gradient(in oklch 0deg, #ff9800, #d81b60); }\n.pack-post-descuento { --degradado: linear-gradient(in oklch 0deg, #cddc39, #4caf50); }\n.codigo-abono { --degradado: linear-gradient(in oklch 181deg, #0074d1, #00fdff); }\n";

var named = {
    black: [0, 0, 0], white: [255, 255, 255], red: [255, 0, 0], green: [0, 128, 0], lime: [0, 255, 0],
    blue: [0, 0, 255], yellow: [255, 255, 0], cyan: [0, 255, 255], aqua: [0, 255, 255],
    magenta: [255, 0, 255], fuchsia: [255, 0, 255], orange: [255, 165, 0], purple: [128, 0, 128],
    pink: [255, 192, 203], gold: [255, 215, 0], violet: [238, 130, 238], indigo: [75, 0, 130],
    teal: [0, 128, 128], navy: [0, 0, 128], gray: [128, 128, 128], grey: [128, 128, 128],
    silver: [192, 192, 192], maroon: [128, 0, 0], olive: [128, 128, 0], coral: [255, 127, 80],
    tomato: [255, 99, 71], turquoise: [64, 224, 208], hotpink: [255, 105, 180], deeppink: [255, 20, 147],
    crimson: [220, 20, 60], orchid: [218, 112, 214], salmon: [250, 128, 114], skyblue: [135, 206, 235],
    springgreen: [0, 255, 127], chartreuse: [127, 255, 0], royalblue: [65, 105, 225],
    dodgerblue: [30, 144, 255], darkorange: [255, 140, 0], lawngreen: [124, 252, 0]
};

function clamp(v, lo, hi) {
    return Math.min(hi, Math.max(lo, v));
}

function num(s, scale) {
    s = s.trim();
    if (s.slice(-1) === "%")
        return parseFloat(s) / 100 * scale;
    return parseFloat(s);
}

function hue(s) {
    s = s.trim();
    var v = parseFloat(s);
    if (/turn$/.test(s)) return v * 360;
    if (/grad$/.test(s)) return v * 0.9;
    if (/rad$/.test(s)) return v * 180 / Math.PI;
    return v;
}

function hslToRgb(h, s, l) {
    h = ((h % 360) + 360) % 360 / 360;
    function f(n) {
        var k = (n + h * 12) % 12;
        var a = s * Math.min(l, 1 - l);
        return l - a * Math.max(-1, Math.min(k - 3, 9 - k, 1));
    }
    return [f(0), f(8), f(4)];
}

// ---- OKLab / OKLCH (https://bottosson.github.io/posts/oklab/) ----

function toLinear(v) { return v <= 0.04045 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4); }
function toGamma(v) { return v <= 0.0031308 ? 12.92 * v : 1.055 * Math.pow(v, 1 / 2.4) - 0.055; }

function rgbToOklab(c) {
    var r = toLinear(c.r), g = toLinear(c.g), b = toLinear(c.b);
    var l = Math.cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
    var m = Math.cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
    var s = Math.cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
    return {
        L: 0.2104542553 * l + 0.7936177850 * m - 0.0040720420 * s,
        a: 1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
        b: 0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s
    };
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

// OKLCH -> sRGB. Out-of-gamut colors are clipped per channel, like browsers render them
function oklchToRgb(L, C, H, alpha) {
    var hr = H * Math.PI / 180;
    var rgb = oklabToLinear(clamp(L, 0, 1), C * Math.cos(hr), C * Math.sin(hr));
    return { r: toGamma(clamp(rgb[0], 0, 1)), g: toGamma(clamp(rgb[1], 0, 1)), b: toGamma(clamp(rgb[2], 0, 1)), a: alpha };
}

function oklabToRgb(L, A, B, alpha) {
    var C = Math.sqrt(A * A + B * B);
    return oklchToRgb(L, C, Math.atan2(B, A) * 180 / Math.PI, alpha);
}

// Returns {r, g, b, a} in 0..1 (plus `lab` for OKLab/OKLCH inputs), or null
function parseColor(str) {
    var s = str.trim().toLowerCase();
    var m;
    if (s === "transparent")
        return { r: 0, g: 0, b: 0, a: 0 };
    if ((m = s.match(/^#([0-9a-f]{3,8})$/))) {
        var h = m[1];
        if (h.length === 3 || h.length === 4)
            h = h.split("").map(function (c) { return c + c; }).join("");
        if (h.length !== 6 && h.length !== 8)
            return null;
        return {
            r: parseInt(h.substr(0, 2), 16) / 255, g: parseInt(h.substr(2, 2), 16) / 255,
            b: parseInt(h.substr(4, 2), 16) / 255, a: h.length === 8 ? parseInt(h.substr(6, 2), 16) / 255 : 1
        };
    }
    if ((m = s.match(/^(rgba?|hsla?|oklch|oklab)\((.*)\)$/))) {
        var args = m[2].replace("/", " ").split(/[\s,]+/).filter(function (x) { return x.length; });
        if (args.length < 3)
            return null;
        var a = args.length > 3 ? clamp(num(args[3], 1), 0, 1) : 1;
        var c;
        if (m[1] === "oklch") {
            c = oklchToRgb(num(args[0], 1), num(args[1], 0.4), hue(args[2]), a);
            c.modern = true;
            return c;
        }
        if (m[1] === "oklab") {
            c = oklabToRgb(num(args[0], 1), num(args[1], 0.4), num(args[2], 0.4), a);
            c.modern = true;
            return c;
        }
        var rgb = m[1][0] === "r"
            ? [num(args[0], 255) / 255, num(args[1], 255) / 255, num(args[2], 255) / 255]
            : hslToRgb(hue(args[0]), num(args[1], 1), num(args[2], 1));
        return { r: clamp(rgb[0], 0, 1), g: clamp(rgb[1], 0, 1), b: clamp(rgb[2], 0, 1), a: a };
    }
    if (named[s])
        return { r: named[s][0] / 255, g: named[s][1] / 255, b: named[s][2] / 255, a: 1 };
    return null;
}

function css(c) {
    return "rgba(" + Math.round(c.r * 255) + "," + Math.round(c.g * 255) + "," + Math.round(c.b * 255) + "," + c.a.toFixed(3) + ")";
}

// Split on commas that are not inside parentheses
function splitTop(s) {
    var out = [], depth = 0, cur = "";
    for (var i = 0; i < s.length; ++i) {
        var ch = s[i];
        if (ch === "(") depth++;
        if (ch === ")") depth--;
        if (ch === "," && depth === 0) { out.push(cur); cur = ""; } else cur += ch;
    }
    if (cur.trim().length)
        out.push(cur);
    return out;
}

function titleCase(s) {
    return s.replace(/^[.#]/, "").replace(/[-_]+/g, " ").trim().replace(/\b\w/g, function (c) { return c.toUpperCase(); });
}

// The name is whatever naming construct comes last before the gradient:
// a /* comment */, a ".selector {" or a "Name:" label (CSS properties are ignored)
function guessName(context, index) {
    var best = -1, name = "", m, re;
    re = /\/\*([\s\S]*?)\*\//g;
    while ((m = re.exec(context))) if (m.index > best) { best = m.index; name = m[1].replace(/^[-\s]+|[-\s]+$/g, ""); }
    // selectors and labels are only looked for outside comments
    var bare = context.replace(/\/\*[\s\S]*?\*\//g, function (c) { return c.replace(/[^\n]/g, " "); });
    re = /([.#]?[A-Za-z_][\w-]*)\s*\{/g;
    while ((m = re.exec(bare))) if (m.index > best) { best = m.index; name = titleCase(m[1]); }
    re = /(^|[;{}\n\/])\s*([A-Za-z][\w ]*?)\s*:/g;
    while ((m = re.exec(bare))) {
        if (/^(background(-image|-color)?|border(-image)?|fill|color)$/i.test(m[2])) continue;
        if (m.index > best) { best = m.index; name = m[2].trim(); }
    }
    return name || ("Gradient " + (index + 1));
}

var spaces = ["srgb", "oklab", "oklch"];
var hueModes = ["shorter", "longer", "increasing", "decreasing"];

// Hue difference from H1 to H2 following a CSS hue interpolation method
function hueDelta(H1, H2, mode) {
    var d = ((H2 - H1) % 360 + 360) % 360; // 0..360
    if (mode === "longer") return d > 0 && d < 180 ? d - 360 : d === 0 ? 360 : d;
    if (mode === "increasing") return d;
    if (mode === "decreasing") return d > 0 ? d - 360 : d;
    return d > 180 ? d - 360 : d;
}

// Interpolate between two colors in sRGB, OKLab or OKLCH
function interpolate(c1, c2, t, space, hueMode) {
    if (space === "srgb")
        return mix(c1, c2, t);
    var p = rgbToOklab(c1), q = rgbToOklab(c2);
    var alpha = c1.a + (c2.a - c1.a) * t;
    if (space === "oklch") {
        var C1 = Math.hypot(p.a, p.b), C2 = Math.hypot(q.a, q.b);
        var H1 = Math.atan2(p.b, p.a) * 180 / Math.PI, H2 = Math.atan2(q.b, q.a) * 180 / Math.PI;
        var gray = C1 < 1e-4 || C2 < 1e-4; // powerless hue: take the other one
        if (C1 < 1e-4) H1 = H2;
        if (C2 < 1e-4) H2 = H1;
        var dh = gray ? 0 : hueDelta(H1, H2, hueMode);
        return oklchToRgb(p.L + (q.L - p.L) * t, C1 + (C2 - C1) * t, H1 + dh * t, alpha);
    }
    return oklabToRgb(p.L + (q.L - p.L) * t, p.a + (q.a - p.a) * t, p.b + (q.b - p.b) * t, alpha);
}

// CSS color hint: `mid` is where (0..1 of the segment) the colors mix 50/50
function ease(t, mid) {
    mid = clamp(mid, 0.001, 0.999);
    return Math.abs(mid - 0.5) < 1e-4 ? t : Math.pow(t, Math.log(0.5) / Math.log(mid));
}

function isCurved(mid) {
    return mid !== undefined && Math.abs(mid - 0.5) > 1e-3;
}

// Parse CSS text into editable definitions:
// [{name, space: "srgb"|"oklab"|"oklch", hue: "shorter"|"longer"|"increasing"|"decreasing",
//   stops: [{pos: 0..1, color: "<css color>", mid: 0..1}]}]
// `mid` is the color hint of the segment that ends at that stop (0.5 = linear).
function parseDefs(text) {
    var result = [];
    var re = /(?:repeating-)?(?:linear|radial|conic)-gradient\(/gi;
    var m, prevEnd = 0;
    text = text || "";
    while ((m = re.exec(text))) {
        var start = re.lastIndex, depth = 1, i = start;
        while (i < text.length && depth > 0) {
            if (text[i] === "(") depth++;
            else if (text[i] === ")") depth--;
            i++;
        }
        var body = text.substring(start, i - 1);
        var context = text.substring(prevEnd, m.index);
        prevEnd = i;
        re.lastIndex = i;

        var args = splitTop(body);
        var stops = [];
        var space = "srgb", hueMode = "shorter";
        var hint = args.length ? args[0].match(/\bin\s+(oklch|oklab|srgb|hsl|lch|lab)\b(?:\s+(shorter|longer|increasing|decreasing)\s+hue\b)?/i) : null;
        args.forEach(function (arg) {
            var a = arg.trim();
            if (/^-?[\d.]+%$/.test(a)) { // color hint between two stops
                if (stops.length)
                    stops[stops.length - 1].hint = parseFloat(a) / 100;
                return;
            }
            var cm = a.match(/^((?:rgba?|hsla?|oklch|oklab)\([^)]*\)|#[0-9a-fA-F]+|[a-zA-Z]+)\s*(.*)$/);
            if (!cm) return;
            var c = parseColor(cm[1]);
            if (!c) return; // angle, "to right", "in oklch", shape, etc.
            if (c.modern) space = "oklab"; // CSS interpolates modern colors in OKLab by default
            var positions = cm[2].split(/\s+/).filter(function (p) { return /%$/.test(p); });
            if (!positions.length)
                stops.push({ pos: NaN, color: cm[1] });
            positions.forEach(function (p) { stops.push({ pos: parseFloat(p) / 100, color: cm[1] }); });
        });
        if (hint) {
            space = /lch|hsl/i.test(hint[1]) ? "oklch" : hint[1].toLowerCase() === "srgb" ? "srgb" : "oklab";
            if (hint[2])
                hueMode = hint[2].toLowerCase();
        }
        if (stops.length === 0)
            continue;
        if (stops.length === 1)
            stops.push({ pos: 1, color: stops[0].color });
        if (isNaN(stops[0].pos)) stops[0].pos = 0;
        if (isNaN(stops[stops.length - 1].pos)) stops[stops.length - 1].pos = 1;
        // distribute stops without a position evenly between their neighbours
        for (var k = 1; k < stops.length - 1; ++k) {
            if (!isNaN(stops[k].pos)) continue;
            var j = k;
            while (isNaN(stops[j].pos)) j++;
            var from = stops[k - 1].pos, to = stops[j].pos;
            for (var n = k; n < j; ++n)
                stops[n].pos = from + (to - from) * (n - k + 1) / (j - k + 1);
        }
        var out = [];
        stops.forEach(function (s, idx) {
            var pos = clamp(s.pos, 0, 1);
            if (idx > 0) pos = Math.max(pos, out[idx - 1].pos); // CSS never goes backwards
            var mid = 0.5, prev = idx > 0 ? stops[idx - 1] : null;
            if (prev && prev.hint !== undefined && pos > out[idx - 1].pos)
                mid = Math.round(clamp((prev.hint - out[idx - 1].pos) / (pos - out[idx - 1].pos), 0, 1) * 1000) / 1000;
            out.push({ pos: pos, color: s.color, mid: mid });
        });
        result.push({ name: guessName(context, result.length), space: space, hue: hueMode, stops: out });
    }
    return result;
}

// Stops sorted by position (stable), with parsed colors
function sortedStops(def) {
    return def.stops.map(function (s, i) {
        return { pos: clamp(s.pos, 0, 1), color: parseColor(s.color) || { r: 0, g: 0, b: 0, a: 1 }, mid: s.mid === undefined ? 0.5 : s.mid, i: i };
    }).sort(function (a, b) { return a.pos - b.pos || a.i - b.i; });
}

// Definition -> {name, stops: [{pos, color, css}]} ready for Canvas, which interpolates in sRGB:
// intermediate stops are added for OKLab/OKLCH segments and curved (hinted) ones.
function compile(def) {
    var src = sortedStops(def);
    if (!src.length)
        return { name: def.name, stops: [] };
    if (src.length === 1)
        src.push({ pos: 1, color: src[0].color, mid: 0.5 });
    var out = [src[0]];
    for (var i = 1; i < src.length; ++i) {
        var a = src[i - 1], b = src[i];
        var curved = isCurved(b.mid);
        if (b.pos > a.pos && (def.space !== "srgb" || curved)) {
            var N = curved ? 24 : 12;
            for (var k = 1; k < N; ++k)
                out.push({ pos: a.pos + (b.pos - a.pos) * k / N,
                           color: interpolate(a.color, b.color, ease(k / N, b.mid), def.space, def.hue) });
        }
        out.push(b);
    }
    return {
        name: def.name,
        stops: out.map(function (s) { return { pos: s.pos, color: s.color, css: css(s.color) }; })
    };
}

// Parse CSS text into [{name, stops: [{pos, color, css}]}]
function parse(text) {
    return parseDefs(text).map(compile);
}

var defaultDefs = parseDefs(defaultCss);

// ---- writing gradients back as CSS ----

function hex2(v) {
    var s = Math.round(clamp(v, 0, 1) * 255).toString(16);
    return s.length < 2 ? "0" + s : s;
}

// {r, g, b, a} -> "#rrggbb" or "#rrggbbaa" (CSS order)
function hex(c) {
    return "#" + hex2(c.r) + hex2(c.g) + hex2(c.b) + (c.a < 0.999 ? hex2(c.a) : "");
}

function pct(v) {
    return (Math.round(v * 1000) / 10) + "%";
}

function safeName(name) {
    return (name || "").replace(/\*\//g, "* /").replace(/[\r\n]+/g, " ").trim();
}

function stringify(def) {
    var head = "in " + def.space + (def.space === "oklch" && def.hue && def.hue !== "shorter" ? " " + def.hue + " hue" : "") + " 90deg";
    var args = [head];
    var s = def.stops.slice().sort(function (a, b) { return a.pos - b.pos; });
    s.forEach(function (st, i) {
        if (i > 0 && isCurved(st.mid))
            args.push(pct(s[i - 1].pos + (st.pos - s[i - 1].pos) * st.mid));
        args.push(st.color.trim() + " " + pct(st.pos));
    });
    return "linear-gradient(" + args.join(", ") + ")";
}

function serialize(defs) {
    return defs.map(function (d) { return "/* " + safeName(d.name) + " */\n" + stringify(d) + ";\n"; }).join("\n");
}

function uniqueName(names, base) {
    base = safeName(base) || "Gradient";
    if (names.indexOf(base) < 0)
        return base;
    var root = base.replace(/\s+\d+$/, "");
    for (var n = 2; ; ++n)
        if (names.indexOf(root + " " + n) < 0)
            return root + " " + n;
}

// ---- color editing helpers ----

function round(v, d) {
    var f = Math.pow(10, d);
    return Math.round(v * f) / f;
}

// CSS color text -> {L: 0..1, C, H: 0..360, a}. oklch() is read as written so
// out-of-gamut values survive editing; anything else is converted.
function toOklch(str) {
    var s = (str || "").trim().toLowerCase(), m = s.match(/^oklch\((.*)\)$/);
    if (m) {
        var args = m[1].replace("/", " ").split(/[\s,]+/).filter(function (x) { return x.length; });
        if (args.length >= 3)
            return { L: clamp(num(args[0], 1), 0, 1), C: Math.max(0, num(args[1], 0.4)), H: ((hue(args[2]) % 360) + 360) % 360,
                     a: args.length > 3 ? clamp(num(args[3], 1), 0, 1) : 1 };
    }
    var c = parseColor(s) || { r: 0, g: 0, b: 0, a: 1 };
    var lab = rgbToOklab(c), C = Math.hypot(lab.a, lab.b);
    return { L: clamp(lab.L, 0, 1), C: C, H: C < 1e-4 ? 0 : ((Math.atan2(lab.b, lab.a) * 180 / Math.PI) + 360) % 360, a: c.a };
}

function oklchCss(L, C, H, a) {
    return "oklch(" + round(L * 100, 1) + "% " + round(C, 3) + " " + round(H, 1) + (a < 0.999 ? " / " + round(a, 3) : "") + ")";
}

function rgbToHsl(c) {
    var max = Math.max(c.r, c.g, c.b), min = Math.min(c.r, c.g, c.b), l = (max + min) / 2, d = max - min, h = 0, s = 0;
    if (d > 1e-6) {
        s = d / (1 - Math.abs(2 * l - 1));
        h = max === c.r ? ((c.g - c.b) / d) % 6 : max === c.g ? (c.b - c.r) / d + 2 : (c.r - c.g) / d + 4;
        h = (h * 60 + 360) % 360;
    }
    return { h: h, s: s, l: l };
}

// Rewrite a CSS color in another notation: "hex", "rgb", "hsl" or "oklch"
function formatColor(str, fmt) {
    if (fmt === "oklch") {
        var o = toOklch(str);
        return oklchCss(o.L, o.C, o.H, o.a);
    }
    var c = parseColor(str) || { r: 0, g: 0, b: 0, a: 1 };
    var alpha = c.a < 0.999 ? " / " + round(c.a, 3) : "";
    if (fmt === "rgb")
        return "rgb(" + Math.round(c.r * 255) + " " + Math.round(c.g * 255) + " " + Math.round(c.b * 255) + alpha + ")";
    if (fmt === "hsl") {
        var h = rgbToHsl(c);
        return "hsl(" + round(h.h, 1) + "deg " + round(h.s * 100, 1) + "% " + round(h.l * 100, 1) + "%" + alpha + ")";
    }
    return hex(c);
}

// Stops for a slider background showing one OKLCH channel ("L", "C", "H" or "a") swept 0..max
function channelRamp(lch, channel, max) {
    var out = [];
    for (var i = 0; i <= 16; ++i) {
        var v = max * i / 16;
        var c = oklchToRgb(channel === "L" ? v : lch.L, channel === "C" ? v : lch.C, channel === "H" ? v : lch.H, channel === "a" ? v : 1);
        out.push({ pos: i / 16, color: c, css: css(c) });
    }
    return out;
}

// Color of the gradient at pos as "#rrggbb[aa]"
function sample(def, pos) {
    return hex(colorAt(compile(def).stops, pos));
}

function find(list, name) {
    for (var i = 0; i < list.length; ++i)
        if (list[i].name === name)
            return list[i];
    return list.length ? list[0] : { name: "", stops: [{ pos: 0, color: { r: .24, g: .68, b: .91, a: 1 }, css: "#3daee9" }, { pos: 1, color: { r: .24, g: .68, b: .91, a: 1 }, css: "#3daee9" }] };
}

function mix(a, b, t) {
    return { r: a.r + (b.r - a.r) * t, g: a.g + (b.g - a.g) * t, b: a.b + (b.b - a.b) * t, a: a.a + (b.a - a.a) * t };
}

// `c` adjusted in OKLCH, alpha kept. Luminosity (-100..100) moves lightness toward 0 or 1; chroma (-100..100)
// scales it from gray to double. Chroma fades out toward the lightness ends, so -100 / 100 give pure black / white.
function shade(c, luminosity, chroma) {
    var t = clamp(luminosity || 0, -100, 100) / 100;
    var k = (1 + clamp(chroma || 0, -100, 100) / 100) * (1 - t * t);
    var lab = rgbToOklab(c);
    var L = t < 0 ? lab.L * (1 + t) : lab.L + (1 - lab.L) * t;
    return oklabToRgb(L, lab.a * k, lab.b * k, c.a === undefined ? 1 : c.a);
}

function colorAt(stops, pos) {
    if (!stops.length) return { r: 0, g: 0, b: 0, a: 0 };
    if (pos <= stops[0].pos) return stops[0].color;
    for (var i = 1; i < stops.length; ++i) {
        if (pos <= stops[i].pos) {
            var span = stops[i].pos - stops[i - 1].pos;
            return mix(stops[i - 1].color, stops[i].color, span > 0 ? (pos - stops[i - 1].pos) / span : 1);
        }
    }
    return stops[stops.length - 1].color;
}

// Paint `top` (with alpha) over opaque `bottom`
function over(top, bottom) {
    var a = top.a;
    return { r: top.r * a + bottom.r * (1 - a), g: top.g * a + bottom.g * (1 - a), b: top.b * a + bottom.b * (1 - a), a: 1 };
}

function luminance(c) {
    function lin(v) { return v <= 0.03928 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4); }
    return 0.2126 * lin(c.r) + 0.7152 * lin(c.g) + 0.0722 * lin(c.b);
}

// true when black text reads better than white on this color
function prefersDark(c) {
    var L = luminance(c);
    return (L + 0.05) / 0.05 > 1.05 / (L + 0.05);
}

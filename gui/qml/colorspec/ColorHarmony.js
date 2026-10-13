.pragma library
.import "ColorSpecCore.js" as Core
.import "ColorExpr.js" as Expr

// Functional color groups: configurable colors (ColorSpecCore.js) that share a group name (`grp`) are computed
// from the group's primary color (and its secondary one) according to their `role`:
//   primary, secondary     their own color (source + adjustments), the inputs of the group
//   equal                  exactly the primary (only the opacity of its own spec applies)
//   complement, analogous, split, triad, triad2, square, square2
//                          hue harmonies, rotated in OKLCH (lightness and chroma kept) and brought into sRGB with
//                          the CSS Color 4 gamut mapping (chroma reduced until the clipped color is within a JND)
//   inverse                RGB inverse of the primary
//   tone                   a tone (0 black … 100 white, perceptually even: Ottosson's toe-corrected OKLab lightness,
//                          the L of OKHSL) of the primary's tonal scale, keeping its chroma relative to the most vivid
//                          color the gamut has at that tone and hue, so every tone is as vivid as the primary
//   contrast               the nearest tone of the primary's hue readable on the primary: APCA (SAPC 0.0.98G-4g)
//                          contrast |Lc| ≥ rp, on the side (lighter / darker) that can reach the most contrast
//   mix                    primary → secondary in OKLab, rp % of the way
//   custom                 the primary through the spec's own OKLCH modifiers (luminosity, chroma, hue offset)
//   equation               one expression per channel of a color model (eqm: rgb, hsl, hsv, oklch) over the
//                          channels of the primary (R1 G1 B1 …) and the secondary (R2 G2 B2 …), see ColorExpr.js
// The spec's adjustments (l, c, h) and opacity apply after the role, except for `equal` (opacity only).
// Fields in the spec: grp (group name), role, rp (the role's parameter), eqm (model), eq (three expressions).
// A group without a primary uses each member's own source color as the primary; without a secondary, the
// primary's complement. Hosts call resolveEntries([{key, spec, base}]) with all the colors of a settings page
// (or of the widget, at run time), `base` being each one's source color; ColorGroups.qml does it for buttons.

// [id, name (translated where shown), parameter {label, from, to, step, def, unit, icon}]
var ROLES = [
    { id: "primary", name: "Primary", own: true },
    { id: "secondary", name: "Secondary", own: true },
    { id: "equal", name: "Same as the primary" },
    { id: "complement", name: "Complementary (+180°)" },
    { id: "analogous", name: "Analogous", param: { label: "Hue step:", from: -90, to: 90, step: 5, def: 30, unit: "°", icon: "color-mode-hue-shift-negative" } },
    { id: "split", name: "Split complementary", param: { label: "Split:", from: -90, to: 90, step: 5, def: 30, unit: "°", icon: "color-mode-hue-shift-negative" } },
    { id: "triad", name: "Triadic (+120°)" },
    { id: "triad2", name: "Triadic (−120°)" },
    { id: "square", name: "Square (+90°)" },
    { id: "square2", name: "Square (−90°)" },
    { id: "inverse", name: "Inverse (RGB)" },
    { id: "tone", name: "Tone of the primary", param: { label: "Tone:", from: 0, to: 100, step: 1, def: 90, unit: "", icon: "adjustlevels" } },
    { id: "contrast", name: "Readable on the primary", param: { label: "Contrast (APCA Lc):", from: 15, to: 105, step: 5, def: 75, unit: "", icon: "contrast" } },
    { id: "mix", name: "Mix primary → secondary", param: { label: "Mix:", from: 0, to: 100, step: 5, def: 50, unit: " %", icon: "kipi-expoblending" } },
    { id: "custom", name: "Custom (OKLCH modifiers)" },
    { id: "equation", name: "Equation" }
];

// color models of the equations: channel names, ranges (for the hint) and a short description
var MODELS = [
    { id: "rgb", name: "RGB", ch: ["R", "G", "B"], range: ["0–255", "0–255", "0–255"] },
    { id: "hsl", name: "HSL", ch: ["H", "S", "L"], range: ["0–360°", "0–100", "0–100"] },
    { id: "hsv", name: "HSV", ch: ["H", "S", "V"], range: ["0–360°", "0–100", "0–100"] },
    { id: "oklch", name: "OKLCH", ch: ["L", "C", "H"], range: ["0–100", "0–100 (= 0.4)", "0–360°"] }
];

function roleOf(id) {
    for (var i = 0; i < ROLES.length; ++i)
        if (ROLES[i].id === id)
            return ROLES[i];
    return ROLES[0];
}
function modelOf(id) {
    for (var i = 0; i < MODELS.length; ++i)
        if (MODELS[i].id === id)
            return MODELS[i];
    return MODELS[0];
}
// the expressions that give back the primary in model `m`
function identity(m) {
    return modelOf(m).ch.map(function (c) { return c + "1"; });
}
function paramOf(spec) {
    var p = roleOf(spec.role).param;
    if (!p) return 0;
    return spec.rp === undefined || spec.rp === null || isNaN(Number(spec.rp)) ? p.def : Core.clamp(Number(spec.rp), p.from, p.to);
}

// ---------------------------------------------------------------- OKLCH, gamut, tones

function oklchOf(c) {
    var lab = Core.rgbToOklab(c);
    var C = Math.sqrt(lab.a * lab.a + lab.b * lab.b);
    var H = C < 1e-4 ? 0 : Math.atan2(lab.b, lab.a) * 180 / Math.PI;
    return { L: lab.L, C: C, H: H < 0 ? H + 360 : H };
}

function inGamutLin(v) {
    return v[0] >= -1e-4 && v[0] <= 1.0001 && v[1] >= -1e-4 && v[1] <= 1.0001 && v[2] >= -1e-4 && v[2] <= 1.0001;
}

// CSS Color 4 gamut mapping (OKLCH chroma reduction with a just-noticeable-difference test on the clipped color)
function gamutMap(L, C, H, alpha) {
    var a = alpha === undefined ? 1 : alpha;
    if (L >= 1) return { r: 1, g: 1, b: 1, a: a };
    if (L <= 0) return { r: 0, g: 0, b: 0, a: a };
    var hr = H * Math.PI / 180, cs = Math.cos(hr), sn = Math.sin(hr);
    function lin(c) { return Core.oklabToLinear(L, c * cs, c * sn); }
    function clip(c) { return Core.oklabToRgb(L, c * cs, c * sn, a); }
    function dE(rgb, c) {
        var x = Core.rgbToOklab(rgb);
        var dl = x.L - L, da = x.a - c * cs, db = x.b - c * sn;
        return Math.sqrt(dl * dl + da * da + db * db);
    }
    if (inGamutLin(lin(C)))
        return clip(C);
    var JND = 0.02, eps = 0.0001;
    var clipped = clip(C);
    if (dE(clipped, C) < JND)
        return clipped;
    var lo = 0, hi = C, loInGamut = true;
    while (hi - lo > eps) {
        var mid = (lo + hi) / 2;
        if (loInGamut && inGamutLin(lin(mid))) { lo = mid; continue; }
        clipped = clip(mid);
        var e = dE(clipped, mid);
        if (e < JND) {
            if (JND - e < eps) return clipped;
            loInGamut = false;
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return clipped;
}

// the most vivid chroma sRGB has at lightness L and hue H
function maxChroma(L, H) {
    if (L <= 0 || L >= 1) return 0;
    var hr = H * Math.PI / 180, cs = Math.cos(hr), sn = Math.sin(hr);
    var lo = 0, hi = 0.5;
    for (var i = 0; i < 22; ++i) {
        var mid = (lo + hi) / 2;
        if (inGamutLin(Core.oklabToLinear(L, mid * cs, mid * sn))) lo = mid; else hi = mid;
    }
    return lo;
}

// Ottosson's toe: OKLab L ↔ a lightness that matches CIE L* (perceptually even tones, used by OKHSL)
var K1 = 0.206, K2 = 0.03, K3 = (1 + K1) / (1 + K2);
function toe(x) { return 0.5 * (K3 * x - K1 + Math.sqrt((K3 * x - K1) * (K3 * x - K1) + 4 * K2 * K3 * x)); }
function toeInv(x) { return (x * x + K1 * x) / (K3 * (x + K2)); }

// tone of a color, 0 (black) … 100 (white)
function toneOf(c) {
    return Core.clamp(toe(Core.rgbToOklab(c).L), 0, 1) * 100;
}

// tone `t` (0..100) of the tonal scale of `c`: same hue, same chroma relative to the gamut's maximum
function tone(c, t) {
    var o = oklchOf(c);
    var cmax = maxChroma(o.L, o.H);
    var rel = cmax > 1e-4 ? Core.clamp(o.C / cmax, 0, 1) : 0;
    var L = toeInv(Core.clamp(t, 0, 100) / 100);
    var out = gamutMap(L, rel * maxChroma(L, o.H), o.H, c.a);
    return out;
}

// APCA lightness contrast Lc of `txt` on `bg` (SAPC-APCA 0.0.98G-4g; positive: dark on light)
function apca(txt, bg) {
    function Y(c) {
        var y = 0.2126729 * Math.pow(c.r, 2.4) + 0.7151522 * Math.pow(c.g, 2.4) + 0.0721750 * Math.pow(c.b, 2.4);
        return y < 0.022 ? y + Math.pow(0.022 - y, 1.414) : y;
    }
    var yt = Y(txt), yb = Y(bg);
    if (Math.abs(yb - yt) < 0.0005) return 0;
    var s;
    if (yb > yt) {
        s = (Math.pow(yb, 0.56) - Math.pow(yt, 0.57)) * 1.14;
        return s < 0.1 ? 0 : (s - 0.027) * 100;
    }
    s = (Math.pow(yb, 0.65) - Math.pow(yt, 0.62)) * 1.14;
    return s > -0.1 ? 0 : (s + 0.027) * 100;
}

// the tone of `c`'s scale nearest to it with |Lc| ≥ target on `c`, on the side that can contrast most
function readableOn(c, target) {
    var t0 = toneOf(c);
    function lc(t) { return Math.abs(apca(tone(c, t), c)); }
    var sides = [100, 0].map(function (end) {
        var best = lc(end);
        if (best < target)
            return { t: end, reach: false, max: best };
        var lo = t0, hi = end; // lo fails, hi reaches
        for (var i = 0; i < 18; ++i) {
            var mid = (lo + hi) / 2;
            if (lc(mid) >= target) hi = mid; else lo = mid;
        }
        return { t: hi, reach: true, max: best };
    });
    var side = sides[0].max >= sides[1].max ? sides[0] : sides[1];
    return tone(c, side.t);
}

// ---------------------------------------------------------------- equations

function rgbToHsl(c) {
    var max = Math.max(c.r, c.g, c.b), min = Math.min(c.r, c.g, c.b), d = max - min, L = (max + min) / 2;
    var S = d === 0 ? 0 : d / (1 - Math.abs(2 * L - 1));
    return [hueOf(c, max, d), S * 100, L * 100];
}
function rgbToHsv(c) {
    var max = Math.max(c.r, c.g, c.b), min = Math.min(c.r, c.g, c.b), d = max - min;
    return [hueOf(c, max, d), max === 0 ? 0 : d / max * 100, max * 100];
}
function hueOf(c, max, d) {
    if (d === 0) return 0;
    var h = max === c.r ? ((c.g - c.b) / d) % 6 : max === c.g ? (c.b - c.r) / d + 2 : (c.r - c.g) / d + 4;
    h *= 60;
    return h < 0 ? h + 360 : h;
}
function fromHue(h, chroma, x0, m) {
    var hp = (((h % 360) + 360) % 360) / 60, x = chroma * (1 - Math.abs(hp % 2 - 1)), r = 0, g = 0, b = 0;
    if (hp < 1) { r = chroma; g = x; } else if (hp < 2) { r = x; g = chroma; } else if (hp < 3) { g = chroma; b = x; }
    else if (hp < 4) { g = x; b = chroma; } else if (hp < 5) { r = x; b = chroma; } else { r = chroma; b = x; }
    return { r: r + m, g: g + m, b: b + m };
}
function hslToRgb(h, s, l) {
    s = Core.clamp(s, 0, 100) / 100; l = Core.clamp(l, 0, 100) / 100;
    var ch = (1 - Math.abs(2 * l - 1)) * s;
    return fromHue(h, ch, 0, l - ch / 2);
}
function hsvToRgb(h, s, v) {
    s = Core.clamp(s, 0, 100) / 100; v = Core.clamp(v, 0, 100) / 100;
    var ch = v * s;
    return fromHue(h, ch, 0, v - ch);
}

// channel values of `c` in model `m` (the units of MODELS)
function channels(m, c) {
    if (m === "hsl") return rgbToHsl(c);
    if (m === "hsv") return rgbToHsv(c);
    if (m === "oklch") { var o = oklchOf(c); return [o.L * 100, o.C / 0.4 * 100, o.H]; }
    return [c.r * 255, c.g * 255, c.b * 255];
}

// the variables an equation sees: <channel>1 of the primary, <channel>2 of the secondary, A1 / A2 their alpha
function variables(m, P, S) {
    var ch = modelOf(m).ch, env = {}, p = channels(m, P), s = channels(m, S);
    for (var i = 0; i < 3; ++i) {
        env[ch[i] + "1"] = p[i];
        env[ch[i] + "2"] = s[i];
    }
    env.A1 = P.a === undefined ? 1 : P.a;
    env.A2 = S.a === undefined ? 1 : S.a;
    return env;
}

// {color, values: [3 numbers], errors: [3 × null | {error, pos}]}
function equation(m, exprs, P, S) {
    var env = variables(m, P, S), id = identity(m), v = [], errors = [];
    for (var i = 0; i < 3; ++i) {
        var src = exprs && typeof exprs[i] === "string" && exprs[i].trim() !== "" ? exprs[i] : id[i];
        var r = Expr.evaluate(src, env);
        errors.push(r.error ? r : null);
        v.push(r.error ? env[id[i]] : r.value);
    }
    var c, a = P.a === undefined ? 1 : P.a;
    if (m === "hsl") c = hslToRgb(v[0], v[1], v[2]);
    else if (m === "hsv") c = hsvToRgb(v[0], v[1], v[2]);
    else if (m === "oklch") c = gamutMap(Core.clamp(v[0], 0, 100) / 100, Math.max(0, v[1]) / 100 * 0.4, ((v[2] % 360) + 360) % 360);
    else c = { r: Core.clamp(v[0], 0, 255) / 255, g: Core.clamp(v[1], 0, 255) / 255, b: Core.clamp(v[2], 0, 255) / 255 };
    return { color: { r: c.r, g: c.g, b: c.b, a: a }, values: v, errors: errors };
}

// ---------------------------------------------------------------- roles

function rotate(c, deg) {
    var o = oklchOf(c);
    return gamutMap(o.L, o.C, (o.H + deg + 360) % 360, c.a);
}

// The color a role gives before the spec's own adjustments: {color, errors?}
function derive(spec, P, S) {
    var p = paramOf(spec);
    switch (spec.role) {
    case "equal": case "custom": return { color: P };
    case "complement": return { color: rotate(P, 180) };
    case "analogous": return { color: rotate(P, p) };
    case "split": return { color: rotate(P, 180 + p) };
    case "triad": return { color: rotate(P, 120) };
    case "triad2": return { color: rotate(P, -120) };
    case "square": return { color: rotate(P, 90) };
    case "square2": return { color: rotate(P, -90) };
    case "inverse": return { color: { r: 1 - P.r, g: 1 - P.g, b: 1 - P.b, a: P.a } };
    case "tone": return { color: tone(P, p) };
    case "contrast": return { color: readableOn(P, p) };
    case "mix": {
        var a = Core.rgbToOklab(P), b = Core.rgbToOklab(S), t = p / 100;
        var pa = P.a === undefined ? 1 : P.a, sa = S.a === undefined ? 1 : S.a;
        return { color: Core.oklabToRgb(a.L + (b.L - a.L) * t, a.a + (b.a - a.a) * t, a.b + (b.b - a.b) * t, pa + (sa - pa) * t) };
    }
    case "equation": {
        var r = equation(spec.eqm || "rgb", spec.eq, P, S);
        return { color: r.color, errors: r.errors, values: r.values };
    }
    }
    return { color: P };
}

function finish(spec, base) {
    if (spec.role === "equal")
        return { r: base.r, g: base.g, b: base.b, a: Core.clamp((base.a === undefined ? 1 : base.a) * spec.a / 100, 0, 1) };
    return Core.apply(spec, base);
}

// All the colors of a page: [{key, spec, base}] → {key: {color, base, P, S, primaryKey, secondaryKey, errors,
// conflict}}. `base` of an entry is its own source color; in the result it is the color the role gave.
function resolveEntries(list) {
    var out = {}, groups = {}, order = [];
    for (var i = 0; i < list.length; ++i) {
        var e = list[i];
        if (!e.spec.grp) {
            out[e.key] = { color: Core.apply(e.spec, e.base), base: e.base };
            continue;
        }
        if (!groups[e.spec.grp]) { groups[e.spec.grp] = []; order.push(e.spec.grp); }
        groups[e.spec.grp].push(e);
    }
    order.forEach(function (g) {
        var members = groups[g], prim = null, sec = null;
        members.forEach(function (e) {
            if (e.spec.role === "primary" && !prim) prim = e;
            if (e.spec.role === "secondary" && !sec) sec = e;
        });
        var P = prim ? Core.apply(prim.spec, prim.base) : null;
        var S = sec ? Core.apply(sec.spec, sec.base) : null;
        members.forEach(function (e) {
            var role = roleOf(e.spec.role);
            if (role.own) {
                out[e.key] = { color: Core.apply(e.spec, e.base), base: e.base, P: P, S: S,
                               primaryKey: prim ? prim.key : "", secondaryKey: sec ? sec.key : "",
                               conflict: e !== prim && e !== sec };
                return;
            }
            var p = P || Core.apply(Object.assign({}, e.spec, { l: 0, c: 0, h: 0, a: 100 }), e.base);
            var s = S || rotate(p, 180);
            var d = derive(e.spec, p, s);
            out[e.key] = { color: finish(e.spec, d.color), base: d.color, P: p, S: s, errors: d.errors, values: d.values,
                           primaryKey: prim ? prim.key : "", secondaryKey: sec ? sec.key : "" };
        });
    });
    return out;
}

// group names used in `list`, sorted
function groupNames(list) {
    var seen = {};
    list.forEach(function (e) { if (e.spec.grp) seen[e.spec.grp] = true; });
    return Object.keys(seen).sort();
}

// A ready-made scheme for a primary (for hosts that want to fill a group at once): role ids by use
function scheme(P) {
    return {
        primary: P, complement: rotate(P, 180), analogous: [rotate(P, -30), rotate(P, 30)],
        triad: [rotate(P, 120), rotate(P, -120)], split: [rotate(P, 150), rotate(P, 210)],
        tones: [0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 95, 99, 100].map(function (t) { return { tone: t, color: tone(P, t) }; }),
        on: readableOn(P, 75)
    };
}

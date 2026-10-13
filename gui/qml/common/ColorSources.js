.pragma library
.import "../colorspec/ColorSpecCore.js" as Core
.import "../gradientpicker/code/gradients.js" as Gradients

// The sources most widgets offer for a configurable color (colorspec/): a system color of the Plasma color scheme
// ("system", the key stored as `sys`) and the active gradient's begin, end or current fill. Library code cannot see
// Kirigami.Theme: callers pass `theme`, SystemTheme.map, so system colors follow the color scheme live.
// Widget-specific sources (e.g. "best contrast with the background") go in the widget's own baseColor before these.

// [Kirigami.Theme property, label] (labels are translated where shown: i18n(e[1]))
var SYSTEM = [
    ["textColor", "Text"],
    ["backgroundColor", "Background"],
    ["alternateBackgroundColor", "Alternate background"],
    ["highlightColor", "Highlight"],
    ["highlightedTextColor", "Highlighted text"],
    ["focusColor", "Focus ring"],
    ["hoverColor", "Hover"],
    ["linkColor", "Link"],
    ["visitedLinkColor", "Visited link"],
    ["activeTextColor", "Active text"],
    ["disabledTextColor", "Disabled text"],
    ["positiveTextColor", "Positive text"],
    ["neutralTextColor", "Neutral text"],
    ["negativeTextColor", "Negative text"]
];

var IDS = ["fixed", "system", "begin", "end", "current"];

function systemKey(k) {
    for (var i = 0; i < SYSTEM.length; ++i)
        if (SYSTEM[i][0] === k)
            return k;
    return "textColor";
}

// The color a source stands for, before adjustments: {r, g, b, a}
function baseOf(spec, stops, progress, theme) {
    if (spec.src === "system") {
        var t = theme && theme[systemKey(spec.sys)];
        return t ? { r: t.r, g: t.g, b: t.b, a: t.a === undefined ? 1 : t.a } : { r: 1, g: 1, b: 1, a: 1 };
    }
    if (spec.src === "fixed" || !stops || !stops.length)
        return spec.color;
    var g = Gradients.colorAt(stops, spec.src === "begin" ? 0 : spec.src === "end" ? 1 : Core.clamp(progress, 0, 1));
    return { r: g.r, g: g.g, b: g.b, a: 1 };
}

// A stored string → the final color {r, g, b, a}; an empty string is `fallback` (a spec string, e.g. the theme text)
function resolve(str, stops, progress, theme, fallback) {
    var s = String(str || "") || fallback || '{"src":"system","sys":"textColor"}';
    var spec = Core.parse(s, IDS);
    return Core.apply(spec, baseOf(spec, stops, progress, theme));
}

// For QML: Qt.rgba(...) of resolve()
function css(c) {
    return Core.css(c);
}

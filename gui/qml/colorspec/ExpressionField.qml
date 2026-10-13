import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

import "ColorExpr.js" as Expr

// One channel's expression of an "Equation" color (ColorHarmony.js), with syntax colouring: variables of the
// primary (…1) and of the secondary (…2), functions, constants, numbers; an unknown name or the place of a syntax
// error is marked. The text is drawn by a read-only rich-text copy laid over the (transparent) editor with the same
// font and wrapping, so the cursor and selection stay the editor's. Inside, at the left edge, a badge with the
// channel letter tinted like the channel; at the right the value it gives now.
//   channel  "R", "H" …;  vars  the variable names it may use;  error / errorPos  from ColorExpr.evaluate()
//   valueText  the current value ("= 127")
QQC2.TextArea {
    id: field

    property string channel: "R"
    property var vars: []
    property string error: ""
    property int errorPos: -1
    property string valueText: ""

    readonly property var swatchStops: ({
        R: ["#3a0000", "#ff2020"], G: ["#003a00", "#20d020"], B: ["#00003a", "#2060ff"],
        S: ["#808080", "#e02090"], L: ["#000000", "#ffffff"], V: ["#000000", "#ffd040"], C: ["#808080", "#00b0c0"]
    })

    font.family: "Fira Code"
    font.features: { "liga": 0, "calt": 0 }
    font.pointSize: Kirigami.Theme.defaultFont.pointSize
    wrapMode: TextEdit.Wrap
    textFormat: TextEdit.PlainText
    color: "transparent"
    selectedTextColor: "transparent"
    leftPadding: 28
    rightPadding: valueLabel.implicitWidth + 12
    implicitWidth: Kirigami.Units.gridUnit * 18
    Accessible.name: i18nc("%1 is a color channel (R, H, L …)", "Expression for %1", channel)

    QQC2.ToolTip.text: error !== "" ? error : i18n("Math with the variables %1 and the functions of JavaScript's Math (sin, pow, min, max …), clamp, mix, step, smoothstep, mod; PI, E, TAU; comparisons and ?:", vars.join(" "))
    QQC2.ToolTip.visible: hovered
    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

    function esc(s) {
        return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
    }
    function rich(src) {
        const T = Kirigami.Theme;
        const col = {
            var1: T.linkColor, var2: T.visitedLinkColor, func: T.positiveTextColor, num: T.neutralTextColor,
            cnst: T.activeTextColor, op: T.textColor, bad: T.negativeTextColor
        };
        let html = "";
        for (const t of Expr.tokens(src)) {
            let c = col.op, bold = false, bad = t.type === "error";
            if (t.type === "num")
                c = col.num;
            else if (t.type === "ident") {
                const k = Expr.identKind(t.text, vars);
                if (k === "var") { c = /2$/.test(t.text) ? col.var2 : col.var1; bold = true; }
                else if (k === "func") c = col.func;
                else if (k === "const") c = col.cnst;
                else bad = true;
            }
            if (errorPos >= 0 && t.pos === errorPos && t.type !== "space")
                bad = true;
            if (bad)
                c = col.bad;
            let s = '<span style="color:' + c + (bad ? ';background-color:' + Qt.rgba(c.r, c.g, c.b, 0.18) : '') + '">' + esc(t.text) + "</span>";
            html += bold ? "<b>" + s + "</b>" : s;
        }
        return '<span style="white-space:pre-wrap">' + html + "</span>";
    }

    background: Rectangle {
        radius: Kirigami.Units.cornerRadius
        color: Kirigami.Theme.backgroundColor
        border.width: field.activeFocus || field.error !== "" ? 1.5 : 1
        border.color: field.error !== "" ? Kirigami.Theme.negativeTextColor
                    : field.activeFocus ? Kirigami.Theme.focusColor
                    : Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.25)
    }

    // the coloured text, exactly over the editor's own
    TextEdit {
        x: field.leftPadding
        y: field.topPadding
        width: field.width - field.leftPadding - field.rightPadding
        readOnly: true
        enabled: false
        activeFocusOnPress: false
        font: field.font
        wrapMode: TextEdit.Wrap
        textFormat: TextEdit.RichText
        text: field.rich(field.text)
        opacity: field.enabled ? 1 : 0.5
    }

    FontMetrics {
        id: fm
        font: field.font
    }

    // channel badge, on the first line: white letter with a black outline over the channel's colours
    Rectangle {
        x: 6
        y: field.topPadding + (fm.height - 16) / 2
        width: 16
        height: 16
        radius: 4
        opacity: field.enabled ? 1 : 0.45
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: field.channel === "H" ? "#ff3030" : (field.swatchStops[field.channel] || ["#444", "#aaa"])[0] }
            GradientStop { position: 0.33; color: field.channel === "H" ? "#30ff30" : Qt.tint((field.swatchStops[field.channel] || ["#444", "#aaa"])[0], Qt.rgba(1, 1, 1, 0)) }
            GradientStop { position: 0.66; color: field.channel === "H" ? "#3060ff" : (field.swatchStops[field.channel] || ["#444", "#aaa"])[1] }
            GradientStop { position: 1; color: field.channel === "H" ? "#ff30c0" : (field.swatchStops[field.channel] || ["#444", "#aaa"])[1] }
        }
        border.width: 1
        border.color: Qt.rgba(0, 0, 0, 0.35)
        Text {
            anchors.centerIn: parent
            text: field.channel
            color: "white"
            style: Text.Outline
            styleColor: "black"
            font.bold: true
            font.pixelSize: 11
        }
    }

    QQC2.Label {
        id: valueLabel
        anchors.right: parent.right
        anchors.rightMargin: 6
        y: field.topPadding + (fm.height - height) / 2
        text: field.error !== "" ? "⚠" : field.valueText
        color: field.error !== "" ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
        opacity: field.error !== "" ? 1 : 0.7
        font.family: field.font.family
    }
}

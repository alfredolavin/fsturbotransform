import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

import "ColorSpecCore.js" as Core
import "ColorHarmony.js" as Harmony

// The tonal scale of a group's primary, white (100) at the top to black (0) at the bottom (ColorHarmony.tone():
// perceptually even tones, same relative chroma), with a mark at the tone of every color of the group: the
// primary's labelled 1, the secondary's 2, the color being edited highlighted. Next to GroupWheel.
//   base     {r, g, b, a}, the primary (whose scale is drawn)
//   members  as GroupWheel's
Item {
    id: strip

    property var base: null
    property var members: []

    implicitWidth: Kirigami.Units.gridUnit * 2.6
    implicitHeight: Kirigami.Units.gridUnit * 7

    readonly property real barX: 2
    readonly property real barWidth: Kirigami.Units.gridUnit * 0.9

    onBaseChanged: canvas.requestPaint()
    onMembersChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            if (!strip.base)
                return;
            const top = 4, h = height - 8, x = strip.barX, w = strip.barWidth;
            const g = ctx.createLinearGradient(0, top, 0, top + h);
            for (let i = 0; i <= 20; ++i)
                g.addColorStop(i / 20, Core.css(Harmony.tone(strip.base, 100 - i * 5)));
            ctx.beginPath();
            ctx.roundedRect(x, top, w, h, 3, 3);
            ctx.fillStyle = g;
            ctx.fill();
            ctx.lineWidth = 1;
            ctx.strokeStyle = Qt.rgba(0, 0, 0, 0.3);
            ctx.stroke();
            for (const m of strip.members) {
                const y = top + h * (1 - Harmony.toneOf(m.color) / 100);
                const sel = m.self;
                ctx.lineWidth = sel ? 2 : 1;
                ctx.strokeStyle = sel ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor;
                ctx.beginPath();
                ctx.moveTo(x + w + 1, y);
                ctx.lineTo(x + w + 7, y);
                ctx.stroke();
                ctx.beginPath();
                ctx.arc(x + w + 11, y, sel ? 4.5 : 3.5, 0, 2 * Math.PI);
                ctx.fillStyle = Core.css({ r: m.color.r, g: m.color.g, b: m.color.b, a: 1 });
                ctx.fill();
                ctx.strokeStyle = sel ? Kirigami.Theme.highlightColor : Qt.rgba(0, 0, 0, 0.6);
                ctx.stroke();
                if (m.role === "primary" || m.role === "secondary") {
                    ctx.font = "bold 10px sans-serif";
                    ctx.textBaseline = "middle";
                    ctx.lineWidth = 2.5;
                    ctx.strokeStyle = "black";
                    ctx.strokeText(m.role === "primary" ? "1" : "2", x + w + 17, y);
                    ctx.fillStyle = "white";
                    ctx.fillText(m.role === "primary" ? "1" : "2", x + w + 17, y);
                }
            }
        }
    }
    QQC2.ToolTip.text: i18n("Tonal scale of the primary color (0 black, 100 white) and the tone of each color of the group")
    QQC2.ToolTip.visible: tipArea.containsMouse
    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
    MouseArea {
        id: tipArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
    }
}

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

import "ColorSpecCore.js" as Core
import "ColorHarmony.js" as Harmony

// The colors of a group on an OKLCH hue wheel: the angle is the hue, the distance from the centre the chroma (gray
// in the middle), the ring the hues at a mid tone. Lines join the primary to every other color, so the wheel shows
// where each one sits relative to it; the primary is marked 1, the secondary 2, the color being edited has a ring.
// `mini` draws a small one without marks (inside a button). Hovering a dot (full size) tells its name and role.
//   members  [{key, label, role, roleName, color: {r, g, b, a}, self}]
Item {
    id: wheel

    property var members: []
    property bool mini: false

    implicitWidth: mini ? Kirigami.Units.gridUnit * 1.2 : Kirigami.Units.gridUnit * 7
    implicitHeight: implicitWidth

    readonly property real radius: Math.min(width, height) / 2 - (mini ? 0.5 : 2)
    readonly property real ring: mini ? Math.max(2, radius * 0.24) : radius * 0.13
    readonly property real inner: radius - ring - (mini ? 1 : 4)
    readonly property var primary: members.find(m => m.role === "primary") || null

    function place(c) {
        const o = Harmony.oklchOf(c);
        const r = inner * Math.min(1, Math.sqrt(o.C / 0.32));
        const a = -o.H * Math.PI / 180;
        return { x: width / 2 + r * Math.cos(a), y: height / 2 + r * Math.sin(a), o: o };
    }
    function tip(m) {
        const parts = [m.label || m.key, m.roleName || m.role];
        if (primary && m !== primary) {
            const p = Harmony.oklchOf(primary.color), o = Harmony.oklchOf(m.color);
            let dh = Math.round(((o.H - p.H + 540) % 360) - 180);
            parts.push(i18nc("hue difference from the group's primary color, in degrees", "hue %1°", (dh > 0 ? "+" : "") + dh));
            parts.push(i18nc("tone of the primary → tone of this color", "tone %1 → %2",
                             Math.round(Harmony.toneOf(primary.color)), Math.round(Harmony.toneOf(m.color))));
        }
        return parts.join(" · ");
    }

    onMembersChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            const cx = width / 2, cy = height / 2, R = wheel.radius, ring = wheel.ring;
            if (R <= 0)
                return;
            // hue ring, at a mid tone and moderate chroma so every hue is shown with the same weight
            const n = wheel.mini ? 36 : 72;
            for (let i = 0; i < n; ++i) {
                const h0 = i * 360 / n, h1 = (i + 1.15) * 360 / n;
                ctx.beginPath();
                ctx.arc(cx, cy, R, -h1 * Math.PI / 180, -h0 * Math.PI / 180, false);
                ctx.arc(cx, cy, R - ring, -h0 * Math.PI / 180, -h1 * Math.PI / 180, true);
                ctx.closePath();
                ctx.fillStyle = Core.css(Harmony.gamutMap(0.72, 0.14, (h0 + h1) / 2));
                ctx.fill();
            }
            // chroma circles
            ctx.lineWidth = 1;
            ctx.strokeStyle = Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.15);
            if (!wheel.mini)
                for (const c of [0.08, 0.16, 0.24, 0.32]) {
                    ctx.beginPath();
                    ctx.arc(cx, cy, wheel.inner * Math.sqrt(c / 0.32), 0, 2 * Math.PI);
                    ctx.stroke();
                }
            const pts = wheel.members.map(m => ({ m: m, p: wheel.place(m.color) }));
            const prim = pts.find(e => e.m.role === "primary");
            // lines from the primary
            if (prim) {
                ctx.lineWidth = wheel.mini ? 0.8 : 1.2;
                ctx.strokeStyle = Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, wheel.mini ? 0.55 : 0.45);
                for (const e of pts) {
                    if (e === prim)
                        continue;
                    ctx.beginPath();
                    ctx.moveTo(prim.p.x, prim.p.y);
                    ctx.lineTo(e.p.x, e.p.y);
                    ctx.stroke();
                }
            }
            // dots: the edited color last, so it is on top
            pts.sort((a, b) => (a.m.self ? 1 : 0) - (b.m.self ? 1 : 0));
            for (const e of pts) {
                const big = e.m.role === "primary" ? 1.5 : e.m.role === "secondary" ? 1.25 : 1;
                const r = (wheel.mini ? 1.7 : 4.5) * big;
                if (e.m.self) {
                    ctx.beginPath();
                    ctx.arc(e.p.x, e.p.y, r + (wheel.mini ? 1.6 : 3), 0, 2 * Math.PI);
                    ctx.fillStyle = Kirigami.Theme.highlightColor;
                    ctx.fill();
                }
                ctx.beginPath();
                ctx.arc(e.p.x, e.p.y, r, 0, 2 * Math.PI);
                ctx.fillStyle = Core.css({ r: e.m.color.r, g: e.m.color.g, b: e.m.color.b, a: 1 });
                ctx.fill();
                ctx.lineWidth = wheel.mini ? 0.7 : 1.2;
                ctx.strokeStyle = Harmony.toneOf(e.m.color) > 55 ? "rgba(0,0,0,0.75)" : "rgba(255,255,255,0.9)";
                ctx.stroke();
                if (!wheel.mini && (e.m.role === "primary" || e.m.role === "secondary")) {
                    ctx.font = "bold " + Math.round(r * 1.3) + "px sans-serif";
                    ctx.textAlign = "center";
                    ctx.textBaseline = "middle";
                    ctx.lineWidth = 2.5;
                    ctx.strokeStyle = "black";
                    ctx.strokeText(e.m.role === "primary" ? "1" : "2", e.p.x, e.p.y + 0.5);
                    ctx.fillStyle = "white";
                    ctx.fillText(e.m.role === "primary" ? "1" : "2", e.p.x, e.p.y + 0.5);
                }
            }
        }
    }

    // the name of the dot under the mouse
    MouseArea {
        id: hover
        anchors.fill: parent
        enabled: !wheel.mini
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        property var hit: null
        onPositionChanged: mouse => {
            let best = null, bd = 12 * 12;
            for (const m of wheel.members) {
                const p = wheel.place(m.color), d = (p.x - mouse.x) ** 2 + (p.y - mouse.y) ** 2;
                if (d < bd) { bd = d; best = m; }
            }
            hit = best;
        }
        onExited: hit = null
        QQC2.ToolTip.visible: !!hit
        QQC2.ToolTip.text: hit ? wheel.tip(hit) : ""
    }
}

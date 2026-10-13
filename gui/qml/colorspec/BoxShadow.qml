import QtQuick

// CSS-like box-shadow of the rounded rectangle this item fills, painted only outside it,
// so whatever sits on top can be transparent
Item {
    id: box

    property real radius: 0
    property real blur: 4
    property real offsetX: 0
    property real offsetY: 1
    property color color: Qt.rgba(0, 0, 0, 0.35)

    readonly property int pad: Math.ceil(blur * 1.5 + Math.max(Math.abs(offsetX), Math.abs(offsetY))) + 1

    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()
    onRadiusChanged: canvas.requestPaint()
    onBlurChanged: canvas.requestPaint()
    onOffsetXChanged: canvas.requestPaint()
    onOffsetYChanged: canvas.requestPaint()
    onColorChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        anchors.margins: -box.pad
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            const p = box.pad, w = box.width, h = box.height;
            if (w <= 0 || h <= 0)
                return;
            const r = Math.max(0, Math.min(box.radius, w / 2, h / 2));
            ctx.save();
            // only outside the box
            ctx.beginPath();
            ctx.rect(0, 0, width, height);
            ctx.roundedRect(p, p, w, h, r, r);
            ctx.fillRule = Qt.OddEvenFill;
            ctx.clip();
            // the shape itself is drawn far away; its offset shadow lands back in view
            const far = 10000, c = box.color;
            ctx.shadowColor = "rgba(" + Math.round(c.r * 255) + "," + Math.round(c.g * 255) + "," + Math.round(c.b * 255) + "," + c.a + ")";
            ctx.shadowBlur = box.blur;
            ctx.shadowOffsetX = box.offsetX + far;
            ctx.shadowOffsetY = box.offsetY;
            ctx.translate(-far, 0);
            ctx.beginPath();
            ctx.roundedRect(p, p, w, h, r, r);
            ctx.fillStyle = "black";
            ctx.fill();
            ctx.restore();
        }
    }
}

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

import "ColorSpecCore.js" as Core

// Slider (no tick marks) with a strip above it painting the color each value gives, and the value beside it
RowLayout {
    id: root

    property alias value: slider.value
    property alias from: slider.from
    property alias to: slider.to
    property alias stepSize: slider.stepSize
    // v => {r, g, b, a} for a slider value
    property var colorAt: null
    // change it whenever colorAt would give other colors, to repaint the strip
    property var repaintKey
    property bool checker: false
    property string suffix: ""
    // optional icon of the setting, drawn inside the slider at its left edge (the groove starts after it): a theme
    // icon name ("edit-opacity") or a URL. Same metrics as the Icon* controls (16 px at x 6, 28 reserved), drawn
    // here because colorspec is also used where there is no controls/ directory.
    property var iconSource: ""
    readonly property bool hasIcon: !!iconSource && iconSource.toString() !== ""
    // the value label: explicit "+" for signed ranges, then the suffix (already translated by the host)
    property var format: v => (v > 0 && from < 0 ? "+" : "") + Math.round(v) + suffix
    signal moved

    // a form label lines up with the value, not with the strip above the slider
    baselineOffset: valueLabel.y + valueLabel.baselineOffset

    onRepaintKeyChanged: strip.requestPaint()
    onColorAtChanged: strip.requestPaint()

    ColumnLayout {
        spacing: 2
        Canvas {
            id: strip
            // spans the handle's travel, so each color sits right above its value
            Layout.leftMargin: slider.leftPadding + slider.handle.width / 2
            Layout.preferredWidth: Math.max(1, slider.availableWidth - slider.handle.width)
            Layout.preferredHeight: Kirigami.Units.smallSpacing * 2
            opacity: root.enabled ? 1 : 0.4
            onWidthChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();
                if (!root.colorAt || width <= 0)
                    return;
                if (root.checker)
                    for (let x = 0; x < width; x += 4)
                        for (let y = 0; y < height; y += 4) {
                            ctx.fillStyle = (x + y) % 8 ? "#999999" : "#666666";
                            ctx.fillRect(x, y, 4, 4);
                        }
                const g = ctx.createLinearGradient(0, 0, width, 0);
                const n = 24;
                for (let i = 0; i <= n; ++i) {
                    const c = root.colorAt(slider.from + (slider.to - slider.from) * i / n);
                    g.addColorStop(i / n, Core.css({ r: c.r, g: c.g, b: c.b, a: c.a === undefined ? 1 : c.a }));
                }
                ctx.fillStyle = g;
                ctx.fillRect(0, 0, width, height);
            }
        }
        QQC2.Slider {
            id: slider
            from: 0
            to: 100
            stepSize: 5
            snapMode: QQC2.Slider.SnapAlways
            Kirigami.StyleHints.tickMarkStepSize: -1
            Layout.preferredWidth: Kirigami.Units.gridUnit * 10 + leftInset
            leftInset: root.hasIcon ? 28 : 0
            leftPadding: leftInset
            onMoved: root.moved()

            Kirigami.Icon {
                visible: root.hasIcon
                z: 5
                width: 16
                height: 16
                x: 6
                anchors.verticalCenter: parent.verticalCenter
                source: root.iconSource
                isMask: false
                opacity: root.enabled ? 1 : 0.45
            }
        }
    }
    QQC2.Label {
        id: valueLabel
        Layout.alignment: Qt.AlignBottom
        // the widest value fits, so the slider does not jump while moving
        Layout.minimumWidth: Math.max(Kirigami.Units.gridUnit * 2.5, widestFrom.advanceWidth, widestTo.advanceWidth)
        horizontalAlignment: Text.AlignRight
        text: root.format(slider.value)
        opacity: root.enabled ? 1 : 0.6
        TextMetrics { id: widestFrom; font: valueLabel.font; text: root.format(slider.from) }
        TextMetrics { id: widestTo; font: valueLabel.font; text: root.format(slider.to) }
    }
}

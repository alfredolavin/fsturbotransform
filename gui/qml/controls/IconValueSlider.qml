import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

// An IconSlider with its value beside it, in a label wide enough for the widest value. `format` turns a value into
// the label text (e.g. v => i18n("%1 %", Math.round(v * 100)), or an explicit "+" for signed ranges).
RowLayout {
    id: row

    property alias iconName: slider.iconName
    property alias value: slider.value
    property alias from: slider.from
    property alias to: slider.to
    property alias stepSize: slider.stepSize
    property alias slider: slider
    property var format: v => String(v)
    signal moved()

    spacing: 6

    IconSlider {
        id: slider
        Layout.fillWidth: true
        onMoved: row.moved()
    }
    QQC2.Label {
        id: valueLabel
        text: row.format(slider.value)
        horizontalAlignment: Text.AlignRight
        Layout.minimumWidth: Math.max(widestFrom.advanceWidth, widestTo.advanceWidth)
        TextMetrics { id: widestFrom; font: valueLabel.font; text: row.format(slider.from) }
        TextMetrics { id: widestTo; font: valueLabel.font; text: row.format(slider.to) }
    }
}

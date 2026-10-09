import QtQuick
import QtQuick.Controls as QQC2

// A combo box with its property's icon inside, at the left. org.kde.desktop paints the current
// text at a fixed place (leftPadding does not move it), so the text is our own label at x 28.
QQC2.ComboBox {
    id: combo

    property alias iconName: icon.name
    // The widest entry, so the box is wide enough for every choice.
    property string widestText: ""

    displayText: ""
    implicitWidth: 28 + metrics.advanceWidth + 34
    Accessible.name: current.text

    TextMetrics {
        id: metrics
        font: combo.font
        text: combo.widestText
    }

    PropertyIcon { id: icon }

    QQC2.Label {
        id: current
        x: 28
        width: combo.width - 28 - 34
        anchors.verticalCenter: parent.verticalCenter
        text: combo.currentText
        elide: Text.ElideRight
    }
}

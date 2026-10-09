import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// One figure of the run's report: its icon inside the tile at the left, the value, its label.
Rectangle {
    id: tile

    property string iconName
    property string emblem
    property string label
    property string value
    property color accent: Kirigami.Theme.textColor

    implicitWidth: 28 + 10 + Math.max(valueText.implicitWidth, labelText.implicitWidth) + 12
    implicitHeight: 52
    radius: 10
    color: Qt.alpha(accent, 0.07)
    border.width: 1
    border.color: Qt.alpha(accent, 0.22)

    PropertyIcon {
        name: tile.iconName
        emblem: tile.emblem
        size: 22
        x: 10
    }

    ColumnLayout {
        anchors.left: parent.left
        anchors.leftMargin: 10 + 22 + 10
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        spacing: 0
        Kirigami.Heading {
            id: valueText
            level: 3
            text: tile.value
            font.weight: Font.Bold
            color: tile.accent
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        QQC2.Label {
            id: labelText
            text: tile.label
            opacity: 0.75
            font: Kirigami.Theme.smallFont
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
    }
}

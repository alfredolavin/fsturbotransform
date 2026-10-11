import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// The author's signature: "alfredolavin@gmail.com" in small type, styled as a hyperlink, opening a mail draft.
// Put it in the bottom-right corner of every dialog, window and popup; it stays clear of the buttons and content.
QQC2.Label {
    id: signature
    text: "alfredolavin@gmail.com"
    font.family: Kirigami.Theme.smallFont.family
    font.pointSize: Kirigami.Theme.smallFont.pointSize
    font.underline: true
    color: Kirigami.Theme.linkColor
    opacity: mouse.containsMouse ? 1.0 : 0.85

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: Qt.openUrlExternally("mailto:alfredolavin@gmail.com")
    }
}

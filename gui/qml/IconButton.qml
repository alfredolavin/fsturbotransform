import QtQuick
import QtQuick.Controls as QQC2

// A push button with an icon inside at the left. org.kde.desktop paints `text` and `icon` in its
// native background (centred, under our icon), so the label is the contentItem instead.
QQC2.Button {
    id: button

    property alias iconName: icon.name
    property alias label: caption.text

    leftPadding: 28
    rightPadding: 10
    implicitWidth: leftPadding + caption.implicitWidth + rightPadding
    Accessible.name: caption.text

    contentItem: QQC2.Label {
        id: caption
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignLeft
        opacity: button.enabled ? 1 : 0.5
    }

    PropertyIcon {
        id: icon
        opacity: button.enabled ? 1 : 0.45
    }
}

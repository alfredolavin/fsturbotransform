import QtQuick
import org.kde.kirigami as Kirigami

// The icon of one property, drawn inside its control at the left edge and vertically centred
// (sizes and paddings: ~/.claude/qml-icon-padding.md). `emblem` adds a small second icon at the
// bottom right, for properties that combine two ideas (a renamed folder: folder + pencil).
Image {
    id: root

    property string name
    property string emblem
    property int size: 16

    x: 6
    anchors.verticalCenter: parent ? parent.verticalCenter : undefined
    width: size
    height: size
    // The palette colour in the URL makes the icon reload (and recolour) when the theme changes.
    source: name ? "image://icon/" + name + "?" + Kirigami.Theme.textColor : ""
    sourceSize: Qt.size(size * Screen.devicePixelRatio, size * Screen.devicePixelRatio)
    fillMode: Image.PreserveAspectFit
    smooth: true

    Image {
        visible: root.emblem !== ""
        width: Math.round(root.size * 0.62)
        height: width
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: -Math.round(width / 4)
        anchors.bottomMargin: -Math.round(width / 4)
        source: root.emblem ? "image://icon/" + root.emblem + "?" + Kirigami.Theme.textColor : ""
        sourceSize: Qt.size(width * Screen.devicePixelRatio, height * Screen.devicePixelRatio)
    }
}

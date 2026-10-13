import QtQuick

// Color over a checkerboard, raised (drop shadow and a light-to-dark bevel, no outline). A host can put
// something over the bottom edge with `decoration` (a Component whose root has `property var context`).
Item {
    id: sw
    property color color
    property Component decoration: null
    property var context: null
    readonly property real radius: 3

    BoxShadow {
        anchors.fill: parent
        radius: sw.radius
        blur: 3
        offsetY: 1.5
        color: Qt.rgba(0, 0, 0, 0.45)
    }
    Canvas {
        anchors.fill: parent
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            ctx.beginPath();
            ctx.roundedRect(0, 0, width, height, sw.radius, sw.radius);
            ctx.clip();
            for (let x = 0; x < width; x += 4)
                for (let y = 0; y < height; y += 4) {
                    ctx.fillStyle = (x / 4 + y / 4) % 2 ? "#999999" : "#666666";
                    ctx.fillRect(x, y, 4, 4);
                }
        }
    }
    Rectangle {
        anchors.fill: parent
        radius: sw.radius
        color: sw.color
    }
    // bevel: lit from above
    Rectangle {
        anchors.fill: parent
        radius: sw.radius
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.35) }
            GradientStop { position: 0.45; color: Qt.rgba(1, 1, 1, 0.0) }
            GradientStop { position: 0.8; color: Qt.rgba(0, 0, 0, 0.0) }
            GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.25) }
        }
    }
    Loader {
        anchors.fill: parent
        sourceComponent: sw.decoration
        onLoaded: item.context = Qt.binding(() => sw.context)
    }
}

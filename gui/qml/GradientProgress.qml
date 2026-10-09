import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// The run's progress: the phase's icon inside the track at the left, then the neon gradient of
// the terminal dashboard (cyan → purple → pink), and the count in white outlined text.
QQC2.ProgressBar {
    id: bar

    property string iconName
    property string caption

    implicitHeight: 30
    // org.kde.desktop centres a thin native groove: no insets, our own track fills the control.
    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0
    leftPadding: 28
    rightPadding: 4
    topPadding: 4
    bottomPadding: 4

    background: Rectangle {
        implicitHeight: 30
        radius: height / 2
        color: Qt.alpha(Kirigami.Theme.textColor, 0.07)
        border.width: 1
        border.color: Qt.alpha(Kirigami.Theme.textColor, 0.14)
    }

    contentItem: Item {
        property real sweep: 0

        implicitHeight: 22
        clip: true

        NumberAnimation on sweep {
            running: bar.indeterminate && bar.visible
            from: 0
            to: 1
            duration: 1100
            loops: Animation.Infinite
        }

        Rectangle {
            id: fill
            height: parent.height
            radius: height / 2
            width: bar.indeterminate ? parent.width * 0.3 : Math.max(height, parent.width * bar.visualPosition)
            visible: bar.indeterminate || bar.visualPosition > 0
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "#00e5ff" }
                GradientStop { position: 0.5; color: "#a24bff" }
                GradientStop { position: 1.0; color: "#ff3d9a" }
            }
            x: bar.indeterminate ? -width + parent.sweep * (parent.width + width) : 0
        }

        Text {
            anchors.centerIn: parent
            text: bar.caption
            color: "white"
            style: Text.Outline
            styleColor: "black"
            font.weight: Font.Bold
            font.pointSize: Kirigami.Theme.smallFont.pointSize
        }
    }

    PropertyIcon { name: bar.iconName }
}

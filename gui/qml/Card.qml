import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// A rounded panel with a soft shadow and a titled header (the title has its own icon).
Kirigami.ShadowedRectangle {
    id: card

    property string title
    property string iconName
    property alias headerExtra: headerExtra.data
    default property alias content: body.data

    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false

    implicitWidth: layout.implicitWidth + 2 * layout.anchors.margins
    implicitHeight: layout.implicitHeight + 2 * layout.anchors.margins
    radius: 12
    color: Kirigami.Theme.backgroundColor
    border.width: 1
    border.color: Qt.alpha(Kirigami.Theme.textColor, 0.10)
    shadow.size: 14
    shadow.yOffset: 2
    shadow.color: Qt.rgba(0, 0, 0, 0.13)

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing + 4
        spacing: Kirigami.Units.largeSpacing

        RowLayout {
            visible: card.title !== ""
            spacing: 8
            Item {
                implicitWidth: 20
                implicitHeight: 20
                PropertyIcon { name: card.iconName; size: 20; x: 0 }
            }
            Kirigami.Heading {
                text: card.title
                level: 4
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }
            RowLayout { id: headerExtra; spacing: 4 }
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Kirigami.Units.largeSpacing
        }
    }
}

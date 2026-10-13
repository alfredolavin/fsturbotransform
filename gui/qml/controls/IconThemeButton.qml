import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.iconthemes as KIconThemes

import "IconMetrics.js" as IconMetrics

// A theme icon setting: the icon of the setting inside at the left edge, then the chosen icon and its name; a click
// opens KDE's icon dialog. Bind `value` to the cfg_ string and write `picked(name)` back.
QQC2.Button {
    id: control

    property string iconName
    property string value
    // what the button shows for `value`: a theme name, or a file of the widget when value is its own default
    property var previewSource: value
    signal picked(string name)

    // no `text`/`icon`: the desktop style paints those in its background, under the property icon
    Accessible.name: value
    leftPadding: IconMetrics.reserve
    rightPadding: IconMetrics.margin * 2
    implicitWidth: leftPadding + contentItem.implicitWidth + rightPadding
    contentItem: RowLayout {
        spacing: IconMetrics.gap
        Kirigami.Icon {
            source: control.previewSource
            Layout.preferredWidth: Kirigami.Units.iconSizes.small
            Layout.preferredHeight: Kirigami.Units.iconSizes.small
        }
        QQC2.Label {
            text: control.value
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
    }
    onClicked: dialog.open()

    PropertyIcon { name: control.iconName }
    KIconThemes.IconDialog {
        id: dialog
        onIconNameChanged: name => {
            if (name)
                control.picked(name);
        }
    }
}

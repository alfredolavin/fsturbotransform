import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

import "IconMetrics.js" as IconMetrics

// A push button with the icon of its setting inside, at the left edge, then `label` and an optional `trailingIcon`
// (theme name, e.g. "document-edit"). The desktop style paints `text` and `icon.*` itself in its background, under
// the icon, so they stay unset and a content item shows them after it.
QQC2.Button {
    id: control

    property string iconName
    property string label
    property string trailingIcon

    leftPadding: IconMetrics.reserve
    rightPadding: IconMetrics.margin * 2
    implicitWidth: leftPadding + contentItem.implicitWidth + rightPadding
    Accessible.name: label
    contentItem: RowLayout {
        spacing: IconMetrics.gap
        QQC2.Label {
            text: control.label
            verticalAlignment: Text.AlignVCenter
            Layout.fillWidth: true
        }
        Kirigami.Icon {
            visible: control.trailingIcon !== ""
            source: control.trailingIcon
            implicitWidth: IconMetrics.size
            implicitHeight: IconMetrics.size
        }
    }

    PropertyIcon { name: control.iconName }
}

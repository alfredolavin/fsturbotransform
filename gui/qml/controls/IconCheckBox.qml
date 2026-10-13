import QtQuick
import QtQuick.Controls as QQC2

import "IconMetrics.js" as IconMetrics

// A CheckBox with the icon of its setting before the box (the desktop style places the box at leftPadding)
QQC2.CheckBox {
    id: control

    property string iconName

    leftPadding: IconMetrics.reserve

    PropertyIcon { name: control.iconName }
}

import QtQuick
import QtQuick.Controls as QQC2

import "IconMetrics.js" as IconMetrics

// A Switch with the icon of its setting before the switch
QQC2.Switch {
    id: control

    property string iconName

    leftPadding: IconMetrics.reserve

    PropertyIcon { name: control.iconName }
}

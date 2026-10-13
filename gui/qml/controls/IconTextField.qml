import QtQuick
import QtQuick.Controls as QQC2

import "IconMetrics.js" as IconMetrics

// A TextField with the icon of its setting inside, at the left edge
QQC2.TextField {
    id: control

    property string iconName

    leftPadding: IconMetrics.reserve

    PropertyIcon { name: control.iconName }
}

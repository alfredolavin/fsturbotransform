import QtQuick
import QtQuick.Controls as QQC2

import "IconMetrics.js" as IconMetrics

// A TextArea with the icon of its setting inside, at the left edge of its first line (in a tall area a centred icon
// would float away from the text it belongs to)
QQC2.TextArea {
    id: control

    property string iconName

    leftPadding: IconMetrics.reserve
    wrapMode: TextEdit.Wrap

    PropertyIcon {
        name: control.iconName
        firstLine: true
    }
}

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

import "IconMetrics.js" as IconMetrics

// A ComboBox with the icon of its setting inside, at the left edge. The desktop style paints the chosen text itself
// at a fixed place (an icon would cover it, and its own icon hint only reads files on disk), so it paints none and a
// label after the icon shows the choice. Sized for its longest entry. Any model works (strings, textRole, ListModel).
QQC2.ComboBox {
    id: control

    property string iconName
    property real maximumTextWidth: Kirigami.Units.gridUnit * 26

    displayText: ""
    baselineOffset: choice.y + choice.baselineOffset
    implicitWidth: IconMetrics.reserve + Math.min(maximumTextWidth, widestText) + IconMetrics.comboArrow
    // measured once per model (not again on every choice), and no further than the width it can get
    readonly property real widestText: {
        let w = 0;
        for (let i = 0; i < count && w < maximumTextWidth; ++i)
            w = Math.max(w, metrics.advanceWidth(textAt(i)));
        return w;
    }

    FontMetrics {
        id: metrics
        font: control.font
    }
    PropertyIcon { name: control.iconName }
    QQC2.Label {
        id: choice
        x: IconMetrics.reserve
        z: 5
        width: Math.max(0, control.width - x - IconMetrics.comboArrow)
        anchors.verticalCenter: parent.verticalCenter
        text: control.currentText
        font: control.font
        elide: Text.ElideRight
        opacity: control.enabled ? 1 : 0.6
    }
}

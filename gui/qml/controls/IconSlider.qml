import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

import "IconMetrics.js" as IconMetrics

// A Slider with the icon of its setting inside, at the left edge: the style draws groove and handle over the
// background, so the background moves with leftInset and the same leftPadding keeps the handle travel aligned.
// Snaps to stepSize and draws no tick marks (plasma.timer.style). Put a value label beside it, or use IconValueSlider.
QQC2.Slider {
    id: control

    property string iconName

    snapMode: QQC2.Slider.SnapAlways
    Kirigami.StyleHints.tickMarkStepSize: -1
    leftInset: IconMetrics.reserve
    leftPadding: IconMetrics.reserve
    implicitWidth: Kirigami.Units.gridUnit * 10 + IconMetrics.reserve

    PropertyIcon { name: control.iconName }
}

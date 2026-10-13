import QtQuick
import org.kde.kirigami as Kirigami

import "IconMetrics.js" as IconMetrics
import "icons/bundled.js" as Bundled

// The icon of one setting, inside its control at the left edge, vertically centred (the global QML rule in
// ~/.claude/CLAUDE.md). `name` is icons/<name>.svg next to this file when it is bundled there (Inkscape's coloured
// Tango icons: shared/tools/gen_icons.py copies them and lists them in icons/bundled.js and icons/SOURCES.md), else
// the icon theme's (KDE's Breeze / Oxygen, recoloured for the color scheme). A URL is used as is.
// The control makes room for it itself: leftPadding: IconMetrics.reserve (the Icon* controls here do it). In a
// multi-line text area it sits on the first line instead (firstLine), next to the text it belongs to.
Kirigami.Icon {
    id: icon

    property string name
    property bool firstLine: false

    implicitWidth: IconMetrics.size
    implicitHeight: IconMetrics.size
    width: IconMetrics.size
    height: IconMetrics.size
    x: IconMetrics.margin
    anchors.verticalCenter: parent && !firstLine ? parent.verticalCenter : undefined
    y: firstLine && parent ? parent.topPadding + (lineMetrics.height - height) / 2 : 0
    z: 5
    source: Bundled.names[name] ? Qt.resolvedUrl("icons/" + name + ".svg") : name
    isMask: false
    opacity: parent && parent.enabled === false ? 0.45 : 1

    FontMetrics {
        id: lineMetrics
        font: icon.parent && icon.parent.font ? icon.parent.font : Qt.application.font
    }
}

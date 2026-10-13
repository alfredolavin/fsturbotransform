import QtQuick
import org.kde.kirigami as Kirigami

// The Plasma color scheme's colors as a plain object, for the configurable colors (colorspec/), which
// cannot see Kirigami.Theme. It follows the scheme live: put one where colors are resolved and pass `map` along.
Item {
    id: sys

    // visible (but empty): Kirigami stops updating the theme of invisible items
    visible: true
    width: 0
    height: 0

    function rgba(c) {
        return { r: c.r, g: c.g, b: c.b, a: c.a };
    }

    readonly property var map: ({
        textColor: rgba(Kirigami.Theme.textColor),
        backgroundColor: rgba(Kirigami.Theme.backgroundColor),
        alternateBackgroundColor: rgba(Kirigami.Theme.alternateBackgroundColor),
        highlightColor: rgba(Kirigami.Theme.highlightColor),
        highlightedTextColor: rgba(Kirigami.Theme.highlightedTextColor),
        focusColor: rgba(Kirigami.Theme.focusColor),
        hoverColor: rgba(Kirigami.Theme.hoverColor),
        linkColor: rgba(Kirigami.Theme.linkColor),
        visitedLinkColor: rgba(Kirigami.Theme.visitedLinkColor),
        activeTextColor: rgba(Kirigami.Theme.activeTextColor),
        disabledTextColor: rgba(Kirigami.Theme.disabledTextColor),
        positiveTextColor: rgba(Kirigami.Theme.positiveTextColor),
        neutralTextColor: rgba(Kirigami.Theme.neutralTextColor),
        negativeTextColor: rgba(Kirigami.Theme.negativeTextColor)
    })
}

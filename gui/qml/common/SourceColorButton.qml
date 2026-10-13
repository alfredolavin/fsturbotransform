import QtQuick
import org.kde.kirigami as Kirigami

import "../colorspec" as CS
import "../controls"
import "../controls/IconMetrics.js" as IconMetrics
import "ColorSources.js" as Sources

// The configurable-color button (colorspec/) with the usual sources: a Plasma system color and the gradient's begin,
// end and current fill (ColorSources.js), and the icon of its setting inside at the left edge. Bind `value` to the
// cfg_ string and write `edited(v)` back; `stops` / `progress` are the gradient the swatch previews with.
CS.ColorSpecButton {
    id: btn

    property string iconName
    property var stops: []
    property real progress: 0.6

    leftPadding: iconName !== "" ? IconMetrics.reserve : 3

    SystemTheme {
        id: sys
    }

    sources: [{ id: "system", name: i18n("System"), optionLabel: i18n("System color:"),
                options: Sources.SYSTEM.map(e => ({ id: e[0], name: i18n(e[1]) })) }]
             .concat(stops.length ? [{ id: "begin", name: i18n("Gradient begin") }, { id: "end", name: i18n("Gradient end") },
                                     { id: "current", name: i18n("Current fill") }] : [])
    context: ({ stops: btn.stops, progress: btn.progress })
    baseColor: (src, fixedColor, ctx, option) => Sources.baseOf({ src: src, color: fixedColor, sys: option },
                                                                ctx ? ctx.stops : [], ctx ? ctx.progress : 0.6, sys.map)

    PropertyIcon { name: btn.iconName }
}

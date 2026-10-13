import QtQuick
import QtQuick.Controls as QQC2

import "IconMetrics.js" as IconMetrics

// A SpinBox with the icon of its setting inside the box. `suffix` is the unit (translated, e.g. i18nc("unit", " px"))
// and `zeroText` what 0 shows instead (e.g. i18n("Automatic")); a custom textFromValue works too. The desktop style
// sizes the box for its text only, so it is sized here for the widest of its texts plus the icon.
QQC2.SpinBox {
    id: control

    property string iconName
    property string suffix: ""
    property string zeroText: ""

    editable: true
    leftPadding: IconMetrics.reserve
    implicitWidth: IconMetrics.reserve + widest.advanceWidth + IconMetrics.spinButtons
    textFromValue: (v, locale) => v === 0 && zeroText !== "" ? zeroText : Number(v).toLocaleString(locale, "f", 0) + suffix
    valueFromText: (t, locale) => t === zeroText ? 0 : Number.fromLocaleString(locale, t.replace(suffix, "").trim())

    TextMetrics {
        id: widest
        font: control.font
        text: [control.textFromValue(control.from, control.locale), control.textFromValue(control.to, control.locale),
               control.textFromValue(control.value, control.locale)]
            .reduce((a, b) => b.length > a.length ? b : a)
    }
    PropertyIcon { name: control.iconName }
}

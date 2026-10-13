import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import QtQuick.Dialogs as Dialogs

import "ColorSpecCore.js" as Core
import "ColorHarmony.js" as Harmony
import "ColorGroupIcons.js" as GroupIcons

// A configurable color (see ColorSpecCore.js): shows the color and how it is made; a click opens a window to
// pick a fixed color or one of the host's sources, adjust its luminosity, chroma, hue and opacity, and preview
// the result. This is the one place the control lives; hosts only say what the sources are:
//   sources      [{ id, name, options?: [{ id, name }], optionLabel? }] besides "fixed"; a source with options
//                (e.g. the theme's colors) gets a chooser, the choice being stored as `sys`
//   baseColor    (src, fixedColor, context, option) => {r, g, b, a}, the color a source stands for
//   context      passed to baseColor (theme palette, active gradient …); while the window is open the
//                context of `previewExtra`'s item is used instead, when there is one
//   previewExtra optional Component shown in the window's Preview section; its root has `property var context`
//   decoration   optional Component drawn over the bottom edge of swatches (root has `property var context`)
//   groups       optional ColorGroups shared by the buttons of a page: the color can then join a functional group
//                (a name typed or picked in the window) with a role, and is computed from the group's primary
//                (and secondary) color: harmonies, tones, readable contrast, mixes, OKLCH modifiers or one
//                equation per channel (ColorHarmony.js). The window shows the group on a hue wheel and a tonal scale.
//   groupKey     the name this color has in `groups` (default: one of its own); groupLabel  its name for people
// The new value comes out through `edited`, so a binding to the setting stays intact.
// Hosts put the icon of the setting inside the button: leftPadding 28 and a 16 px icon at x 6 (the shared
// controls/PropertyIcon where there is one). In the window every control carries its own icon the same way.
QQC2.Button {
    id: btn

    // stored spec string
    property string value
    property var sources: []
    property var baseColor: (src, fixedColor, context, option) => fixedColor
    property var context: null
    property Component previewExtra: null
    property Component decoration: null
    property string dialogTitle: i18n("Choose a color")
    property var groups: null
    property string groupKey: ""
    property string groupLabel: ""
    signal edited(string value)

    property string autoKey: ""
    readonly property string ownKey: groupKey !== "" ? groupKey : autoKey

    readonly property var sourceIds: ["fixed"].concat(sources.map(s => s.id))
    readonly property var spec: Core.parse(value, sourceIds)
    // the editing window, built on the first click: a page with dozens of buttons would otherwise build dozens of
    // windows up front (slow, and Qt cannot incubate a Window asynchronously, so a Loader with such a page stalls)
    property Window dialog: null
    readonly property var activeContext: dialog && dialog.visible && dialog.extraItem ? dialog.extraItem.context : context

    function sourceName(src) {
        const s = sources.find(x => x.id === src);
        return s ? s.name : i18n("Fixed");
    }
    function sourceOf(src) {
        return sources.find(x => x.id === src) || null;
    }
    function optionsOf(src) {
        const s = sourceOf(src);
        return s && s.options ? s.options : [];
    }
    function optionName(src, id) {
        const o = optionsOf(src).find(x => x.id === id);
        return o ? o.name : "";
    }
    function baseOf(s, ctx) {
        return baseColor(s.src, s.color, ctx, s.sys);
    }
    function roleName(id) {
        return i18n(Harmony.roleOf(id).name);
    }
    function summary(s) {
        const opt = optionName(s.src, s.sys);
        const own = !s.grp || Harmony.roleOf(s.role).own;
        const parts = s.grp ? [s.grp + ": " + roleName(s.role)] : [];
        if (own)
            parts.push(s.src === "fixed" ? Core.hexOf(s.color).replace(/^#ff/, "#") : sourceName(s.src) + (opt ? ": " + opt : ""));
        else if (Harmony.roleOf(s.role).param)
            parts.push(Math.round(Harmony.paramOf(s)) + Harmony.roleOf(s.role).param.unit);
        const adj = s.role === "equal" && s.grp ? "" : Core.adjustments(s);
        if (adj)
            parts.push(adj);
        if (s.a < 100)
            parts.push(s.a + " %");
        return parts.join(" · ");
    }
    function qcolor(c) {
        return Qt.rgba(c.r, c.g, c.b, c.a === undefined ? 1 : c.a);
    }

    // the color of its own source (what a primary or secondary gives the group)
    readonly property var ownBase: baseOf(spec, context)
    // {list, resolved} of the page with `s` as this color (the window's preview) or as it is
    function resolveWith(s, base) {
        if (groups)
            return groups.resolveWith(ownKey, groupLabel, s, base);
        const l = [{ key: ownKey, label: groupLabel, spec: s, base: base }];
        return { list: l, resolved: Harmony.resolveEntries(l) };
    }
    // the colors of `s`'s group for GroupWheel / TonalStrip
    function membersOf(r, s) {
        if (!s.grp)
            return [];
        return r.list.filter(e => e.spec.grp === s.grp).map(e => ({
            key: e.key, label: e.label, role: e.spec.role, roleName: roleName(e.spec.role),
            color: (r.resolved[e.key] || { color: e.base }).color, self: e.key === ownKey
        }));
    }
    readonly property var current: groups ? { list: groups.list, resolved: groups.resolved } : resolveWith(spec, ownBase)
    readonly property var currentEntry: current.resolved[ownKey] || { color: Core.apply(spec, ownBase), base: ownBase }
    readonly property color resolved: qcolor(currentEntry.color)

    // keep `groups` up to date with this color
    readonly property string registration: groups && ownKey !== "" ? JSON.stringify([ownKey, groupLabel, value, Core.hexOf(ownBase)]) : ""
    onRegistrationChanged: if (registration !== "") groups.register(ownKey, groupLabel, spec, ownBase)
    Component.onCompleted: {
        if (groups && groupKey === "" && autoKey === "")
            autoKey = groups.newKey();
        if (registration !== "")
            groups.register(ownKey, groupLabel, spec, ownBase);
    }
    Component.onDestruction: if (groups && ownKey !== "") groups.unregister(ownKey)
    onGroupsChanged: if (groups && groupKey === "" && autoKey === "") autoKey = groups.newKey()

    // The icon of a setting inside its control, at the left edge, vertically centred: 16 px at x 6, the control
    // reserving 28 px (the metrics of the shared Icon* controls, drawn here because colorspec is also used where
    // there is no controls/ directory). `source` is a theme icon name.
    component SettingIcon: Kirigami.Icon {
        width: 16
        height: 16
        x: 6
        z: 5
        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
        isMask: false
        opacity: parent && parent.enabled === false ? 0.45 : 1
    }

    // A ComboBox of [{id, name}] with the icon of its setting inside at the left edge and a swatch before each name
    // (colorOf(id)). The desktop style paints the chosen text itself at a fixed place, under the icon, so it paints
    // none and a swatch + label after the icon show the choice. Sized for its longest entry.
    component IconCombo: QQC2.ComboBox {
        id: combo

        property string iconName
        property var colorOf: id => "transparent"
        property bool swatches: true
        readonly property real swatchWidth: swatches ? Kirigami.Units.gridUnit * 1.2 : 0
        readonly property var current: currentIndex >= 0 && model && model[currentIndex] ? model[currentIndex] : null
        readonly property real widestText: {
            model; // re-evaluate when the model changes
            let w = metrics.advanceWidth(currentText);
            for (let i = 0; i < count; ++i)
                w = Math.max(w, metrics.advanceWidth(textAt(i)));
            return w;
        }

        textRole: "name"
        displayText: ""
        baselineOffset: choice.y + choice.baselineOffset
        implicitWidth: 28 + swatchWidth + Kirigami.Units.smallSpacing + Math.min(Kirigami.Units.gridUnit * 16, widestText) + 34

        FontMetrics {
            id: metrics
            font: combo.font
        }
        SettingIcon { source: combo.iconName }
        Swatch {
            id: chosen
            x: 28
            z: 5
            anchors.verticalCenter: parent.verticalCenter
            width: combo.swatchWidth
            height: Kirigami.Units.gridUnit * 0.8
            visible: !!combo.current && combo.swatches
            color: combo.current && combo.swatches ? combo.colorOf(combo.current.id) : "transparent"
            opacity: combo.enabled ? 1 : 0.5
        }
        QQC2.Label {
            id: choice
            x: combo.swatches ? chosen.x + chosen.width + Kirigami.Units.smallSpacing : 28
            z: 5
            width: Math.max(0, combo.width - x - 34)
            anchors.verticalCenter: parent.verticalCenter
            text: combo.currentText
            font: combo.font
            elide: Text.ElideRight
            opacity: combo.enabled ? 1 : 0.6
        }
        delegate: QQC2.ItemDelegate {
            required property var modelData
            required property int index
            width: combo.popup.width
            highlighted: combo.highlightedIndex === index
            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing
                Swatch {
                    visible: combo.swatches
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 1.4
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 0.9
                    color: combo.swatches ? combo.colorOf(modelData.id) : "transparent"
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    text: modelData.name
                    elide: Text.ElideRight
                }
            }
        }
    }

    QQC2.ToolTip.text: i18n("Click to change")
    QQC2.ToolTip.visible: hovered
    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

    leftPadding: 3
    rightPadding: 3

    // transparent, with a small shadow around it; a tint while hovered or pressed, a ring when focused
    background: Item {
        implicitWidth: Kirigami.Units.gridUnit * 3
        implicitHeight: Kirigami.Units.gridUnit * 1.6
        readonly property color hl: Kirigami.Theme.highlightColor
        BoxShadow {
            anchors.fill: parent
            radius: Kirigami.Units.cornerRadius
            blur: 4
            offsetY: 1
            color: Qt.rgba(0, 0, 0, 0.3)
        }
        Rectangle {
            anchors.fill: parent
            radius: Kirigami.Units.cornerRadius
            color: btn.pressed ? Qt.rgba(parent.hl.r, parent.hl.g, parent.hl.b, 0.3)
                 : btn.hovered ? Qt.rgba(parent.hl.r, parent.hl.g, parent.hl.b, 0.15) : "transparent"
            border.width: btn.visualFocus ? 1 : 0
            border.color: parent.hl
        }
    }

    contentItem: RowLayout {
        spacing: Kirigami.Units.smallSpacing
        Swatch {
            Layout.preferredWidth: Kirigami.Units.gridUnit * 1.6
            Layout.preferredHeight: Kirigami.Units.gridUnit * 1.1
            color: btn.resolved
            // gradient sources show a tiny gradient underline
            decoration: btn.spec.src === "fixed" || (btn.spec.grp && !Harmony.roleOf(btn.spec.role).own) ? null : btn.decoration
            context: btn.context
        }
        // where it sits in its group, relative to the primary
        GroupWheel {
            visible: btn.spec.grp !== ""
            mini: true
            members: visible ? btn.membersOf(btn.current, btn.spec) : []
        }
        QQC2.Label {
            text: btn.summary(btn.spec)
            elide: Text.ElideRight
            Layout.maximumWidth: Kirigami.Units.gridUnit * 12
        }
    }

    onClicked: {
        if (!dialog)
            dialog = dialogComponent.createObject(btn);
        dialog.load(spec);
        dialog.open();
    }

    // A window of its own, sized to its content (not limited by the settings dialog), scrolling when the
    // screen is smaller than that
    Component {
        id: dialogComponent
    Window {
        id: dialog
        title: btn.dialogTitle
        transientParent: btn.Window.window
        modality: Qt.WindowModal
        flags: Qt.Dialog
        color: Kirigami.Theme.backgroundColor

        readonly property int margin: Kirigami.Units.largeSpacing * 2
        readonly property int fitWidth: editor.implicitWidth + 2 * margin + scroll.effectiveScrollBarWidth
        readonly property int fitHeight: editor.implicitHeight + buttons.implicitHeight + signature.implicitHeight + 4 * margin
        minimumWidth: Math.min(fitWidth, Screen.desktopAvailableWidth)
        minimumHeight: Math.min(Kirigami.Units.gridUnit * 12, fitHeight)
        width: minimumWidth
        height: Math.min(fitHeight, Screen.desktopAvailableHeight * 0.9)

        function open() {
            width = minimumWidth;
            height = Math.min(fitHeight, Screen.desktopAvailableHeight * 0.9);
            show();
            raise();
            requestActivate();
            refit.restart();
        }
        function accept() {
            btn.edited(Core.stringify(editor.edited));
            close();
        }
        function load(spec) {
            editor.load(spec);
        }
        readonly property var extraItem: extraLoader.item

        // the content grows after the window opens (fonts, models) and when the role shows other controls; the
        // size is set a moment later, as on Wayland the compositor's first configure would undo it
        onMinimumWidthChanged: if (visible) refit.restart()
        onFitHeightChanged: if (visible) refit.restart()
        Timer {
            id: refit
            interval: 60
            onTriggered: {
                if (dialog.width < dialog.minimumWidth)
                    dialog.width = dialog.minimumWidth;
                dialog.height = Math.min(dialog.fitHeight, Screen.desktopAvailableHeight * 0.9);
            }
        }

        Shortcut { sequence: "Escape"; onActivated: dialog.close() }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: dialog.margin
            spacing: dialog.margin

            QQC2.ScrollView {
                id: scroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth

                Kirigami.FormLayout {
                    id: editor
                    width: scroll.availableWidth
                    // the window is sized to the wide layout, so it must not fold into the narrow one
                    wideMode: true

                    // the spec being edited; written back only on OK
                    property var edited: Core.defaults()
                    property bool loading: false
                    // its own source color, and the page's colors with it as edited
                    readonly property var ownBase: btn.baseOf(edited, btn.activeContext)
                    readonly property var res: btn.resolveWith(edited, ownBase)
                    readonly property var entry: res.resolved[btn.ownKey] || { color: Core.apply(edited, ownBase), base: ownBase }
                    // the color before the adjustments (the source's, or what the group role gives) and the result
                    readonly property var base: entry.base
                    readonly property var result: entry.color
                    readonly property string key: JSON.stringify([edited, Core.hexOf(base)])

                    readonly property bool grouped: edited.grp !== ""
                    readonly property var role: Harmony.roleOf(edited.role)
                    readonly property bool own: !grouped || !!role.own
                    readonly property var roleParam: grouped && role.param ? role.param : null
                    readonly property bool isEquation: grouped && edited.role === "equation"
                    readonly property var model: Harmony.modelOf(edited.eqm)
                    readonly property var eqVars: model.ch.map(c => c + "1").concat(model.ch.map(c => c + "2"), ["A1", "A2"])
                    readonly property var eqErrors: isEquation && entry.errors ? entry.errors : [null, null, null]
                    readonly property var eqValues: isEquation && entry.values ? entry.values : null
                    readonly property var members: btn.membersOf(res, edited)
                    // the group's primary (whose tonal scale is shown)
                    readonly property var primaryColor: entry.P || (grouped ? result : null)

                    // what role `id` would give (the role chooser's swatches)
                    function roleColor(id) {
                        if (!grouped)
                            return btn.qcolor(result);
                        const r = Harmony.roleOf(id);
                        if (r.own)
                            return btn.qcolor(Core.apply(edited, ownBase));
                        const P = entry.P || Core.apply(edited, ownBase);
                        const S = entry.S || Harmony.rotate(P, 180);
                        const s = Object.assign({}, edited, { role: id, rp: null });
                        return btn.qcolor(Harmony.finish(s, Harmony.derive(s, P, S).color));
                    }
                    // what the role's parameter gives at `v` (its slider's strip)
                    function paramColor(v) {
                        const P = entry.P || Core.apply(edited, ownBase);
                        const S = entry.S || Harmony.rotate(P, 180);
                        const s = Object.assign({}, edited, { rp: v });
                        return Harmony.finish(s, Harmony.derive(s, P, S).color);
                    }
                    function setGroup(name) {
                        if (name === edited.grp)
                            return;
                        const e = Object.assign({}, edited);
                        e.grp = name;
                        // joining a group that already has a primary: start as its complement
                        if (name && e.role === "primary" && res.list.some(x => x.key !== btn.ownKey && x.spec.grp === name && x.spec.role === "primary"))
                            e.role = "complement";
                        edited = e;
                    }
                    function setRole(id) {
                        const e = Object.assign({}, edited);
                        e.role = id;
                        e.rp = null;
                        if (id === "equation" && !(e.eq && e.eq.length))
                            e.eq = Harmony.identity(e.eqm);
                        edited = e;
                        loadRole(e);
                    }
                    function setModel(id) {
                        const e = Object.assign({}, edited);
                        e.eqm = id;
                        e.eq = Harmony.identity(id);
                        edited = e;
                        loadRole(e);
                    }
                    function setExpr(i, text) {
                        if (loading)
                            return;
                        const e = Object.assign({}, edited);
                        const eq = Harmony.identity(e.eqm).map((d, k) => (e.eq && typeof e.eq[k] === "string" ? e.eq[k] : d));
                        eq[i] = text;
                        e.eq = eq;
                        edited = e;
                    }
                    // the role's controls from a spec (they are not bound to it, so typing is not disturbed)
                    function loadRole(s) {
                        loading = true;
                        if (roleParam)
                            paramSlider.value = Harmony.paramOf(s);
                        const eq = Harmony.identity(s.eqm).map((d, k) => (s.eq && typeof s.eq[k] === "string" ? s.eq[k] : d));
                        expr0.text = eq[0];
                        expr1.text = eq[1];
                        expr2.text = eq[2];
                        loading = false;
                    }
                    function fmt(v) {
                        return Math.abs(v) >= 100 ? v.toFixed(0) : v.toFixed(1);
                    }

                    function load(s) {
                        edited = Object.assign({}, s);
                        groupCombo.editText = s.grp;
                        loadRole(s);
                        const opts = optionCombo.model;
                        optionCombo.currentIndex = Math.max(0, opts.findIndex(o => o.id === s.sys));
                        luminosity.value = s.l;
                        chroma.value = s.c;
                        hue.value = s.h;
                        opacitySlider.value = s.a;
                        fixedColor.color = btn.qcolor(s.color);
                    }
                    function set(field, v) {
                        const e = Object.assign({}, edited);
                        e[field] = v;
                        edited = e;
                    }

                    // ---- group ----
                    Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Group") }

                    QQC2.ComboBox {
                        id: groupCombo
                        Kirigami.FormData.label: i18n("Group:")
                        editable: true
                        model: btn.groups ? btn.groups.names : []
                        leftPadding: 28
                        implicitWidth: Kirigami.Units.gridUnit * 12
                        Accessible.name: i18n("Group")
                        onEditTextChanged: if (!editor.loading) editor.setGroup(editText.trim())
                        onActivated: index => editor.setGroup(textAt(index))
                        // Tango's coloured icon, bundled as a data URL (Kirigami.Icon does not draw those)
                        Image {
                            x: 6
                            z: 5
                            width: 16
                            height: 16
                            anchors.verticalCenter: parent.verticalCenter
                            sourceSize: Qt.size(16, 16)
                            source: GroupIcons.objectGroup
                        }
                        QQC2.ToolTip.text: i18n("Colors with the same group name are computed together from the group's primary color. Type a new name or pick one; empty: a color of its own")
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                    IconCombo {
                        id: roleCombo
                        Kirigami.FormData.label: i18n("Role:")
                        iconName: "distribute-graph-directed"
                        enabled: editor.grouped
                        model: Harmony.ROLES.map(r => ({ id: r.id, name: i18n(r.name) }))
                        currentIndex: Math.max(0, Harmony.ROLES.findIndex(r => r.id === editor.edited.role))
                        colorOf: id => editor.roleColor(id)
                        onActivated: index => editor.setRole(Harmony.ROLES[index].id)
                        QQC2.ToolTip.text: enabled ? i18n("How this color is made from the group's primary (1) and secondary (2) colors")
                                                   : i18n("Give the color a group to choose its role")
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                    ColorStripSlider {
                        id: paramSlider
                        visible: !!editor.roleParam
                        Kirigami.FormData.label: editor.roleParam ? i18n(editor.roleParam.label) : ""
                        iconSource: editor.roleParam ? editor.roleParam.icon : ""
                        from: editor.roleParam ? editor.roleParam.from : 0
                        to: editor.roleParam ? editor.roleParam.to : 100
                        stepSize: editor.roleParam ? editor.roleParam.step : 1
                        suffix: editor.roleParam ? editor.roleParam.unit : ""
                        colorAt: v => editor.paramColor(v)
                        repaintKey: editor.key
                        onMoved: editor.set("rp", value)
                    }

                    // the equation: a color model and one expression per channel
                    IconCombo {
                        id: modelCombo
                        visible: editor.isEquation
                        Kirigami.FormData.label: i18n("Color model:")
                        iconName: "adjustrgb"
                        swatches: false
                        model: Harmony.MODELS.map(m => ({ id: m.id, name: m.name }))
                        currentIndex: Math.max(0, Harmony.MODELS.findIndex(m => m.id === editor.edited.eqm))
                        onActivated: index => editor.setModel(Harmony.MODELS[index].id)
                        QQC2.ToolTip.text: i18n("The channels the expressions compute (choosing another model starts from the primary's channels)")
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                    ExpressionField {
                        id: expr0
                        visible: editor.isEquation
                        Kirigami.FormData.label: editor.model.ch[0] + ":"
                        channel: editor.model.ch[0]
                        vars: editor.eqVars
                        error: editor.eqErrors[0] ? editor.eqErrors[0].error : ""
                        errorPos: editor.eqErrors[0] ? editor.eqErrors[0].pos : -1
                        valueText: editor.eqValues ? "= " + editor.fmt(editor.eqValues[0]) : ""
                        onTextChanged: editor.setExpr(0, text)
                    }
                    ExpressionField {
                        id: expr1
                        visible: editor.isEquation
                        Kirigami.FormData.label: editor.model.ch[1] + ":"
                        channel: editor.model.ch[1]
                        vars: editor.eqVars
                        error: editor.eqErrors[1] ? editor.eqErrors[1].error : ""
                        errorPos: editor.eqErrors[1] ? editor.eqErrors[1].pos : -1
                        valueText: editor.eqValues ? "= " + editor.fmt(editor.eqValues[1]) : ""
                        onTextChanged: editor.setExpr(1, text)
                    }
                    ExpressionField {
                        id: expr2
                        visible: editor.isEquation
                        Kirigami.FormData.label: editor.model.ch[2] + ":"
                        channel: editor.model.ch[2]
                        vars: editor.eqVars
                        error: editor.eqErrors[2] ? editor.eqErrors[2].error : ""
                        errorPos: editor.eqErrors[2] ? editor.eqErrors[2].pos : -1
                        valueText: editor.eqValues ? "= " + editor.fmt(editor.eqValues[2]) : ""
                        onTextChanged: editor.setExpr(2, text)
                    }
                    QQC2.Label {
                        visible: editor.isEquation
                        Layout.maximumWidth: expr0.implicitWidth
                        wrapMode: Text.Wrap
                        font: Kirigami.Theme.smallFont
                        opacity: 0.75
                        text: i18n("Primary: %1 · secondary: %2 (%3) · alpha A1 A2 (0–1)",
                                   editor.model.ch.map(c => c + "1").join(" "), editor.model.ch.map(c => c + "2").join(" "),
                                   editor.model.ch.map((c, k) => c + " " + editor.model.range[k]).join(", "))
                    }

                    QQC2.Label {
                        readonly property string note: !editor.grouped ? ""
                            : editor.entry.conflict ? i18n("“%1” already has a %2 color: this one only gives its own color", editor.edited.grp, editor.edited.role === "primary" ? i18n("primary") : i18n("secondary"))
                            : !editor.own && !editor.entry.primaryKey ? i18n("“%1” has no primary color yet: this color's own source stands in for it", editor.edited.grp)
                            : ""
                        visible: note !== ""
                        text: note
                        color: Kirigami.Theme.neutralTextColor
                        wrapMode: Text.Wrap
                        Layout.maximumWidth: Kirigami.Units.gridUnit * 18
                    }
                    RowLayout {
                        Kirigami.FormData.label: i18n("Group colors:")
                        enabled: editor.grouped
                        opacity: enabled ? 1 : 0.4
                        spacing: Kirigami.Units.largeSpacing
                        GroupWheel {
                            members: editor.members
                        }
                        TonalStrip {
                            Layout.fillHeight: true
                            base: editor.primaryColor
                            members: editor.members
                        }
                    }

                    // ---- source ----
                    Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Color") }

                    IconCombo {
                        id: sourceCombo
                        enabled: editor.own
                        Kirigami.FormData.label: i18n("Source:")
                        iconName: "color-management"
                        model: btn.sourceIds.map(id => ({ id: id, name: btn.sourceName(id) }))
                        currentIndex: Math.max(0, btn.sourceIds.indexOf(editor.edited.src))
                        // what each source gives in the preview, before adjustments
                        colorOf: id => btn.qcolor(btn.baseColor(id, editor.edited.color, btn.activeContext, editor.edited.sys))
                        onActivated: index => editor.set("src", btn.sourceIds[index])
                        QQC2.ToolTip.text: i18n("Where the color comes from; the adjustments below are applied to it")
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                    QQC2.Button {
                        id: fixedColor
                        Kirigami.FormData.label: i18n("Fixed color:")
                        enabled: editor.own && editor.edited.src === "fixed"
                        property color color: "white"
                        leftPadding: 28
                        rightPadding: 6
                        Accessible.name: i18n("Fixed color")
                        QQC2.ToolTip.text: enabled ? i18n("Pick the fixed color") : i18n("Used when the source is “Fixed”")
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                        contentItem: Item {
                            implicitWidth: Kirigami.Units.gridUnit * 1.6
                            implicitHeight: Kirigami.Units.gridUnit * 1.1
                            Swatch {
                                anchors.centerIn: parent
                                width: Kirigami.Units.gridUnit * 1.6
                                height: Kirigami.Units.gridUnit * 1.1
                                color: fixedColor.color
                                opacity: fixedColor.enabled ? 1 : 0.5
                            }
                        }
                        onClicked: {
                            colorDialog.selectedColor = color;
                            colorDialog.open();
                        }
                        SettingIcon { source: "color-picker" }
                        Dialogs.ColorDialog {
                            id: colorDialog
                            title: i18n("Fixed color")
                            options: Dialogs.ColorDialog.ShowAlphaChannel
                            onAccepted: {
                                const c = selectedColor;
                                fixedColor.color = c;
                                editor.set("color", { r: c.r, g: c.g, b: c.b, a: c.a });
                            }
                        }
                    }

                    IconCombo {
                        id: optionCombo
                        readonly property var choices: btn.optionsOf(editor.edited.src)
                        readonly property var owner: btn.sources.find(x => x.options && x.options.length) || null
                        // only hosts with a source that has variants (e.g. the theme's colors) show it
                        visible: !!owner
                        Kirigami.FormData.label: owner && owner.optionLabel ? owner.optionLabel : i18n("Variant:")
                        iconName: "preferences-desktop-color"
                        enabled: choices.length > 0 && editor.own
                        model: choices.length ? choices : (owner ? owner.options : [])
                        colorOf: id => btn.qcolor(btn.baseColor(choices.length || !owner ? editor.edited.src : owner.id, editor.edited.color, btn.activeContext, id))
                        onActivated: index => editor.set("sys", model[index].id)
                        QQC2.ToolTip.text: enabled ? "" : i18n("Choose the source “%1” to use it", owner ? owner.name : "")
                        QQC2.ToolTip.visible: hovered && !enabled
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }

                    // ---- adjustments ----
                    Kirigami.Separator {
                        Kirigami.FormData.isSection: true
                        Kirigami.FormData.label: editor.grouped && editor.edited.role === "custom" ? i18n("OKLCH modifiers from the primary")
                                               : i18n("Adjustments (OKLCH)")
                    }

                    ColorStripSlider {
                        id: luminosity
                        enabled: !(editor.grouped && editor.edited.role === "equal")
                        iconSource: "colors-luma"
                        Kirigami.FormData.label: i18n("Luminosity:")
                        from: -100
                        colorAt: v => Core.shade(editor.base, v, editor.edited.c, editor.edited.h)
                        repaintKey: editor.key
                        onMoved: editor.set("l", value)
                    }
                    ColorStripSlider {
                        id: chroma
                        enabled: !(editor.grouped && editor.edited.role === "equal")
                        iconSource: "colors-chromablue"
                        Kirigami.FormData.label: i18n("Chroma:")
                        from: -100
                        colorAt: v => Core.shade(editor.base, editor.edited.l, v, editor.edited.h)
                        repaintKey: editor.key
                        onMoved: editor.set("c", value)
                    }
                    ColorStripSlider {
                        id: hue
                        enabled: !(editor.grouped && editor.edited.role === "equal")
                        iconSource: "color-mode-hue-shift-positive"
                        Kirigami.FormData.label: i18n("Hue offset:")
                        from: -180
                        to: 180
                        stepSize: 5
                        suffix: i18nc("unit, after a number: degrees", "°")
                        colorAt: v => Core.shade(editor.base, editor.edited.l, editor.edited.c, v)
                        repaintKey: editor.key
                        onMoved: editor.set("h", value)
                    }
                    ColorStripSlider {
                        id: opacitySlider
                        iconSource: "edit-opacity"
                        Kirigami.FormData.label: i18n("Opacity:")
                        checker: true
                        suffix: i18nc("unit, after a number", " %")
                        colorAt: v => {
                            const c = Core.shade(editor.base, editor.edited.l, editor.edited.c, editor.edited.h);
                            return { r: c.r, g: c.g, b: c.b, a: (editor.base.a === undefined ? 1 : editor.base.a) * v / 100 };
                        }
                        repaintKey: editor.key
                        onMoved: editor.set("a", value)
                    }

                    // ---- preview ----
                    Kirigami.Separator { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("Preview") }

                    Loader {
                        id: extraLoader
                        Layout.fillWidth: true
                        sourceComponent: btn.previewExtra
                        visible: active && status === Loader.Ready
                    }
                    RowLayout {
                        Kirigami.FormData.label: i18n("Result:")
                        spacing: Kirigami.Units.smallSpacing
                        Swatch {
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 3
                            Layout.preferredHeight: Kirigami.Units.gridUnit * 1.6
                            color: btn.qcolor(editor.base)
                        }
                        QQC2.Label { text: "→" }
                        Swatch {
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 5
                            Layout.preferredHeight: Kirigami.Units.gridUnit * 1.6
                            color: btn.qcolor(editor.result)
                        }
                        QQC2.Label {
                            text: btn.summary(editor.edited)
                            opacity: 0.8
                        }
                    }
                }
            }

            QQC2.DialogButtonBox {
                id: buttons
                Layout.fillWidth: true
                standardButtons: QQC2.DialogButtonBox.Ok | QQC2.DialogButtonBox.Cancel
                onAccepted: dialog.accept()
                onRejected: dialog.close()
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                AuthorSignature { id: signature }
            }
        }
    }
}
}

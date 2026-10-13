import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

import "IconMetrics.js" as IconMetrics

// The font family of a setting: a ComboBox with the icon of its setting inside at the left edge (like IconComboBox)
// made for the system's font list, hundreds of entries. The desktop style's drop-down is a Menu that builds one
// native-painted row per entry when it opens (slow to open and to scroll with that many), so this one has a popup
// of its own: a search field that filters the list as you type and a virtual list (only the visible rows exist)
// showing each family's name and a sample of it. Bind `family` ("" = the default, the first entry, labelled
// `defaultText`) and store what `familyEdited(name)` reports; the control never writes `family` itself.
QQC2.ComboBox {
    id: control

    property string iconName
    property string family: ""
    property string defaultText: i18n("System default")
    property string sampleText: "AaBbGg 123"
    property real maximumTextWidth: Kirigami.Units.gridUnit * 14
    property int visibleRows: 12
    // the families to choose from: the system's, read once (every Qt.fontFamilies() call builds the whole list
    // again); a host with bundled fonts passes its own list (textspec's TextFonts.families)
    property var families: Qt.fontFamilies()
    signal familyEdited(string name)

    property string filter: ""
    // what the list shows: the default entry (index 0), then the families matching the filter
    readonly property var shown: {
        const f = filter.trim().toLowerCase();
        return [defaultText].concat(f === "" ? families : families.filter(n => n.toLowerCase().indexOf(f) >= 0));
    }
    readonly property int rowHeight: Math.round(Kirigami.Units.gridUnit * 1.7)
    readonly property real widestText: {
        let w = metrics.advanceWidth(defaultText);
        for (let i = 0; i < families.length && w < maximumTextWidth; ++i)
            w = Math.max(w, metrics.advanceWidth(families[i]));
        return w;
    }

    model: shown
    displayText: ""
    // a wheel over a list this long would change the font while the page is being scrolled
    wheelEnabled: false
    baselineOffset: choice.y + choice.baselineOffset
    implicitWidth: IconMetrics.reserve + Math.min(maximumTextWidth, widestText) + IconMetrics.comboArrow

    onActivated: index => familyEdited(index > 0 && index < shown.length ? shown[index] : "")

    // the entry of `family`; set, not bound: a new model (the filter, a host list that arrives later) puts the
    // ComboBox back on its first entry, after which a binding would not run again
    function sync() {
        currentIndex = Math.max(0, family === "" ? 0 : shown.indexOf(family));
    }
    Component.onCompleted: sync()
    onFamilyChanged: sync()
    onModelChanged: sync()

    // a choice made with the keyboard, from the search field (the name is taken before closing clears the filter)
    function choose(index) {
        if (index < 0 || index >= shown.length)
            return;
        const name = index > 0 ? shown[index] : "";
        popup.close();
        familyEdited(name);
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
        text: control.family === "" ? control.defaultText : control.family
        font: control.font
        elide: Text.ElideRight
        opacity: control.enabled ? 1 : 0.6
    }

    // a row: a check mark on the chosen family, its name, and a sample written in it
    delegate: QQC2.ItemDelegate {
        id: row
        required property int index
        required property string modelData
        width: ListView.view ? ListView.view.width : implicitWidth
        implicitHeight: control.rowHeight
        highlighted: list.currentIndex === index
        contentItem: RowLayout {
            spacing: Kirigami.Units.smallSpacing
            Kirigami.Icon {
                source: "checkmark"
                opacity: row.index === control.currentIndex ? 1 : 0
                Layout.preferredWidth: IconMetrics.size
                Layout.preferredHeight: IconMetrics.size
            }
            QQC2.Label {
                text: row.modelData
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            QQC2.Label {
                text: control.sampleText
                font.family: row.index === 0 ? control.font.family : row.modelData
                font.pixelSize: Math.round(control.font.pixelSize * 1.15)
                opacity: 0.75
                elide: Text.ElideRight
                Layout.maximumWidth: Kirigami.Units.gridUnit * 9
            }
        }
    }

    popup: QQC2.Popup {
        id: pop

        readonly property int rows: Math.max(1, Math.min(control.count, control.visibleRows,
            Math.floor(((control.Window.window ? control.Window.window.height : 700) - Kirigami.Units.gridUnit * 6) / control.rowHeight)))
        // below the box, or above it when there is no room there
        readonly property bool fitsBelow: {
            const win = control.Window.window;
            if (!win)
                return true;
            return control.mapToItem(null, 0, control.height).y + height <= win.height;
        }
        y: fitsBelow ? control.height : -height
        width: Math.max(control.width, Kirigami.Units.gridUnit * 22)
        padding: Kirigami.Units.smallSpacing
        focus: true
        // the style's background reads popup.exit.running
        enter: Transition {}
        exit: Transition {}

        onOpened: {
            search.text = "";
            list.currentIndex = control.currentIndex;
            list.positionViewAtIndex(control.currentIndex, ListView.Center);
            search.forceActiveFocus();
        }
        onClosed: {
            search.text = "";
            control.filter = "";
        }

        contentItem: ColumnLayout {
            spacing: Kirigami.Units.smallSpacing
            Kirigami.SearchField {
                id: search
                Layout.fillWidth: true
                placeholderText: i18n("Search a font…")
                // accepted() on Return only (by default Kirigami emits it at every keystroke)
                autoAccept: false
                onTextChanged: {
                    control.filter = text;
                    list.currentIndex = text === "" ? control.currentIndex : (list.count > 1 ? 1 : 0);
                    list.positionViewAtIndex(list.currentIndex, ListView.Contain);
                }
                onAccepted: control.choose(list.currentIndex)
                Keys.onUpPressed: list.step(-1)
                Keys.onDownPressed: list.step(1)
                Keys.onPressed: event => {
                    if (event.key === Qt.Key_PageDown) {
                        list.step(control.visibleRows - 1);
                        event.accepted = true;
                    } else if (event.key === Qt.Key_PageUp) {
                        list.step(1 - control.visibleRows);
                        event.accepted = true;
                    }
                }
            }
            ListView {
                id: list
                Layout.fillWidth: true
                Layout.preferredHeight: pop.rows * control.rowHeight
                clip: true
                model: control.delegateModel
                keyNavigationEnabled: false
                boundsBehavior: Flickable.StopAtBounds
                QQC2.ScrollBar.vertical: QQC2.ScrollBar {}
                function step(n) {
                    currentIndex = Math.max(0, Math.min(count - 1, currentIndex + n));
                    positionViewAtIndex(currentIndex, ListView.Contain);
                }
            }
        }
        // the ComboBox highlights the row under the mouse; the keyboard cursor follows it
        Connections {
            target: control
            function onHighlightedIndexChanged() {
                if (control.highlightedIndex >= 0)
                    list.currentIndex = control.highlightedIndex;
            }
        }
        background: Rectangle {
            Kirigami.Theme.colorSet: Kirigami.Theme.View
            Kirigami.Theme.inherit: false
            color: Kirigami.Theme.backgroundColor
            border.color: Kirigami.ColorUtils.tintWithAlpha(color, Kirigami.Theme.textColor, 0.3)
            border.width: 1
            radius: Kirigami.Units.cornerRadius
        }
    }
}

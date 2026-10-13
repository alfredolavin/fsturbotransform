import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import FsTurbo

import "controls" as SC
import "common" as Common

// The mini-console's format (console.conf, read by the terminal dashboard and by the GUI's console
// view), edited with the shared controls of plasmoids/shared: every setting has its own icon inside
// its control, colours can be system colours (stored as the scheme role). Changes apply at once and
// are saved after a short pause; the preview is a terminal frame drawn by the terminal's renderer.
QQC2.ApplicationWindow {
    id: dialog

    readonly property var fmt: ConsoleFormat.values

    title: qsTr("Console format")
    width: 980
    height: 820
    minimumWidth: 760
    minimumHeight: 560
    color: Kirigami.Theme.backgroundColor
    flags: Qt.Dialog

    // One colour setting: the shared configurable-colour button with the system colours as a source.
    // (An inline component cannot see the ids of this file: it reads ConsoleFormat directly.)
    component ColorSetting: RowLayout {
        id: setting
        required property string key
        required property string label
        required property string icon
        Layout.fillWidth: true
        spacing: 8
        QQC2.Label {
            text: setting.label
            Layout.preferredWidth: 150
            elide: Text.ElideRight
        }
        Common.SourceColorButton {
            Layout.fillWidth: true
            iconName: setting.icon
            value: ConsoleFormat.values[setting.key]
            dialogTitle: setting.label
            onEdited: v => ConsoleFormat.set(setting.key, v)
        }
    }

    component Section: Kirigami.ShadowedRectangle {
        id: section
        property string title
        default property alias content: body.data
        Layout.fillWidth: true
        implicitHeight: body.implicitHeight + heading.implicitHeight + 34
        Kirigami.Theme.colorSet: Kirigami.Theme.View
        Kirigami.Theme.inherit: false
        color: Kirigami.Theme.backgroundColor
        radius: 10
        border.width: 1
        border.color: Qt.alpha(Kirigami.Theme.textColor, 0.10)
        shadow.size: 10
        shadow.yOffset: 2
        shadow.color: Qt.rgba(0, 0, 0, 0.12)
        Kirigami.Heading {
            id: heading
            x: 12
            y: 10
            level: 4
            text: section.title
            font.weight: Font.DemiBold
        }
        ColumnLayout {
            id: body
            x: 12
            y: heading.y + heading.implicitHeight + 8
            width: section.width - 24
            spacing: 6
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        // ── Preview: a terminal frame (Sixel / Kitty) drawn by the terminal's renderer
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: preview.paintedHeight + 20
            radius: 10
            color: Qt.rgba(0.11, 0.11, 0.13, 1) // a terminal's background behind the transparent corners
            Image {
                id: preview
                anchors.centerIn: parent
                width: parent.width - 20
                fillMode: Image.PreserveAspectFit
                cache: false
                source: "image://console/" + ConsoleFormat.revision
                sourceSize: Qt.size(Math.round(width * Screen.devicePixelRatio), Math.round(9 * 20 * Screen.devicePixelRatio))
            }
            QQC2.Label {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 4
                text: qsTr("Terminal preview")
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                color: "white"
                opacity: 0.6
            }
        }

        QQC2.ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

            GridLayout {
                width: scroll.availableWidth - 14
                x: 4
                columns: width > 820 ? 2 : 1
                columnSpacing: 12
                rowSpacing: 12

                Section {
                    title: qsTr("Font and window")
                    Layout.alignment: Qt.AlignTop

                    SC.IconFontComboBox {
                        Layout.fillWidth: true
                        iconName: "dialog-text-and-font"
                        families: ConsoleFormat.monospaceFamilies
                        defaultText: qsTr("Fira Code (embedded)")
                        family: dialog.fmt.font_family
                        onFamilyEdited: name => ConsoleFormat.set("font_family", name)
                    }
                    SC.IconSpinBox {
                        iconName: "format-font-size-more"
                        from: 60
                        to: 140
                        stepSize: 5
                        suffix: " %"
                        value: dialog.fmt.font_scale
                        onValueModified: ConsoleFormat.set("font_scale", value)
                        Accessible.name: qsTr("Text size")
                    }
                    SC.IconTextField {
                        Layout.fillWidth: true
                        iconName: "draw-text"
                        placeholderText: qsTr("Window title")
                        text: dialog.fmt.title
                        onTextEdited: ConsoleFormat.set("title", text)
                    }
                    SC.IconCheckBox {
                        iconName: "window-close"
                        text: qsTr("Window buttons in the title bar")
                        checked: dialog.fmt.traffic_lights
                        onToggled: ConsoleFormat.set("traffic_lights", checked)
                    }
                    SC.IconCheckBox {
                        iconName: "preferences-desktop-icons"
                        text: qsTr("Application icon beside the title")
                        checked: dialog.fmt.app_icon
                        onToggled: ConsoleFormat.set("app_icon", checked)
                    }
                    ColorSetting { key: "background"; label: qsTr("Background"); icon: "color-fill" }
                    ColorSetting { key: "title_background"; label: qsTr("Title bar"); icon: "window-new" }
                    ColorSetting { key: "title_color"; label: qsTr("Title"); icon: "format-text-color" }
                    ColorSetting { key: "border"; label: qsTr("Border"); icon: "object-stroke" }
                }

                Section {
                    title: qsTr("Progress bar and cursor")
                    Layout.alignment: Qt.AlignTop

                    ColorSetting { key: "track"; label: qsTr("Empty track"); icon: "paint-none" }
                    ColorSetting { key: "gradient_start"; label: qsTr("Gradient start"); icon: "align-horizontal-left" }
                    ColorSetting { key: "gradient_middle"; label: qsTr("Gradient middle"); icon: "align-horizontal-center" }
                    ColorSetting { key: "gradient_end"; label: qsTr("Gradient end"); icon: "align-horizontal-right" }
                    SC.IconCheckBox {
                        iconName: "draw-star"
                        text: qsTr("A light travels along the bar")
                        checked: dialog.fmt.shimmer
                        onToggled: ConsoleFormat.set("shimmer", checked)
                    }
                    ColorSetting { key: "prompt"; label: qsTr("Prompt and cursor"); icon: "go-next" }
                    SC.IconCheckBox {
                        iconName: "object-visible"
                        text: qsTr("The cursor blinks")
                        checked: dialog.fmt.blink_cursor
                        onToggled: ConsoleFormat.set("blink_cursor", checked)
                    }
                }

                Section {
                    title: qsTr("Lines")
                    Layout.alignment: Qt.AlignTop
                    Layout.columnSpan: parent.columns

                    GridLayout {
                        Layout.fillWidth: true
                        columns: width > 760 ? 2 : 1
                        columnSpacing: 18
                        rowSpacing: 6
                        ColorSetting { key: "action"; label: qsTr("Action"); icon: "gears" }
                        SC.IconCheckBox {
                            iconName: "format-text-bold"
                            text: qsTr("Actions in bold")
                            checked: dialog.fmt.bold_actions
                            onToggled: ConsoleFormat.set("bold_actions", checked)
                        }
                        ColorSetting { key: "directory"; label: qsTr("Directory"); icon: "folder" }
                        ColorSetting { key: "name"; label: qsTr("Name"); icon: "text-plain" }
                        ColorSetting { key: "extension"; label: qsTr("Extension"); icon: "preferences-desktop-filetype-association" }
                        ColorSetting { key: "number"; label: qsTr("Digits"); icon: "draw-number" }
                        ColorSetting { key: "arrow"; label: qsTr("Arrow"); icon: "transform-move-horizontal" }
                        ColorSetting { key: "new_name"; label: qsTr("New name"); icon: "edit-rename" }
                        ColorSetting { key: "note"; label: qsTr("Notes and statistics"); icon: "knotes" }
                        ColorSetting { key: "error"; label: qsTr("Errors"); icon: "dialog-error" }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            SC.IconTextButton {
                iconName: "edit-reset"
                label: qsTr("Defaults")
                onClicked: ConsoleFormat.reset()
            }
            QQC2.Label {
                Layout.fillWidth: true
                text: qsTr("Saved to %1 — the terminal reads it too").arg(ConsoleFormat.path)
                elide: Text.ElideMiddle
                opacity: 0.65
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }
            SC.IconTextButton {
                iconName: "dialog-close"
                label: qsTr("Close")
                onClicked: dialog.close()
            }
        }
        SC.AuthorSignature { Layout.alignment: Qt.AlignRight }
    }
}

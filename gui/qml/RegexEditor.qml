import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import FsTurbo

// A one-line regex editor with syntax highlighting (RegexHighlighter: the engine's own reading
// of the spec), the property's icon inside at the left, and the engine's error below it.
ColumnLayout {
    id: root

    property alias text: area.text
    property alias placeholderText: area.placeholderText
    property alias iconName: icon.name
    property int mode: RegexHighlighter.Rename
    property string error
    property int errorPosition: -1
    readonly property alias editor: area

    // The user changed the text (not emitted for changes made through `text`).
    signal edited(string text)

    spacing: 4

    QQC2.TextArea {
        id: area

        property bool settingText: false

        Layout.fillWidth: true
        leftPadding: 28
        wrapMode: TextEdit.WrapAnywhere
        textFormat: TextEdit.PlainText
        font.family: "Fira Code"
        font.pointSize: Kirigami.Theme.defaultFont.pointSize
        selectByMouse: true
        Accessible.name: placeholderText

        // One line: Return does nothing (an x-flag pattern may still be long; it wraps).
        Keys.onReturnPressed: event => event.accepted = true
        Keys.onEnterPressed: event => event.accepted = true
        onTextChanged: if (activeFocus) root.edited(text)

        FontMetrics {
            id: lineMetrics
            font: area.font
        }

        // In a text area the icon sits on the first line rather than in the middle.
        PropertyIcon {
            id: icon
            anchors.verticalCenter: undefined
            y: area.topPadding + (lineMetrics.height - height) / 2
        }

        RegexHighlighter {
            document: area.textDocument
            mode: root.mode
            errorPosition: root.errorPosition
            dark: Kirigami.ColorUtils.brightnessForColor(Kirigami.Theme.backgroundColor) === Kirigami.ColorUtils.Dark
        }
    }

    RowLayout {
        visible: root.error !== ""
        spacing: 6
        Layout.fillWidth: true
        Item {
            implicitWidth: 16
            implicitHeight: 16
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: 1
            PropertyIcon { name: "error"; x: 0 }
        }
        QQC2.Label {
            text: root.error
            color: Kirigami.Theme.negativeTextColor
            wrapMode: Text.Wrap
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            Layout.fillWidth: true
        }
    }
}

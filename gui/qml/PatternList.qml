import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import FsTurbo

// An editable list of -e / -i patterns: a glob, or a /regex/flags (highlighted, and checked by
// the engine as you type).
ColumnLayout {
    id: root

    property var patterns: []
    property string iconName
    property string placeholderText
    property string addLabel

    signal patternsEdited(var patterns)

    spacing: 6

    function replaced(index, value) {
        const copy = root.patterns.slice();
        if (value === undefined) copy.splice(index, 1);
        else copy[index] = value;
        return copy;
    }

    Repeater {
        // A count, not the list: editing one entry must not rebuild (and unfocus) the others.
        model: root.patterns.length

        delegate: RowLayout {
            id: row
            required property int index
            readonly property var check: TransformController.checkFilter(root.patterns[index] ?? "")
            Layout.fillWidth: true
            spacing: 4

            RegexEditor {
                Layout.fillWidth: true
                mode: RegexHighlighter.Filter
                iconName: root.iconName
                placeholderText: root.placeholderText
                text: root.patterns[row.index] ?? ""
                error: row.check.error
                errorPosition: row.check.position
                onEdited: value => root.patternsEdited(root.replaced(row.index, value))
            }
            QQC2.ToolButton {
                Layout.alignment: Qt.AlignTop
                implicitWidth: 30
                implicitHeight: 30
                Accessible.name: qsTr("Remove this pattern")
                QQC2.ToolTip.text: Accessible.name
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                onClicked: root.patternsEdited(root.replaced(row.index))
                PropertyIcon { name: "remove"; x: (parent.width - width) / 2 }
            }
        }
    }

    IconButton {
        iconName: "add"
        label: root.addLabel
        onClicked: root.patternsEdited(root.patterns.concat([""]))
    }
}

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami
import FsTurbo

// An editable list of .gitignore-style files, each with a browse button.
ColumnLayout {
    id: root

    property var files: []
    property string iconName
    property string addLabel

    signal filesEdited(var files)

    spacing: 6

    function replaced(index, value) {
        const copy = root.files.slice();
        if (value === undefined) copy.splice(index, 1);
        else copy[index] = value;
        return copy;
    }

    FileDialog {
        id: dialog
        property int index: -1
        title: qsTr("Choose a .gitignore-style file")
        nameFilters: [qsTr("Ignore files (.gitignore *.ignore)"), qsTr("All files (*)")]
        onAccepted: root.filesEdited(root.replaced(index, TransformController.pathFromUrl(selectedFile)))
    }

    Repeater {
        model: root.files.length

        delegate: RowLayout {
            id: row
            required property int index
            Layout.fillWidth: true
            spacing: 4

            IconTextField {
                Layout.fillWidth: true
                iconName: root.iconName
                placeholderText: qsTr("Path to a .gitignore file")
                text: root.files[row.index] ?? ""
                onTextEdited: root.filesEdited(root.replaced(row.index, text))
            }
            QQC2.ToolButton {
                implicitWidth: 30
                Accessible.name: qsTr("Browse…")
                QQC2.ToolTip.text: Accessible.name
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                onClicked: {
                    dialog.index = row.index;
                    dialog.open();
                }
                PropertyIcon { name: "browse"; x: (parent.width - width) / 2 }
            }
            QQC2.ToolButton {
                implicitWidth: 30
                Accessible.name: qsTr("Remove this file")
                QQC2.ToolTip.text: Accessible.name
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                onClicked: root.filesEdited(root.replaced(row.index))
                PropertyIcon { name: "remove"; x: (parent.width - width) / 2 }
            }
        }
    }

    IconButton {
        iconName: "add"
        label: root.addLabel
        onClicked: root.filesEdited(root.files.concat([""]))
    }
}

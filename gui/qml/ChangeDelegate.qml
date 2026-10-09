import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// One row of the change list: the kind of change (its icon), the entry, and its new name.
QQC2.ItemDelegate {
    id: row

    required property int index
    required property string kind
    required property string source
    required property string dest

    readonly property bool isError: kind === "error"

    width: ListView.view ? ListView.view.width : implicitWidth
    leftPadding: 28
    leftInset: 0
    rightInset: 0
    topInset: 0
    bottomInset: 0
    topPadding: 4
    bottomPadding: 4
    hoverEnabled: true
    highlighted: false
    QQC2.ToolTip.text: isError ? source : source + "  →  " + dest
    QQC2.ToolTip.visible: hovered && (sourceText.truncated || destText.truncated)
    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

    background: Rectangle {
        color: row.isError ? Qt.alpha(Kirigami.Theme.negativeTextColor, 0.10)
             : row.hovered ? Qt.alpha(Kirigami.Theme.highlightColor, 0.12)
             : row.index % 2 ? Qt.alpha(Kirigami.Theme.textColor, 0.035) : "transparent"
        radius: 6
    }

    contentItem: RowLayout {
        spacing: 8
        QQC2.Label {
            id: sourceText
            text: row.source
            font.family: "Fira Code"
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: row.isError ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
            opacity: row.isError ? 1 : 0.8
            elide: row.isError ? Text.ElideRight : Text.ElideMiddle
            Layout.fillWidth: true
            Layout.preferredWidth: 1
        }
        QQC2.Label {
            visible: !row.isError
            text: "→"
            font.family: "Fira Code"
            color: Kirigami.Theme.disabledTextColor
        }
        QQC2.Label {
            id: destText
            visible: !row.isError
            text: row.dest
            font.family: "Fira Code"
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            font.weight: Font.DemiBold
            color: Kirigami.Theme.positiveTextColor
            elide: Text.ElideMiddle
            Layout.fillWidth: true
            Layout.preferredWidth: 1
        }
    }

    PropertyIcon {
        name: row.kind === "flatten" ? "flatten" : row.kind === "dir" ? "folder" : row.kind === "error" ? "error" : "file"
        emblem: row.kind === "dir" || row.kind === "file" ? "rename" : ""
    }
}

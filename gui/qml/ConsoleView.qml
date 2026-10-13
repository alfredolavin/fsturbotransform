import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import FsTurbo

import "common" as Common
import "common/ColorSources.js" as Sources

// The GUI's mini-console: the run's changes and errors drawn with the console format (ConsoleFormat,
// the file the terminal dashboard reads too): its font, window colours, title bar and the colour of
// every part of a line, paths split as the terminal splits them (directory / name / digits / extension).
Item {
    id: view

    property var model
    property bool running: false
    property bool hasResults: false
    property string emptyText

    readonly property var fmt: ConsoleFormat.values
    // A stored colour (colorspec string) as a QML colour; system colours follow the scheme live.
    // (Sources.css() gives a CSS "rgba(…)", which a QML colour does not read.)
    function colour(key) {
        const c = Sources.resolve(fmt[key], [], 0, sys.map, ConsoleFormat.defaults[key]);
        return Qt.rgba(c.r, c.g, c.b, c.a);
    }
    readonly property color background: colour("background")
    readonly property color titleBackground: colour("title_background")
    readonly property color border: colour("border")
    readonly property color titleColour: colour("title_color")
    readonly property var roles: ({
        prompt: colour("prompt"), action: colour("action"), directory: colour("directory"), name: colour("name"),
        extension: colour("extension"), number: colour("number"), arrow: colour("arrow"), new_name: colour("new_name"),
        note: colour("note"), error: colour("error")
    })
    readonly property string fontFamily: fmt.font_family || "Fira Code"
    readonly property real fontSize: Kirigami.Theme.defaultFont.pointSize * fmt.font_scale / 100

    Common.SystemTheme { id: sys }

    function htmlEscape(s) {
        return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
    }
    function span(text, c) {
        return text === "" ? "" : "<font color=\"" + String(c) + "\">" + htmlEscape(text) + "</font>";
    }
    // term::highlight_path(): directory / name (digit runs as numbers) / extension
    function path(p, nameRole) {
        let out = "";
        let name = p;
        const slash = p.lastIndexOf("/");
        if (slash >= 0) {
            out += span(p.substring(0, slash + 1), roles.directory);
            name = p.substring(slash + 1);
        }
        let ext = "";
        const dot = name.lastIndexOf(".");
        if (dot > 0) {
            ext = name.substring(dot);
            name = name.substring(0, dot);
        }
        for (const part of name.match(/\d+|\D+/g) || [])
            out += span(part, /^\d/.test(part) ? roles.number : roles[nameRole]);
        return out + span(ext, roles.extension);
    }

    Rectangle {
        anchors.fill: parent
        radius: 10
        color: view.background
        border.color: view.border
        border.width: 1
        clip: true

        // ── Title bar: traffic lights, app icon and title, the line count on the right
        Rectangle {
            id: titleBar
            x: 1
            y: 1
            width: parent.width - 2
            height: Math.round(titleText.implicitHeight * 1.6)
            radius: 9
            color: view.titleBackground
            Rectangle { // square lower corners
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: parent.radius
                color: parent.color
            }
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: view.border
            }
            Row {
                visible: view.fmt.traffic_lights
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6
                Repeater {
                    model: ["#ff5f56", "#ffbd2e", "#27c93f"]
                    Rectangle {
                        required property string modelData
                        width: 10
                        height: 10
                        radius: 5
                        color: modelData
                    }
                }
            }
            Row {
                anchors.centerIn: parent
                spacing: 6
                Image {
                    visible: view.fmt.app_icon
                    anchors.verticalCenter: parent.verticalCenter
                    source: "icons/app.png"
                    width: titleText.implicitHeight
                    height: width
                    sourceSize: Qt.size(width * Screen.devicePixelRatio, height * Screen.devicePixelRatio)
                }
                Text {
                    id: titleText
                    text: view.fmt.title
                    color: view.titleColour
                    font.family: view.fontFamily
                    font.styleName: "" // a desktop font with a style ("Light") would ignore the weight
                    font.pointSize: view.fontSize
                    font.weight: Font.Bold
                }
            }
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                text: view.model ? (view.model.count === 1 ? qsTr("1 line") : qsTr("%1 lines").arg(view.model.count)) : ""
                color: view.roles.note
                font.family: view.fontFamily
                font.pointSize: view.fontSize
            }
        }

        ListView {
            id: list
            anchors.fill: parent
            anchors.topMargin: titleBar.height + 4
            anchors.leftMargin: 6
            anchors.rightMargin: 2
            anchors.bottomMargin: 4
            clip: true
            model: view.model
            reuseItems: true
            QQC2.ScrollBar.vertical: QQC2.ScrollBar {}

            delegate: Item {
                id: row
                required property int index
                required property string kind
                required property string source
                required property string dest
                readonly property bool isError: kind === "error"
                readonly property bool last: index === list.count - 1
                readonly property string action: kind === "flatten" ? "FLATTEN" : kind === "dir" ? "DIR RENAME" : kind === "file" ? "FILE RENAME" : "ERROR"

                width: list.width - 10
                height: line.implicitHeight + 4
                QQC2.ToolTip.text: isError ? source : source + "  →  " + dest
                QQC2.ToolTip.visible: hover.hovered && line.truncated
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                HoverHandler { id: hover }

                // The kind of change (its icon), then the line as the terminal prints it
                PropertyIcon {
                    name: row.kind === "flatten" ? "flatten" : row.kind === "dir" ? "folder" : row.kind === "error" ? "error" : "file"
                    emblem: row.kind === "dir" || row.kind === "file" ? "rename" : ""
                    x: 2
                }
                Text {
                    id: line
                    x: 24
                    width: parent.width - x
                    anchors.verticalCenter: parent.verticalCenter
                    textFormat: Text.StyledText
                    elide: Text.ElideRight
                    font.family: view.fontFamily
                    font.styleName: ""
                    font.pointSize: view.fontSize
                    readonly property string tag: (view.fmt.bold_actions ? "<b>" : "") + view.span((row.action + "            ").substring(0, 12), view.roles.action)
                                                  + (view.fmt.bold_actions ? "</b>" : "")
                    text: view.span(row.last ? "› " : "· ", row.last ? view.roles.prompt : view.roles.note)
                          + (row.isError ? view.span(row.source, view.roles.error)
                                         : tag + view.path(row.source, "name") + view.span(" → ", view.roles.arrow) + view.path(row.dest, "new_name"))
                }
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: list
            width: list.width - 40
            visible: list.count === 0
            text: view.emptyText
            Kirigami.Theme.textColor: view.roles.note
        }
    }
}

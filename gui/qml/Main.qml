import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami
import FsTurbo

QQC2.ApplicationWindow {
    id: window

    readonly property var controller: TransformController
    readonly property var stats: controller.stats
    readonly property bool hasStats: stats.durationMs !== undefined
    property string summary

    width: 1220
    height: 840
    minimumWidth: 940
    minimumHeight: 620
    visible: true
    title: "FS-Turbo-Transformer"
    color: Kirigami.Theme.backgroundColor

    function number(key) {
        return window.hasStats ? Number(window.stats[key]).toLocaleString(Qt.locale(), "f", 0) : "–";
    }

    Connections {
        target: window.controller
        function onFinished(text) { window.summary = text; }
        function onRunStateChanged() { if (window.controller.running) window.summary = ""; }
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Choose the directory to transform")
        currentFolder: window.controller.urlFromPath(window.controller.targetDir)
        onAccepted: window.controller.targetDir = window.controller.pathFromUrl(selectedFolder)
    }

    QQC2.Dialog {
        id: confirmApply
        anchors.centerIn: parent
        modal: true
        title: qsTr("Apply the changes?")
        width: Math.min(window.width - 80, 560)

        contentItem: QQC2.Label {
            wrapMode: Text.Wrap
            text: qsTr("Files and directories in <b>%1</b> will be renamed%2 for real. This cannot be undone automatically; run a preview first if you have not.")
                    .arg(window.controller.targetDir)
                    .arg(window.controller.flatten || window.controller.flattenRegex !== "" ? qsTr(" and flattened") : "")
        }

        footer: QQC2.DialogButtonBox {
            IconButton {
                iconName: "apply"
                label: qsTr("Apply")
                QQC2.DialogButtonBox.buttonRole: QQC2.DialogButtonBox.AcceptRole
            }
            IconButton {
                iconName: "stop"
                label: qsTr("Cancel")
                QQC2.DialogButtonBox.buttonRole: QQC2.DialogButtonBox.RejectRole
            }
        }
        onAccepted: window.controller.apply()
    }

    QQC2.Popup {
        id: expressionHelp
        x: Math.round((window.width - width) / 2)
        y: 90
        width: Math.min(window.width - 60, 640)
        padding: 16
        modal: false
        closePolicy: QQC2.Popup.CloseOnEscape | QQC2.Popup.CloseOnPressOutside
        background: Kirigami.ShadowedRectangle {
            Kirigami.Theme.colorSet: Kirigami.Theme.View
            Kirigami.Theme.inherit: false
            color: Kirigami.Theme.backgroundColor
            radius: 12
            border.width: 1
            border.color: Qt.alpha(Kirigami.Theme.textColor, 0.15)
            shadow.size: 20
            shadow.color: Qt.rgba(0, 0, 0, 0.25)
        }
        contentItem: QQC2.Label {
            textFormat: Text.StyledText
            wrapMode: Text.Wrap
            text: qsTr("<b>/pattern/replacement/flags</b> — PCRE2: lookahead/lookbehind, \\K, UTF-8. Flags: <tt>i</tt> caseless, <tt>m</tt> multiline, <tt>s</tt> dot-all, <tt>x</tt> extended.<br><br>"
                     + "<b>Replacement:</b> <tt>$1 \\1</tt> groups, <tt>$&lt;name&gt; \\&lt;name&gt;</tt> named groups, <tt>\\U \\L \\E \\C</tt> case.<br><br>"
                     + "<b>Expressions:</b> <tt>(?&lt;n=&gt;\\d+)</tt> makes the group a number; <tt>\\&lt;n=&gt;{round(n/2)}</tt> is replaced by the JavaScript expression's value "
                     + "(<tt>\\&lt;n=v&gt;{…}</tt> names it <tt>v</tt>; default <tt>val</tt>). Everything in Math is in scope.<br><br>"
                     + "<b>Variables:</b> <tt>index</tt> (file number; directories count apart), <tt>nameIndex</tt> (among same-named entries), "
                     + "<tt>nameLength</tt>, <tt>depth</tt> (directories below the target), <tt>$0 $1 $name</tt> (raw text).<br><br>"
                     + "e.g. <tt>/(?&lt;d=&gt;\\d+)-(?&lt;m=&gt;\\d+)/\\&lt;d=&gt;{round(d*10+m/2)}_\\&lt;m&gt;/</tt>")
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        // ── Header: the terminal banner's gradient, icon and title ─────────────────────────
        Kirigami.ShadowedRectangle {
            Layout.fillWidth: true
            implicitHeight: 64
            radius: 14
            shadow.size: 12
            shadow.yOffset: 2
            shadow.color: Qt.rgba(0, 0, 0, 0.18)
            color: "#8f44f0"

            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "#00c6e0" }
                    GradientStop { position: 0.55; color: "#8f44f0" }
                    GradientStop { position: 1.0; color: "#f0388e" }
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 18
                spacing: 12
                Image {
                    source: "icons/app.png"
                    sourceSize: Qt.size(48 * Screen.devicePixelRatio, 48 * Screen.devicePixelRatio)
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                }
                ColumnLayout {
                    spacing: 0
                    Text {
                        text: "FS-TURBO-TRANSFORMER"
                        color: "white"
                        style: Text.Outline
                        styleColor: "black"
                        font.family: "Fira Code"
                        font.pixelSize: 22
                        font.weight: Font.Bold
                    }
                    Text {
                        text: qsTr("Rename · re-case · flatten directory trees — v%1").arg(Qt.application.version)
                        color: "white"
                        style: Text.Outline
                        styleColor: Qt.rgba(0, 0, 0, 0.6)
                        font: Kirigami.Theme.smallFont
                    }
                }
                Item { Layout.fillWidth: true }
            }
        }

        QQC2.SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            // ── Options ───────────────────────────────────────────────────────────────────
            QQC2.ScrollView {
                id: optionsScroll
                QQC2.SplitView.preferredWidth: 500
                QQC2.SplitView.minimumWidth: 400
                contentWidth: availableWidth
                QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

                ColumnLayout {
                    width: optionsScroll.availableWidth - 12
                    x: 4
                    spacing: 14

                    Item { implicitHeight: 2 }

                    Card {
                        title: qsTr("Target")
                        iconName: "target-folder"
                        Layout.fillWidth: true

                        RowLayout {
                            spacing: 6
                            Layout.fillWidth: true
                            IconTextField {
                                id: targetField
                                Layout.fillWidth: true
                                iconName: "target-folder"
                                placeholderText: qsTr("Directory to transform")
                                text: window.controller.targetDir
                                onTextEdited: window.controller.targetDir = text
                            }
                            IconButton {
                                iconName: "browse"
                                label: qsTr("Browse…")
                                onClicked: folderDialog.open()
                            }
                        }
                        QQC2.Label {
                            visible: window.controller.targetError !== ""
                            text: window.controller.targetError
                            color: Kirigami.Theme.negativeTextColor
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                        }
                        IconCheckBox {
                            iconName: "recursive"
                            text: qsTr("Recurse into subdirectories")
                            checked: window.controller.recursive
                            onToggled: window.controller.recursive = checked
                        }
                    }

                    Card {
                        title: qsTr("Rename")
                        iconName: "rename"
                        Layout.fillWidth: true

                        QQC2.Label { text: qsTr("Case"); font.weight: Font.DemiBold }
                        IconComboBox {
                            id: caseCombo
                            Layout.fillWidth: true
                            iconName: "case-style"
                            readonly property var names: [qsTr("Unchanged"), "lowercase", "UPPERCASE", "snake_case", "camelCase", "PascalCase", "kebab-case", "Title Case"]
                            model: names
                            widestText: "Title Case — my file name.txt"
                            currentIndex: window.controller.caseStyle
                            onActivated: index => window.controller.caseStyle = index
                            delegate: QQC2.ItemDelegate {
                                required property int index
                                required property string modelData
                                width: ListView.view ? ListView.view.width : implicitWidth
                                highlighted: caseCombo.highlightedIndex === index
                                contentItem: RowLayout {
                                    spacing: 10
                                    QQC2.Label { text: modelData; Layout.preferredWidth: 110 }
                                    QQC2.Label {
                                        text: window.controller.caseExample(index, "My File name")
                                        font.family: "Fira Code"
                                        opacity: 0.6
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                        }

                        RowLayout {
                            spacing: 4
                            QQC2.Label { text: qsTr("Regular expression"); font.weight: Font.DemiBold }
                            Item { Layout.fillWidth: true }
                            QQC2.ToolButton {
                                implicitWidth: 28
                                implicitHeight: 28
                                Accessible.name: qsTr("Expression syntax")
                                QQC2.ToolTip.text: Accessible.name
                                QQC2.ToolTip.visible: hovered
                                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                                onClicked: expressionHelp.open()
                                PropertyIcon { name: "info"; x: (parent.width - width) / 2 }
                            }
                        }
                        RegexEditor {
                            Layout.fillWidth: true
                            mode: RegexHighlighter.Rename
                            iconName: "regex-rename"
                            placeholderText: "/pattern/replacement/flags"
                            text: window.controller.regex
                            error: window.controller.regexError
                            errorPosition: window.controller.regexErrorPosition
                            onEdited: value => window.controller.regex = value
                        }

                        QQC2.Label { text: qsTr("Try it on a name"); font.weight: Font.DemiBold }
                        IconTextField {
                            Layout.fillWidth: true
                            iconName: "file"
                            font.family: "Fira Code"
                            placeholderText: qsTr("Sample file name")
                            text: window.controller.sampleName
                            onTextEdited: window.controller.sampleName = text
                        }
                        IconTextField {
                            Layout.fillWidth: true
                            iconName: window.controller.sampleError !== "" ? "error" : "rename"
                            font.family: "Fira Code"
                            font.weight: Font.DemiBold
                            readOnly: true
                            placeholderText: qsTr("New name")
                            color: window.controller.sampleError !== "" ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.positiveTextColor
                            text: window.controller.sampleError !== "" ? window.controller.sampleError : window.controller.sampleResult
                        }
                    }

                    Card {
                        title: qsTr("Flatten")
                        iconName: "flatten"
                        Layout.fillWidth: true

                        IconCheckBox {
                            iconName: "flatten"
                            text: qsTr("Move every file up into the target directory")
                            checked: window.controller.flatten
                            onToggled: window.controller.flatten = checked
                        }
                        QQC2.Label {
                            text: qsTr("Only the paths matching (works without the box above too)")
                            font.weight: Font.DemiBold
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }
                        RegexEditor {
                            Layout.fillWidth: true
                            mode: RegexHighlighter.Select
                            iconName: "flatten-regex"
                            placeholderText: "/pattern/flags"
                            text: window.controller.flattenRegex
                            error: window.controller.flattenRegexError
                            errorPosition: window.controller.flattenRegexErrorPosition
                            onEdited: value => window.controller.flattenRegex = value
                        }
                    }

                    Card {
                        title: qsTr("Filters")
                        iconName: "flatten-regex"
                        Layout.fillWidth: true

                        QQC2.Label { text: qsTr("Exclude (glob or /regex/)"); font.weight: Font.DemiBold }
                        PatternList {
                            Layout.fillWidth: true
                            iconName: "exclude"
                            placeholderText: "*.log  **/*.o  /^test_/i"
                            addLabel: qsTr("Add an exclusion")
                            patterns: window.controller.excludes
                            onPatternsEdited: list => window.controller.excludes = list
                        }
                        QQC2.Label { text: qsTr("Include (overrides every exclusion)"); font.weight: Font.DemiBold }
                        PatternList {
                            Layout.fillWidth: true
                            iconName: "include"
                            placeholderText: "keep/**  /\\.keep$/"
                            addLabel: qsTr("Add an inclusion")
                            patterns: window.controller.includes
                            onPatternsEdited: list => window.controller.includes = list
                        }
                        QQC2.Label { text: qsTr(".gitignore files"); font.weight: Font.DemiBold }
                        FileList {
                            Layout.fillWidth: true
                            iconName: "gitignore-file"
                            addLabel: qsTr("Add a .gitignore file")
                            files: window.controller.gitignoreFiles
                            onFilesEdited: list => window.controller.gitignoreFiles = list
                        }
                        IconCheckBox {
                            iconName: "target-gitignore"
                            text: qsTr("Apply the target directory's own .gitignore")
                            checked: window.controller.targetGitignore
                            onToggled: window.controller.targetGitignore = checked
                        }
                    }

                    Card {
                        title: qsTr("Collisions")
                        iconName: "overwrite"
                        Layout.fillWidth: true

                        IconCheckBox {
                            iconName: "overwrite"
                            text: qsTr("Overwrite existing entries on name collisions")
                            checked: window.controller.overwrite
                            onToggled: window.controller.overwrite = checked
                        }
                    }

                    Item { implicitHeight: 6 }
                }
            }

            // ── Run and results ───────────────────────────────────────────────────────────
            ColumnLayout {
                QQC2.SplitView.fillWidth: true
                QQC2.SplitView.minimumWidth: 420
                spacing: 14

                Card {
                    Layout.fillWidth: true
                    Layout.leftMargin: 8
                    Layout.topMargin: 6

                    RowLayout {
                        spacing: 8
                        Layout.fillWidth: true
                        IconButton {
                            iconName: "preview"
                            label: qsTr("Preview")
                            enabled: window.controller.canRun
                            QQC2.ToolTip.text: qsTr("Dry run: list what would change, touching nothing")
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                            onClicked: window.controller.preview()
                        }
                        IconButton {
                            iconName: "apply"
                            label: qsTr("Apply…")
                            enabled: window.controller.canRun
                            onClicked: confirmApply.open()
                        }
                        IconButton {
                            iconName: "stop"
                            label: qsTr("Stop")
                            enabled: window.controller.running
                            onClicked: window.controller.cancel()
                        }
                        Item { Layout.fillWidth: true }
                    }

                    GradientProgress {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        iconName: window.controller.phase === "FLATTEN" ? "flatten" : "rename"
                        from: 0
                        to: Math.max(1, window.controller.progressTotal)
                        value: window.controller.running || window.hasStats ? window.controller.progressCurrent : 0
                        indeterminate: window.controller.running && window.controller.progressTotal === 0
                        caption: window.controller.running
                                 ? qsTr("%1  %2 / %3").arg(window.controller.phase || qsTr("SCANNING"))
                                       .arg(window.controller.progressCurrent.toLocaleString(Qt.locale(), "f", 0))
                                       .arg(window.controller.progressTotal.toLocaleString(Qt.locale(), "f", 0))
                                 : window.hasStats ? (window.controller.wasCancelled ? qsTr("STOPPED") : qsTr("DONE")) : qsTr("READY")
                    }

                    Kirigami.InlineMessage {
                        Layout.fillWidth: true
                        visible: window.summary !== ""
                        text: window.summary
                        type: window.hasStats && window.stats.errors > 0 ? Kirigami.MessageType.Warning
                            : window.controller.previewRun ? Kirigami.MessageType.Information : Kirigami.MessageType.Positive
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 8
                    columns: Math.max(2, Math.floor((width + columnSpacing) / (185 + columnSpacing)))
                    columnSpacing: 10
                    rowSpacing: 10

                    StatTile { Layout.fillWidth: true; iconName: "folder"; label: qsTr("Directories scanned"); value: window.number("scannedDirs") }
                    StatTile { Layout.fillWidth: true; iconName: "file"; label: qsTr("Files scanned"); value: window.number("scannedFiles") }
                    StatTile {
                        Layout.fillWidth: true; iconName: "folder"; emblem: "rename"
                        label: window.controller.previewRun ? qsTr("Directories to rename") : qsTr("Directories renamed")
                        value: window.number("renamedDirs"); accent: "#1d99f3"
                    }
                    StatTile {
                        Layout.fillWidth: true; iconName: "file"; emblem: "rename"
                        label: window.controller.previewRun ? qsTr("Files to rename") : qsTr("Files renamed")
                        value: window.number("renamedFiles"); accent: "#1d99f3"
                    }
                    StatTile {
                        Layout.fillWidth: true; iconName: "flatten"
                        label: window.controller.previewRun ? qsTr("Files to flatten") : qsTr("Files flattened")
                        value: window.number("flattenedFiles"); accent: "#9b59b6"
                    }
                    StatTile { Layout.fillWidth: true; iconName: "exclude"; label: qsTr("Excluded"); value: window.number("excludedItems") }
                    StatTile {
                        Layout.fillWidth: true; iconName: "error"; label: qsTr("Errors"); value: window.number("errors")
                        accent: window.hasStats && window.stats.errors > 0 ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
                    }
                    StatTile {
                        Layout.fillWidth: true; iconName: "duration"; label: qsTr("Duration")
                        value: window.hasStats ? (window.stats.durationMs < 1000 ? qsTr("%1 ms").arg(window.stats.durationMs.toFixed(1))
                                                                                : qsTr("%1 s").arg((window.stats.durationMs / 1000).toFixed(2))) : "–"
                    }
                }

                Card {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.leftMargin: 8
                    Layout.bottomMargin: 6
                    title: window.controller.previewRun ? qsTr("Changes that would be made") : qsTr("Changes made")
                    iconName: "preview"
                    headerExtra: QQC2.Label {
                        text: window.controller.changes.count.toLocaleString(Qt.locale(), "f", 0)
                        opacity: 0.7
                    }

                    QQC2.ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 120

                        ListView {
                            id: changeList
                            clip: true
                            model: window.controller.changes
                            spacing: 1
                            reuseItems: true
                            delegate: ChangeDelegate {}

                            Kirigami.PlaceholderMessage {
                                anchors.centerIn: parent
                                width: parent.width - 40
                                visible: changeList.count === 0
                                icon.source: window.controller.hasResults ? "" : "image://icon/preview?" + Kirigami.Theme.textColor
                                text: window.controller.running ? qsTr("Working…")
                                    : window.controller.hasResults ? qsTr("Nothing to change")
                                    : qsTr("Press Preview to see what would change")
                            }
                        }
                    }
                }
            }
        }
    }
}

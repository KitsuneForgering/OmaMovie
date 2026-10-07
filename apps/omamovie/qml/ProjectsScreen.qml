import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The Projects screen (ui-design §3): recent projects and Omarchy recordings. It reads the
// window's `root`, `actions` and `session` from the document that places it.
Item {
    anchors.fill: parent
    visible: !session.editing

    RowLayout {
        id: projectsBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        height: 60
        UiText { text: "OmaMovie"; color: root.fg; font.pixelSize: 18; font.bold: true }
        Item { Layout.fillWidth: true }
        OmaButton { objectName: "settingsButton"; action: actions.settings; iconName: "settings"; showLabel: false }
        OmaButton { objectName: "openProject"; action: actions.openProject; iconName: "open" }
        OmaButton {
            objectName: "newProject"
            text: "New project"
            primary: true
            onClicked: root.confirmDiscard("Start a new project", () => session.newProject())
        }
    }
    Rectangle { anchors.top: projectsBar.bottom; width: parent.width; height: 1; color: root.line }

    ColumnLayout {
        anchors.top: projectsBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 32
        spacing: 14
        UiText {
            Layout.topMargin: 16
            text: "CURRENT PROJECT"
            color: root.muted
            font.pixelSize: 11
            font.bold: true
            font.letterSpacing: 1
        }
        Rectangle {
            Layout.preferredWidth: 280
            Layout.preferredHeight: 170
            radius: 8
            color: colors.dark_background
            visible: session.media.length > 0
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 6
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 4
                    color: root.viewerBackground
                    Image {
                        anchors.fill: parent
                        fillMode: Image.PreserveAspectCrop
                        source: session.media.length ? session.media[0].thumbnail : ""
                    }
                }
                UiText {
                    text: session.projectName + (session.dirty ? " · unsaved changes" : "")
                    color: root.fg
                    font.pixelSize: 13
                    font.bold: true
                }
                UiText {
                    text: session.media.length + " item" + (session.media.length === 1 ? "" : "s") + " in the library"
                    color: root.muted
                    font.pixelSize: 11
                }
            }
            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: session.continueProject() }
        }
        UiText {
            visible: session.media.length === 0
            text: "Nothing open yet. Start a new project or open a saved one."
            color: root.muted
            font.pixelSize: 12
        }

        // Autosaves left behind by a crash (CLAUDE.md §14): restore one as unsaved changes to its
        // project, or let it go.
        UiText {
            Layout.topMargin: 16
            visible: session.recoveredProjects.length > 0
            text: "RECOVERED AFTER AN UNEXPECTED CLOSE"
            color: colors.yellow
            font.pixelSize: 11
            font.bold: true
            font.letterSpacing: 1
        }
        Flow {
            Layout.fillWidth: true
            spacing: 10
            visible: session.recoveredProjects.length > 0
            Repeater {
                model: session.recoveredProjects
                delegate: Rectangle {
                    id: recoveredCard
                    required property var modelData
                    objectName: "recoveredProject"
                    width: 320
                    height: 56
                    radius: 8
                    color: colors.dark_background
                    border.color: colors.yellow
                    border.width: 1
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            UiText { Layout.fillWidth: true; text: recoveredCard.modelData.name; color: root.fg; font.pixelSize: 12; font.bold: true; elide: Text.ElideRight }
                            UiText { Layout.fillWidth: true; text: "autosaved " + recoveredCard.modelData.when; color: root.muted; font.pixelSize: 11; elide: Text.ElideRight }
                        }
                        OmaButton {
                            objectName: "restoreRecovered"
                            text: "Restore"
                            primary: true
                            onClicked: {
                                const file = recoveredCard.modelData.file
                                root.confirmDiscard("Restore " + recoveredCard.modelData.name, () => session.restoreRecovered(file))
                            }
                        }
                        OmaButton {
                            text: "Discard"
                            onClicked: session.discardRecovered(recoveredCard.modelData.file)
                        }
                    }
                }
            }
        }

        // Project files opened or saved before, newest first (ui-design §3).
        UiText {
            Layout.topMargin: 16
            visible: session.recentProjects.length > 0
            text: "RECENT PROJECTS"
            color: root.muted
            font.pixelSize: 11
            font.bold: true
            font.letterSpacing: 1
        }
        Flow {
            Layout.fillWidth: true
            spacing: 10
            visible: session.recentProjects.length > 0
            Repeater {
                model: session.recentProjects
                delegate: Rectangle {
                    id: recentCard
                    required property var modelData
                    objectName: "recentProject"
                    width: 280
                    height: 56
                    radius: 8
                    color: recentHover.hovered ? colors.lighter_background : colors.dark_background
                    HoverHandler { id: recentHover }
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 2
                        UiText {
                            Layout.fillWidth: true
                            text: recentCard.modelData.name
                            color: root.fg
                            font.pixelSize: 12
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        UiText {
                            Layout.fillWidth: true
                            text: recentCard.modelData.when + " · " + recentCard.modelData.folder
                            color: root.muted
                            font.pixelSize: 11
                            elide: Text.ElideMiddle
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: mouse => {
                            const path = recentCard.modelData.path
                            if (mouse.button === Qt.RightButton) {
                                recentMenu.versions = session.projectVersions(path)
                                recentMenu.popup()
                                return
                            }
                            root.confirmDiscard("Open " + recentCard.modelData.name, () => session.openFiles([path]))
                        }
                    }
                    ToolTip.visible: recentHover.hovered
                    ToolTip.delay: 600
                    ToolTip.text: recentCard.modelData.path + "\nRight-click for earlier versions or to remove it from this list"
                    // Earlier versions (kept on each save) open as unsaved changes to this project.
                    Menu {
                        id: recentMenu
                        objectName: "recentMenu"
                        property var versions: []
                        Menu {
                            title: recentMenu.versions.length > 0 ? "Earlier versions" : "No earlier versions yet"
                            enabled: recentMenu.versions.length > 0
                            Repeater {
                                model: recentMenu.versions
                                delegate: MenuItem {
                                    required property var modelData
                                    text: "As it was before the save of " + modelData.when
                                    onTriggered: root.confirmDiscard("Open an earlier version of " + recentCard.modelData.name,
                                                                     () => session.restoreVersion(modelData.file, recentCard.modelData.path))
                                }
                            }
                        }
                        MenuSeparator {}
                        MenuItem { text: "Remove from this list"; onTriggered: session.forgetRecentProject(recentCard.modelData.path) }
                    }
                }
            }
        }

        // Omarchy screen recordings (UX-09): Edit starts a project with the recording on the
        // storyline. A capture still being written cannot be opened yet.
        UiText {
            Layout.topMargin: 16
            text: "RECENT RECORDINGS"
            color: root.muted
            font.pixelSize: 11
            font.bold: true
            font.letterSpacing: 1
        }
        UiText {
            visible: session.recordings.length === 0
            text: "No screen recordings in " + session.recordingsFolder + "."
            color: root.muted
            font.pixelSize: 12
        }
        Flow {
            Layout.fillWidth: true
            spacing: 10
            Repeater {
                model: session.recordings
                delegate: Rectangle {
                    required property var modelData
                    objectName: "recording"
                    width: 280
                    height: 64
                    radius: 8
                    color: colors.dark_background
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 10
                        Rectangle {
                            Layout.preferredWidth: 78
                            Layout.fillHeight: true
                            radius: 4
                            color: root.viewerBackground
                            visible: modelData.thumbnail !== ""
                            Image {
                                anchors.fill: parent
                                fillMode: Image.PreserveAspectCrop
                                source: modelData.thumbnail
                                asynchronous: true
                                sourceSize.width: 156
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            UiText {
                                Layout.fillWidth: true
                                text: modelData.title
                                color: root.fg
                                font.pixelSize: 12
                                elide: Text.ElideMiddle
                                ToolTip.visible: titleHover.hovered
                                ToolTip.text: modelData.name
                                HoverHandler { id: titleHover }
                            }
                            UiText {
                                text: modelData.growing ? "Still recording…"
                                                        : modelData.when + (modelData.noAudio ? " · no audio" : "")
                                color: root.muted
                                font.pixelSize: 11
                            }
                        }
                        OmaButton {
                            objectName: "editRecording"
                            text: "Edit"
                            enabled: !modelData.growing
                            onClicked: {
                                const path = modelData.path
                                root.confirmDiscard("Edit this recording", () => {
                                    session.newProject()
                                    session.open(path)
                                })
                            }
                        }
                    }
                }
            }
        }
    }
}

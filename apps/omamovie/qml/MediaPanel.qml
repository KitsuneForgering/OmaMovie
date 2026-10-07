import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// The library (ui-design §4): docked at ~30% in wide windows, an overlay panel otherwise.
// Reads the window's ids from the document that places it.
Rectangle {
    id: library
    objectName: "librarySidebar"
    z: 3
    visible: !root.viewerOnly && (root.compact ? root.libraryOverlay : !root.libraryHidden)
    width: root.compact ? Math.min(380, upper.width * 0.8) : upper.width * 0.30
    // As an overlay it stops above the transport so the timecode stays visible.
    height: root.compact ? upper.height - transport.height : upper.height
    color: colors.dark_background

    RowLayout {
        anchors.fill: parent
        spacing: 0
        ColumnLayout {
            Layout.preferredWidth: 104
            Layout.fillHeight: true
            Layout.margins: 8
            spacing: 4
            UiText { text: "SOURCES"; color: root.muted; font.pixelSize: 10; font.bold: true; Layout.bottomMargin: 4 }
            OmaButton {
                text: "Project"
                selected: root.librarySource === "project"
                Layout.fillWidth: true
                onClicked: root.librarySource = "project"
            }
            OmaButton {
                objectName: "recordingsSource"
                text: "Recordings"
                selected: root.librarySource === "recordings"
                Layout.fillWidth: true
                onClicked: {
                    session.refreshRecordings()
                    root.librarySource = "recordings"
                }
            }
            Item { Layout.fillHeight: true }
        }
        Rectangle { Layout.fillHeight: true; implicitWidth: 1; color: root.line }
        // Omarchy screen recordings (UX-09): import one into the project's media.
        ColumnLayout {
            visible: root.librarySource === "recordings"
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                Layout.leftMargin: 12
                Layout.rightMargin: 8
                UiText { text: "Recordings"; color: root.fg; font.pixelSize: 12; font.bold: true }
                UiText { text: session.recordings.length; color: root.muted; font.pixelSize: 11 }
                Item { Layout.fillWidth: true }
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 6
                clip: true
                spacing: 6
                model: session.recordings
                delegate: RowLayout {
                    required property var modelData
                    objectName: "recordingItem"
                    width: ListView.view.width
                    spacing: 8
                    Rectangle {
                        Layout.preferredWidth: 72
                        Layout.preferredHeight: 40
                        radius: 4
                        color: root.viewerBackground
                        Image {
                            anchors.fill: parent
                            fillMode: Image.PreserveAspectCrop
                            source: modelData.thumbnail
                            asynchronous: true
                            sourceSize.width: 144
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1
                        UiText {
                            Layout.fillWidth: true
                            text: modelData.title
                            color: root.fg
                            font.pixelSize: 12
                            elide: Text.ElideMiddle
                        }
                        UiText {
                            Layout.fillWidth: true
                            text: modelData.growing ? "Still recording…"
                                                    : modelData.when + (modelData.noAudio ? " · no audio" : "")
                            color: root.muted
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }
                    OmaButton {
                        objectName: "importRecording"
                        text: "Import"
                        enabled: !modelData.growing
                        onClicked: {
                            session.importUrl(modelData.url)
                            root.librarySource = "project"
                        }
                    }
                }
                UiText {
                    anchors.centerIn: parent
                    width: parent.width - 24
                    visible: session.recordings.length === 0
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: "No screen recordings in " + session.recordingsFolder
                    color: root.muted
                    font.pixelSize: 11
                }
            }
        }
        ColumnLayout {
            visible: root.librarySource === "project"
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                Layout.leftMargin: 12
                Layout.rightMargin: 8
                UiText { text: "Media"; color: root.fg; font.pixelSize: 12; font.bold: true }
                UiText { text: session.media.length; color: root.muted; font.pixelSize: 11 }
                Item { Layout.fillWidth: true }
                // The selected item onto the timeline; the chevron offers insert and overwrite.
                OmaButton {
                    visible: actions.mediaChosen
                    action: actions.append
                    text: "Add"
                    iconName: "append"
                }
                OmaButton {
                    id: placeMore
                    visible: actions.mediaChosen
                    iconName: "more"
                    showLabel: false
                    tip: "More ways to add"
                    onClicked: libraryMenu.popup(placeMore, 0, placeMore.height)
                }
                OmaButton { action: actions.importMedia; iconName: "import"; showLabel: false }
            }
            GridView {
                id: mediaGrid
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 6
                cellWidth: Math.max(110, Math.floor(width / Math.max(1, Math.floor(width / 150))))
                cellHeight: 132
                clip: true
                model: session.media
                delegate: Item {
                    objectName: "libraryItem"
                    width: mediaGrid.cellWidth
                    height: mediaGrid.cellHeight
                    readonly property bool chosen: index === session.selectedMedia
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 5
                        spacing: 4
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 72
                            radius: 4
                            color: root.viewerBackground
                            border.width: chosen ? 2 : 0
                            border.color: root.accent
                            Image {
                                anchors.fill: parent
                                anchors.margins: chosen ? 2 : 0
                                fillMode: Image.PreserveAspectFit
                                source: modelData.thumbnail
                                asynchronous: true
                            }
                            // A moved or deleted file of an opened project: its clips are gaps
                            // until it is located again (M7 relink).
                            OmaButton {
                                objectName: "locateMedia"
                                anchors.centerIn: parent
                                visible: !!modelData.missing
                                text: "Missing · Locate…"
                                tip: "Find the file on disk; others missing from the same folder are found too"
                                onClicked: { session.selectMedia(index); locateDialog.open() }
                            }
                            Icon { // sound has no picture
                                anchors.centerIn: parent
                                visible: modelData.audioOnly && !modelData.missing
                                name: "music"
                                size: 28
                                color: colors.green || root.fg
                            }
                        }
                        UiText { Layout.fillWidth: true; text: modelData.name; color: root.fg; font.pixelSize: 12; elide: Text.ElideMiddle }
                        // A marked range (source viewer) is what Add/Insert/Overwrite will use: say so.
                        UiText {
                            objectName: "mediaRange"
                            readonly property bool ranged: modelData.markIn >= 0 || modelData.markOut >= 0
                            text: ranged ? "Range " + root.timecode(Math.max(0, modelData.markIn)) + " – "
                                           + (modelData.markOut >= 0 ? root.timecode(modelData.markOut - 1 / session.frameRate) : "end")
                                         : root.timecode(modelData.duration)
                            color: ranged ? (colors.yellow || root.accent) : root.muted
                            font.pixelSize: 11
                        }
                    }
                    // Click selects, double click appends, dragging carries the item
                    // onto the timeline (ui-design §7.3).
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        preventStealing: true
                        drag.target: mediaDrag
                        drag.threshold: 6
                        onPressed: (mouse) => {
                            if (mouse.button === Qt.RightButton) {
                                session.selectMedia(index)
                                libraryMenu.popup()
                                return
                            }
                            const p = mapToItem(mediaDrag.parent, mouse.x, mouse.y)
                            mediaDrag.x = p.x
                            mediaDrag.y = p.y - mediaDrag.height / 2
                        }
                        onClicked: (mouse) => { if (mouse.button === Qt.LeftButton) session.selectMedia(index) }
                        onDoubleClicked: actions.append.trigger()
                        drag.onActiveChanged: {
                            if (drag.active) {
                                mediaDrag.index = index
                                mediaDrag.Drag.active = true
                            } else {
                                mediaDrag.Drag.drop()
                                mediaDrag.index = -1
                            }
                        }
                    }
                }
                FileDialog {
                    id: locateDialog
                    title: "Locate " + ((session.media[session.selectedMedia] || {}).name || "the missing file")
                    fileMode: FileDialog.OpenFile
                    onAccepted: session.relinkMedia(session.selectedMedia, selectedFile)
                }
                // What a library item can do on the timeline (ui-design §4).
                Menu {
                    id: libraryMenu
                    objectName: "libraryMenu"
                    MenuItem {
                        readonly property bool missing: session.selectedMedia >= 0 && !!(session.media[session.selectedMedia] || {}).missing
                        text: "Locate missing file…"
                        visible: missing
                        height: missing ? implicitHeight : 0
                        onTriggered: locateDialog.open()
                    }
                    MenuItem { action: actions.openSource }
                    MenuSeparator {}
                    MenuItem { action: actions.append }
                    MenuItem { action: actions.insert }
                    MenuItem { action: actions.overwrite }
                    MenuItem { action: actions.connect }
                    MenuSeparator {}
                    MenuItem { action: actions.addTitle }
                }
                UiText {
                    anchors.centerIn: parent
                    width: parent.width - 24
                    visible: session.media.length === 0
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: "Import videos" + root.shortcutText(actions.importMedia)
                    color: root.muted
                    font.pixelSize: 11
                }
            }
        }
    }
    Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: root.line }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Shapes
import OmaMovie 1.0

// Editor shell following Docs/ui-design.md: three fixed areas (library, viewer, timeline),
// adjustments in a bar and drawer above the viewer, a transport, and every command
// registered once as an Action with its §8.1 shortcut.
ApplicationWindow {
    id: root
    visible: false
    width: 1280
    height: 800
    minimumWidth: 640
    minimumHeight: 520
    title: session.editing ? "Untitled project — OmaMovie" : "OmaMovie — Projects"
    color: colors.background

    property color fg: colors.foreground
    property color muted: colors.dark_foreground
    property color accent: colors.accent
    property color line: colors.lighter_background
    readonly property color viewerBackground: "#1b1b1d" // never themed (ui-design §5)
    // Layout breakpoints (ui-design §2): wide ≥ 1500, half width 900–1500, narrow below.
    property bool compact: width < 1500
    property bool narrow: width < 900
    property bool libraryOverlay: false
    property bool viewerOnly: false
    property string drawer: "" // the open adjustment, "" when the drawer is closed
    property real upperRatio: 0.55
    property real pixelsPerSecond: 60
    property bool fitTimeline: true

    function pad(n, width) { return String(n).padStart(width, "0") }
    // HH:MM:SS:FF at the nominal frame rate (display only).
    function timecode(seconds) {
        const fps = Math.max(1, Math.round(session.frameRate))
        const total = Math.floor(Math.max(0, seconds) * fps + 1e-6)
        const s = Math.floor(total / fps)
        return pad(Math.floor(s / 3600), 2) + ":" + pad(Math.floor(s / 60) % 60, 2) + ":" +
            pad(s % 60, 2) + ":" + pad(total % fps, 2)
    }
    readonly property bool audioAdjusted: !!session.info.clip &&
        (session.info.gain !== 1 || session.info.fadeIn > 0 || session.info.fadeOut > 0 || session.info.muted)
    function decibels(gain) {
        return gain <= 0.0001 ? "−∞ dB" : (20 * Math.log(gain) / Math.LN10).toFixed(1) + " dB"
    }
    function shortcutText(action) {
        return action && action.keys ? " (" + action.keys + ")" : ""
    }

    // ---------------------------------------------------------------- actions (ui-design §8.1)
    // The single registry of editor commands: name, default shortcut and handler. Buttons and
    // shortcuts both go through it, so remapping later touches one place.
    QtObject {
        id: actions
        readonly property bool editing: session.editing
        readonly property bool media: session.hasMedia

        property OmaAction playPause: OmaAction {
            text: session.playing ? "Pause" : "Play"
            keys: "Space"; enabled: actions.media
            onTriggered: session.togglePlay()
        }
        property OmaAction pause: OmaAction {
            text: "Pause"; keys: "K"; enabled: actions.media && session.playing
            onTriggered: session.pause()
        }
        property OmaAction stop: OmaAction {
            text: "Stop and return to start"; enabled: actions.media
            onTriggered: { session.pause(); session.seek(0) }
        }
        property OmaAction playForward: OmaAction {
            text: "Play forward (repeat: faster)"; keys: "L"; enabled: actions.media
            onTriggered: session.shuttle(1)
        }
        property OmaAction playBackward: OmaAction {
            text: "Play backward (repeat: faster)"; keys: "J"; enabled: actions.media
            onTriggered: session.shuttle(-1)
        }
        property OmaAction previousFrame: OmaAction {
            text: "Previous frame"; keys: "Left"; enabled: actions.media
            onTriggered: session.stepFrames(-1)
        }
        property OmaAction nextFrame: OmaAction {
            text: "Next frame"; keys: "Right"; enabled: actions.media
            onTriggered: session.stepFrames(1)
        }
        property OmaAction back10: OmaAction {
            text: "Back 10 frames"; keys: "Shift+Left"; enabled: actions.media
            onTriggered: session.stepFrames(-10)
        }
        property OmaAction forward10: OmaAction {
            text: "Forward 10 frames"; keys: "Shift+Right"; enabled: actions.media
            onTriggered: session.stepFrames(10)
        }
        property OmaAction toStart: OmaAction {
            text: "Go to start"; keys: "Home"; enabled: actions.media
            onTriggered: session.seek(0)
        }
        property OmaAction toEnd: OmaAction {
            text: "Go to end"; keys: "End"; enabled: actions.media
            onTriggered: session.toEnd()
        }
        readonly property bool mediaChosen: actions.editing && session.selectedMedia >= 0
        property OmaAction append: OmaAction {
            text: "Append to the end"; keys: "E"; enabled: actions.mediaChosen
            onTriggered: { session.appendSelected(); root.libraryOverlay = false }
        }
        property OmaAction insert: OmaAction {
            text: "Insert at the playhead"; keys: "W"; enabled: actions.mediaChosen
            onTriggered: { session.insertSelected(); root.libraryOverlay = false }
        }
        property OmaAction overwrite: OmaAction {
            text: "Overwrite at the playhead"; keys: "D"; enabled: actions.mediaChosen
            onTriggered: { session.overwriteSelected(); root.libraryOverlay = false }
        }
        property OmaAction split: OmaAction {
            text: "Split at the playhead"; keys: "Ctrl+B"; enabled: actions.media
            onTriggered: session.splitAtPlayhead()
        }
        property OmaAction remove: OmaAction {
            text: "Delete and close the gap"; keys: "Delete"; enabled: actions.media
            onTriggered: session.deleteSelected(true)
        }
        property OmaAction lift: OmaAction {
            text: "Replace with a gap"; keys: "Shift+Delete"; enabled: actions.media
            onTriggered: session.deleteSelected(false)
        }
        property OmaAction undo: OmaAction {
            text: session.canUndo ? "Undo " + session.undoText : "Undo"
            keys: "Ctrl+Z"; enabled: actions.editing && session.canUndo
            onTriggered: session.undo()
        }
        property OmaAction redo: OmaAction {
            text: session.canRedo ? "Redo " + session.redoText : "Redo"
            keys: "Ctrl+Shift+Z"; enabled: actions.editing && session.canRedo
            onTriggered: session.redo()
        }
        property OmaAction importMedia: OmaAction {
            text: "Import"; keys: "Ctrl+I"
            onTriggered: fileDialog.open()
        }
        property OmaAction exportMovie: OmaAction { text: "Export (M7)"; keys: "Ctrl+E"; enabled: false }
        property OmaAction toggleLibrary: OmaAction {
            text: "Library"; keys: "Ctrl+1"; enabled: actions.editing && root.compact
            onTriggered: root.libraryOverlay = !root.libraryOverlay
        }
        property OmaAction fullViewer: OmaAction {
            text: "Full-screen viewer"; keys: "Ctrl+Shift+F"; enabled: actions.editing
            onTriggered: root.viewerOnly = !root.viewerOnly
        }
        property OmaAction leaveFullViewer: OmaAction {
            text: "Leave full screen"; keys: "Escape"; enabled: root.viewerOnly
            onTriggered: root.viewerOnly = false
        }
        property OmaAction zoomIn: OmaAction {
            text: "Zoom in"; keys: "Ctrl+="; enabled: actions.editing
            onTriggered: { root.fitTimeline = false; root.pixelsPerSecond = Math.min(2400, timelinePanel.scale * 1.5) }
        }
        property OmaAction zoomOut: OmaAction {
            text: "Zoom out"; keys: "Ctrl+-"; enabled: actions.editing
            onTriggered: { root.fitTimeline = false; root.pixelsPerSecond = Math.max(4, timelinePanel.scale / 1.5) }
        }
        property OmaAction zoomFit: OmaAction {
            text: "Fit the project"; keys: "Shift+Z"; enabled: actions.editing
            onTriggered: root.fitTimeline = true
        }
        property OmaAction volume: OmaAction {
            text: "Volume"; keys: "Ctrl+Shift+V"
            enabled: actions.editing && !!session.info.clip && !!session.info.hasAudio
            onTriggered: root.drawer = root.drawer === "volume" ? "" : "volume"
        }
        property OmaAction info: OmaAction {
            text: "Info"; keys: "Ctrl+Shift+I"; enabled: actions.editing && !!session.info.name; checkable: true
            checked: root.drawer === "info"
            onTriggered: root.drawer = root.drawer === "info" ? "" : "info"
        }
    }

    FileDialog {
        id: fileDialog
        title: "Import"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Videos and pictures (*.mp4 *.mkv *.mov *.webm *.avi *.m4v *.y4m *.png *.jpg *.jpeg)", "All files (*)"]
        onAccepted: {
            for (const file of selectedFiles) session.importUrl(file)
            root.libraryOverlay = false
        }
    }

    // ---------------------------------------------------------------- Projects screen (§3)
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
            Text { text: "OmaMovie"; color: root.fg; font.pixelSize: 18; font.bold: true }
            Item { Layout.fillWidth: true }
            OmaButton { text: "New project"; primary: true; onClicked: session.newProject() }
        }
        Rectangle { anchors.top: projectsBar.bottom; width: parent.width; height: 1; color: root.line }

        ColumnLayout {
            anchors.top: projectsBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 32
            spacing: 14
            Text {
                Layout.topMargin: 16
                text: "RECENT"
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
                    Text { text: "Untitled project"; color: root.fg; font.pixelSize: 13; font.bold: true }
                    Text {
                        text: session.media.length + " video" + (session.media.length === 1 ? "" : "s") + " · this session"
                        color: root.muted
                        font.pixelSize: 11
                    }
                }
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: session.continueProject() }
            }
            Text {
                visible: session.media.length === 0
                text: "No projects yet. Start one with New project."
                color: root.muted
                font.pixelSize: 12
            }
            Text {
                text: "Projects are kept for this session only; saving arrives with M7."
                color: root.muted
                font.pixelSize: 11
            }
        }
    }

    // ---------------------------------------------------------------- Edit screen
    Item {
        id: editor
        anchors.fill: parent
        visible: session.editing

        // Top bar (§2.1): back to Projects, project name, undo/redo, import, export.
        Rectangle {
            id: topbar
            visible: !root.viewerOnly
            anchors.left: parent.left
            anchors.right: parent.right
            height: visible ? 48 : 0
            color: colors.background
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 10
                spacing: 4
                OmaButton {
                    iconName: "back"
                    text: "Projects"
                    showLabel: !root.compact
                    tip: "Projects"
                    onClicked: session.showProjects()
                }
                Text {
                    Layout.leftMargin: 6
                    Layout.fillWidth: true
                    text: "Untitled project"
                    color: root.fg
                    font.pixelSize: 13
                    font.bold: true
                    elide: Text.ElideRight
                }
                OmaButton { action: actions.undo; iconName: "undo"; showLabel: false }
                OmaButton { action: actions.redo; iconName: "redo"; showLabel: false }
                Separator { Layout.leftMargin: 6; Layout.rightMargin: 6 }
                OmaButton { action: actions.importMedia; iconName: "import"; showLabel: !root.compact }
                OmaButton { action: actions.exportMovie; iconName: "export"; text: "Export"; showLabel: !root.compact }
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: root.line }
        }

        Item {
            id: workspace
            anchors.top: topbar.bottom
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right

            Item {
                id: upper
                width: parent.width
                height: root.viewerOnly ? parent.height : parent.height * root.upperRatio

                // Library (§4): docked at ~30% in wide windows, an overlay panel otherwise.
                Rectangle {
                    id: library
                    z: 3
                    visible: !root.viewerOnly && (!root.compact || root.libraryOverlay)
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
                            Text { text: "SOURCES"; color: root.muted; font.pixelSize: 10; font.bold: true; Layout.bottomMargin: 4 }
                            OmaButton { text: "Project"; selected: true; Layout.fillWidth: true }
                            Item { Layout.fillHeight: true }
                        }
                        Rectangle { Layout.fillHeight: true; implicitWidth: 1; color: root.line }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 0
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 40
                                Layout.leftMargin: 12
                                Layout.rightMargin: 8
                                Text { text: "Media"; color: root.fg; font.pixelSize: 12; font.bold: true }
                                Text { text: session.media.length; color: root.muted; font.pixelSize: 11 }
                                Item { Layout.fillWidth: true }
                                OmaButton { action: actions.importMedia; iconName: "import"; showLabel: false }
                            }
                            GridView {
                                id: mediaGrid
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.margins: 6
                                cellWidth: Math.max(110, Math.floor(width / Math.max(1, Math.floor(width / 150))))
                                cellHeight: 120
                                clip: true
                                model: session.media
                                delegate: Item {
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
                                        }
                                        Text { Layout.fillWidth: true; text: modelData.name; color: root.fg; font.pixelSize: 10; elide: Text.ElideMiddle }
                                        Text { text: root.timecode(modelData.duration); color: root.muted; font.pixelSize: 10 }
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: session.selectMedia(index)
                                        onDoubleClicked: actions.append.trigger()
                                    }
                                }
                                Text {
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

                ColumnLayout {
                    id: viewerColumn
                    x: root.compact || root.viewerOnly ? 0 : library.width
                    width: upper.width - x
                    height: upper.height
                    spacing: 0

                    // Adjustments bar (§6): only what applies to the selection is enabled.
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        visible: !root.viewerOnly
                        color: colors.background
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 2
                            OmaButton { iconName: "color"; text: "Color"; showLabel: !root.compact; enabled: false; tip: "Color (v0.1)" }
                            OmaButton { iconName: "crop"; text: "Crop"; showLabel: !root.compact; enabled: false; tip: "Crop and framing (v0.1)" }
                            OmaButton {
                                action: actions.volume
                                iconName: "volume"
                                showLabel: !root.compact
                                selected: root.drawer === "volume"
                                activeDot: root.audioAdjusted
                            }
                            OmaButton { iconName: "speed"; text: "Speed"; showLabel: !root.compact; enabled: false; tip: "Speed (v0.2)" }
                            OmaButton { iconName: "effects"; text: "Effects"; showLabel: !root.compact; enabled: false; tip: "Effects (v0.2)" }
                            OmaButton { iconName: "overlay"; text: "Overlay"; showLabel: !root.compact; enabled: false; tip: "Overlay, for layers above the storyline (v0.2)" }
                            OmaButton {
                                action: actions.info
                                iconName: "info"
                                showLabel: !root.compact
                                selected: root.drawer === "info"
                            }
                            Item { Layout.fillWidth: true }
                            OmaButton {
                                visible: root.compact
                                action: actions.toggleLibrary
                                iconName: "library"
                                showLabel: !root.narrow
                                selected: root.libraryOverlay
                            }
                        }
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: root.line }
                    }

                    // Drawer (§6): opens between the bar and the viewer, pushing the viewer down.
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 64
                        visible: !root.viewerOnly && root.drawer === "info" && !!session.info.name
                        color: colors.dark_background
                        GridLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            columns: root.compact ? 2 : 4
                            columnSpacing: 22
                            rowSpacing: 4
                            Field { label: "NAME"; value: session.info.name || "" }
                            Field { label: "DURATION"; value: root.timecode(session.info.duration || 0) }
                            Field { label: "FORMAT"; value: (session.info.codec || "") + "  " + (session.info.resolution || "") + "  " + (session.info.frameRate || "") }
                            Field { label: "DECODE PATH"; value: session.info.decodePath || "" }
                        }
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: root.line }
                    }

                    // Volume drawer (§6, level 1): volume, fades and mute for the selected clip.
                    // Values follow the clip; a change is committed as one command on release.
                    Rectangle {
                        id: volumeDrawer
                        Layout.fillWidth: true
                        Layout.preferredHeight: 64
                        visible: !root.viewerOnly && root.drawer === "volume" && actions.volume.enabled
                        color: colors.dark_background
                        function commit() {
                            session.setClipAudio(gainControl.slider.value, fadeInControl.slider.value,
                                                 fadeOutControl.slider.value, !!session.info.muted)
                        }
                        component DrawerSlider: ColumnLayout {
                            property alias slider: control
                            property string label
                            property string readout
                            spacing: 0
                            Layout.fillWidth: true
                            RowLayout {
                                Text { text: parent.parent.label; color: root.muted; font.pixelSize: 9; font.bold: true }
                                Item { Layout.fillWidth: true }
                                Text { text: parent.parent.readout; color: root.fg; font.pixelSize: 10 }
                            }
                            Slider {
                                id: control
                                Layout.fillWidth: true
                                focusPolicy: Qt.NoFocus
                                onPressedChanged: if (!pressed) volumeDrawer.commit()
                            }
                        }
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 18
                            DrawerSlider {
                                id: gainControl
                                label: "VOLUME"
                                readout: root.decibels(gainControl.slider.value)
                            }
                            DrawerSlider {
                                id: fadeInControl
                                label: "FADE IN"
                                readout: fadeInControl.slider.value.toFixed(2) + " s"
                            }
                            DrawerSlider {
                                id: fadeOutControl
                                label: "FADE OUT"
                                readout: fadeOutControl.slider.value.toFixed(2) + " s"
                            }
                            OmaButton {
                                text: session.info.muted ? "Unmute" : "Mute"
                                selected: !!session.info.muted
                                onClicked: session.setClipAudio(gainControl.slider.value, fadeInControl.slider.value,
                                                                fadeOutControl.slider.value, !session.info.muted)
                            }
                        }
                        // The sliders follow the selected clip, except while one is being dragged.
                        Binding { target: gainControl.slider; property: "from"; value: 0 }
                        Binding { target: gainControl.slider; property: "to"; value: 2 }
                        Binding { target: gainControl.slider; property: "value"; value: session.info.gain || 0; when: !gainControl.slider.pressed }
                        Binding { target: fadeInControl.slider; property: "to"; value: Math.max(0.01, session.info.clipDuration || 0) }
                        Binding { target: fadeInControl.slider; property: "value"; value: session.info.fadeIn || 0; when: !fadeInControl.slider.pressed }
                        Binding { target: fadeOutControl.slider; property: "to"; value: Math.max(0.01, session.info.clipDuration || 0) }
                        Binding { target: fadeOutControl.slider; property: "value"; value: session.info.fadeOut || 0; when: !fadeOutControl.slider.pressed }
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: root.line }
                    }

                    // Viewer (§5): neutral background, untouched by the theme.
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: root.viewerBackground
                        PreviewItem { objectName: "preview"; anchors.fill: parent }
                        Text {
                            anchors.centerIn: parent
                            visible: !session.hasMedia && !session.failed
                            text: session.media.length ? "Opening…" : "Import a video to start" + root.shortcutText(actions.importMedia)
                            color: "#8f9095"
                            font.pixelSize: 12
                        }
                        // Playback status: nothing when all is well, the reason when it fails.
                        Rectangle {
                            visible: session.failed
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 12
                            width: Math.min(parent.width - 24, failure.implicitWidth + 24)
                            height: 30
                            radius: 6
                            color: "#2a1d1f"
                            Text {
                                id: failure
                                anchors.centerIn: parent
                                width: parent.width - 24
                                text: session.status
                                color: colors.red || "#ff8a8a"
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                        }
                    }

                    // Transport (§5): the same actions serve buttons and keyboard shortcuts.
                    Rectangle {
                        id: transport
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        color: colors.background
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 8
                            spacing: 2
                            Text {
                                Layout.preferredWidth: root.narrow ? 104 : 214
                                text: root.timecode(session.position) + (root.narrow ? "" : " / " + root.timecode(session.duration))
                                color: session.hasMedia ? root.fg : root.muted
                                font.pixelSize: 12
                            }
                            OmaButton { objectName: "transportStart"; action: actions.toStart; iconName: "start"; showLabel: false }
                            OmaButton { action: actions.previousFrame; iconName: "previous"; showLabel: false }
                            OmaButton { action: actions.playBackward; iconName: "backward"; showLabel: false }
                            OmaButton { objectName: "transportStop"; action: actions.stop; iconName: "stop"; showLabel: false }
                            OmaButton {
                                action: actions.playPause
                                iconName: session.playing ? "pause" : "play"
                                showLabel: false
                                implicitWidth: 44
                            }
                            OmaButton { action: actions.playForward; iconName: "forward"; showLabel: false }
                            OmaButton { action: actions.nextFrame; iconName: "next"; showLabel: false }
                            OmaButton { objectName: "transportEnd"; action: actions.toEnd; iconName: "end"; showLabel: false }
                            Text { // shuttle speed (J/K/L), shown only when not normal
                                Layout.preferredWidth: 34
                                visible: session.speed !== 0 && session.speed !== 1
                                text: (session.speed < 0 ? "−" : "") + Math.abs(session.speed) + "×"
                                color: root.accent
                                font.pixelSize: 12
                                font.bold: true
                            }
                            Item {
                                Layout.preferredWidth: 1
                                Layout.fillWidth: true
                                implicitHeight: 32
                                OmaButton {
                                    anchors.right: parent.right
                                    action: root.viewerOnly ? actions.leaveFullViewer : actions.fullViewer
                                    iconName: "fullscreen"
                                    showLabel: false
                                    selected: root.viewerOnly
                                }
                            }
                        }
                        Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: root.line }
                    }
                }
            }

            // The single divider between the upper area and the timeline (§2.1).
            Rectangle {
                id: splitter
                visible: !root.viewerOnly
                anchors.top: upper.bottom
                width: parent.width
                height: 6
                color: colors.background
                Rectangle { width: 36; height: 2; radius: 1; color: root.muted; anchors.centerIn: parent; opacity: 0.6 }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.SplitVCursor
                    onPositionChanged: (mouse) => {
                        if (pressed)
                            root.upperRatio = Math.max(0.35, Math.min(0.75, mapToItem(workspace, mouse.x, mouse.y).y / workspace.height))
                    }
                }
            }

            // Timeline (§7): edit actions, minimap, storyline, and zoom.
            Rectangle {
                id: timelinePanel
                visible: !root.viewerOnly
                anchors.top: splitter.bottom
                anchors.bottom: parent.bottom
                width: parent.width
                color: colors.darker_background

                readonly property real total: session.duration
                readonly property real scale: root.fitTimeline && total > 0
                    ? Math.min(2400, Math.max(4, (timelineScroll.width - 48) / total)) : root.pixelsPerSecond
                readonly property real origin: 24 // left margin of time zero, in content pixels

                Rectangle {
                    id: editBar
                    anchors.top: parent.top
                    width: parent.width
                    height: 42
                    color: colors.dark_background
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 3
                        OmaButton { action: actions.append; iconName: "append"; showLabel: !root.compact }
                        OmaButton { action: actions.insert; iconName: "insert"; showLabel: !root.compact }
                        OmaButton { action: actions.overwrite; iconName: "overwrite"; showLabel: !root.compact }
                        Separator { Layout.leftMargin: 5; Layout.rightMargin: 5 }
                        OmaButton { objectName: "editSplit"; action: actions.split; iconName: "split"; showLabel: !root.compact }
                        OmaButton { action: actions.remove; iconName: "rippleDelete"; showLabel: !root.compact }
                        OmaButton { action: actions.lift; iconName: "lift"; showLabel: !root.compact }
                        Item { Layout.fillWidth: true }
                    }
                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: root.line }
                }

                // Minimap: the whole project, with the visible region highlighted.
                Item {
                    id: minimap
                    anchors.top: editBar.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 12
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    height: 14
                    visible: session.clips.length > 0
                    Repeater {
                        model: session.clips
                        delegate: Rectangle {
                            x: minimap.width * modelData.start / Math.max(0.001, timelinePanel.total)
                            width: Math.max(2, minimap.width * modelData.duration / Math.max(0.001, timelinePanel.total) - 1)
                            anchors.verticalCenter: parent.verticalCenter
                            height: 6
                            radius: 2
                            color: modelData.id === session.selectedClip ? root.accent : colors.blue
                            opacity: 0.85
                        }
                    }
                    Rectangle { // playhead
                        x: minimap.width * session.position / Math.max(0.001, timelinePanel.total)
                        width: 2
                        height: parent.height
                        color: root.accent
                    }
                    Rectangle { // visible region
                        readonly property real content: Math.max(1, timelineScroll.contentWidth)
                        x: minimap.width * timelineScroll.contentX / content
                        width: Math.max(8, minimap.width * Math.min(1, timelineScroll.width / content))
                        height: parent.height
                        radius: 3
                        color: "transparent"
                        border.color: root.fg
                        opacity: 0.35
                    }
                    MouseArea {
                        anchors.fill: parent
                        onPressed: (mouse) => navigate(mouse.x)
                        onPositionChanged: (mouse) => { if (pressed) navigate(mouse.x) }
                        function navigate(x) {
                            const target = x / minimap.width * timelineScroll.contentWidth - timelineScroll.width / 2
                            timelineScroll.contentX = Math.max(0, Math.min(timelineScroll.contentWidth - timelineScroll.width, target))
                        }
                    }
                }

                Flickable {
                    id: timelineScroll
                    anchors.top: minimap.bottom
                    anchors.topMargin: 10
                    anchors.bottom: zoomBar.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    clip: true
                    interactive: false // dragging belongs to scrubbing and trimming; scroll with the wheel or minimap
                    boundsBehavior: Flickable.StopAtBounds
                    contentWidth: Math.max(width, timelinePanel.origin * 2 + timelinePanel.total * timelinePanel.scale)
                    contentHeight: height
                    function secondsAt(x) { return Math.max(0, (x - timelinePanel.origin) / timelinePanel.scale) }
                    WheelHandler {
                        acceptedModifiers: Qt.ControlModifier
                        onWheel: (event) => event.angleDelta.y > 0 ? actions.zoomIn.trigger() : actions.zoomOut.trigger()
                    }
                    WheelHandler {
                        acceptedModifiers: Qt.NoModifier
                        onWheel: (event) => {
                            const delta = event.angleDelta.x !== 0 ? event.angleDelta.x : event.angleDelta.y
                            timelineScroll.contentX = Math.max(0, Math.min(timelineScroll.contentWidth - timelineScroll.width,
                                                                           timelineScroll.contentX - delta))
                        }
                    }
                    // Clicking or dragging on empty timeline moves the playhead (scrubbing).
                    MouseArea {
                        width: timelineScroll.contentWidth
                        height: timelineScroll.height
                        onPressed: (mouse) => session.seek(timelineScroll.secondsAt(mouse.x))
                        onPositionChanged: (mouse) => { if (pressed) session.seek(timelineScroll.secondsAt(mouse.x)) }
                    }
                    Repeater {
                        model: session.clips
                        delegate: Rectangle {
                            id: clipItem
                            readonly property bool chosen: modelData.id === session.selectedClip
                            // Live feedback while an edge is dragged; committed as one trim on release.
                            property real headDrag: 0
                            property real tailDrag: 0
                            x: timelinePanel.origin + modelData.start * timelinePanel.scale
                            y: 4
                            width: Math.max(6, modelData.duration * timelinePanel.scale - 2 - headDrag + tailDrag)
                            height: Math.min(86, Math.max(44, timelineScroll.height - 12))
                            radius: 5
                            clip: true
                            color: Qt.tint(colors.lighter_background, Qt.rgba(0.31, 0.55, 1, 0.18))
                            border.width: chosen ? 2 : 0
                            border.color: root.accent
                            Image {
                                anchors.fill: parent
                                anchors.margins: 2
                                anchors.bottomMargin: 20
                                source: modelData.thumbnail
                                fillMode: Image.TileHorizontally
                                verticalAlignment: Image.AlignVCenter
                                asynchronous: true
                                sourceSize.height: height
                            }
                            // Fades: a ramp over the clip's ends, as long as the fade.
                            Shape {
                                id: fades
                                anchors.fill: parent
                                visible: modelData.fadeIn > 0 || modelData.fadeOut > 0
                                preferredRendererType: Shape.CurveRenderer
                                readonly property real fadeInWidth: modelData.fadeIn * timelinePanel.scale
                                readonly property real fadeOutWidth: modelData.fadeOut * timelinePanel.scale
                                ShapePath {
                                    strokeColor: "transparent"
                                    fillColor: Qt.rgba(0, 0, 0, 0.45)
                                    startX: 0; startY: 0
                                    PathLine { x: fades.fadeInWidth; y: 0 }
                                    PathLine { x: 0; y: clipItem.height }
                                    PathLine { x: 0; y: 0 }
                                }
                                ShapePath {
                                    strokeColor: "transparent"
                                    fillColor: Qt.rgba(0, 0, 0, 0.45)
                                    startX: clipItem.width; startY: 0
                                    PathLine { x: clipItem.width - fades.fadeOutWidth; y: 0 }
                                    PathLine { x: clipItem.width; y: clipItem.height }
                                    PathLine { x: clipItem.width; y: 0 }
                                }
                            }
                            Text {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 6
                                anchors.bottomMargin: 4
                                text: modelData.name
                                color: root.fg
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }
                            MouseArea {
                                anchors.fill: parent
                                // Selects the clip and moves the playhead to the click.
                                onPressed: (mouse) => {
                                    session.selectClip(modelData.id)
                                    session.seek(modelData.start + mouse.x / timelinePanel.scale)
                                }
                            }
                            // Edges: drag to trim. The storyline is magnetic, so later clips follow
                            // (ripple trim, ui-design §7.3).
                            component Edge: MouseArea {
                                property bool head
                                property real pressX: 0
                                width: 8
                                height: parent.height
                                cursorShape: Qt.SizeHorCursor
                                hoverEnabled: true
                                preventStealing: true
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: 1
                                    radius: 3
                                    color: root.accent
                                    opacity: parent.containsMouse || parent.pressed ? 0.9 : 0
                                }
                                onPressed: (mouse) => { pressX = mapToItem(timelineScroll.contentItem, mouse.x, 0).x }
                                onPositionChanged: (mouse) => {
                                    if (!pressed) return
                                    const dx = mapToItem(timelineScroll.contentItem, mouse.x, 0).x - pressX
                                    if (head) clipItem.headDrag = dx
                                    else clipItem.tailDrag = dx
                                }
                                onReleased: {
                                    const dx = head ? clipItem.headDrag : clipItem.tailDrag
                                    clipItem.headDrag = 0
                                    clipItem.tailDrag = 0
                                    const frames = Math.round(dx / timelinePanel.scale * session.frameRate)
                                    if (frames !== 0) session.trimClip(modelData.id, head, frames)
                                }
                            }
                            Edge { head: true; anchors.left: parent.left }
                            Edge { head: false; anchors.right: parent.right }
                        }
                    }
                    // The playhead across the timeline.
                    Rectangle {
                        id: playhead
                        visible: session.hasMedia
                        x: timelinePanel.origin + session.position * timelinePanel.scale - 1
                        width: 2
                        height: timelineScroll.height
                        color: root.accent
                        Rectangle { width: 10; height: 10; radius: 5; x: -4; y: -2; color: root.accent }
                        // Keep the playhead in view while playing.
                        onXChanged: {
                            if (!session.playing) return
                            if (x > timelineScroll.contentX + timelineScroll.width - 40 || x < timelineScroll.contentX)
                                timelineScroll.contentX = Math.max(0, Math.min(timelineScroll.contentWidth - timelineScroll.width, x - 40))
                        }
                    }
                    Text {
                        visible: session.clips.length === 0
                        x: 24
                        y: 12
                        text: "Add clips with " + actions.append.keys + " or by double-clicking them in the library."
                        color: root.muted
                        font.pixelSize: 11
                    }
                }

                RowLayout {
                    id: zoomBar
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    height: 36
                    spacing: 2
                    OmaButton { action: actions.zoomOut; iconName: "zoomOut"; showLabel: false }
                    Slider {
                        Layout.preferredWidth: 110
                        from: Math.log(4)
                        to: Math.log(2400)
                        value: Math.log(timelinePanel.scale)
                        focusPolicy: Qt.NoFocus
                        onMoved: { root.fitTimeline = false; root.pixelsPerSecond = Math.exp(value) }
                    }
                    OmaButton { action: actions.zoomIn; iconName: "zoomIn"; showLabel: false }
                    OmaButton { action: actions.zoomFit; iconName: "fit"; showLabel: false; selected: root.fitTimeline }
                }
            }
        }
    }

    // Every action's shortcut, registered in one place.
    Repeater {
        model: [actions.playPause, actions.pause, actions.stop, actions.playForward, actions.playBackward, actions.previousFrame,
                actions.nextFrame, actions.back10, actions.forward10, actions.toStart, actions.toEnd,
                actions.append, actions.insert, actions.overwrite, actions.split, actions.remove,
                actions.lift, actions.undo, actions.redo, actions.importMedia,
                actions.exportMovie, actions.toggleLibrary, actions.fullViewer, actions.leaveFullViewer,
                actions.zoomIn, actions.zoomOut, actions.zoomFit, actions.volume, actions.info]
        delegate: Item {
            Shortcut {
                sequence: modelData.keys
                enabled: modelData.enabled && (session.editing || modelData === actions.importMedia)
                onActivated: modelData.trigger()
            }
        }
    }
}

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
    title: session.editing ? session.projectName + (session.dirty ? " •" : "") + " — OmaMovie" : "OmaMovie — Projects"
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
    property bool libraryHidden: false // wide windows: the library sidebar folded away, the viewer centered
    property bool viewerOnly: false
    property string drawer: "" // the open adjustment, "" when the drawer is closed
    property real upperRatio: 0.55
    property real pixelsPerSecond: 60
    property bool fitTimeline: true
    property bool snapping: true // ui-design §7.2: on by default, N toggles

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
        (session.info.gain !== 1 || session.info.fadeIn > 0 || session.info.fadeOut > 0 || session.info.muted ||
         session.info.eqLow !== 0 || session.info.eqMid !== 0 || session.info.eqHigh !== 0 || session.info.noise > 0)
    property bool volumeMore: false // the Volume drawer's "More" level (ui-design §6)
    property bool cropMore: false   // the Crop drawer's "More" level: position, scale, rotation
    property bool colorMore: false  // the Color drawer's "More" level: grading (ADR-0012)
    property string gradeTab: "wheels" // wheels, curves or lut
    property int curveChannel: 0       // 0 master, 1-3 red, green, blue
    readonly property var filterNames: ["None", "Black & White", "Sepia", "Vintage", "Cool", "Warm", "Vignette"]
    // Keep the filter previews in step with the selection while the Effects drawer is open.
    Connections {
        target: session
        function onSelectionChanged() { if (root.drawer === "effects" && actions.videoClip) session.requestFilterPreviews() }
    }
    // Equalizer presets (low, mid, high in dB); "Custom" is whatever the sliders say.
    readonly property var eqPresets: [
        { name: "Flat", bands: [0, 0, 0] },
        { name: "Voice", bands: [-6, 3, 2] },
        { name: "Reduce rumble", bands: [-12, 0, 0] },
        { name: "Bass boost", bands: [6, 0, 0] },
        { name: "Treble boost", bands: [0, 0, 6] }
    ]
    function eqPresetIndex() {
        for (let i = 0; i < eqPresets.length; ++i) {
            const b = eqPresets[i].bands
            if (b[0] === session.info.eqLow && b[1] === session.info.eqMid && b[2] === session.info.eqHigh) return i
        }
        return eqPresets.length // Custom
    }
    function decibels(gain) {
        return gain <= 0.0001 ? "−∞ dB" : (20 * Math.log(gain) / Math.LN10).toFixed(1) + " dB"
    }
    // The volume slider moves in decibels: −40 dB at the bottom means silence, +12 dB on top
    // (the most the model allows, gain 4).
    readonly property real volumeFloorDb: -40
    function gainOf(db) { return db <= volumeFloorDb ? 0 : Math.pow(10, db / 20) }
    function dbOf(gain) { return gain > 0 ? Math.max(volumeFloorDb, 20 * Math.log(gain) / Math.LN10) : volumeFloorDb }
    // Replacing or closing the session asks first when it has changes since the last save.
    // Keys a focused slider or list keeps from the window shortcuts.
    readonly property var sliderKeys: [Qt.Key_Left, Qt.Key_Right, Qt.Key_Up, Qt.Key_Down, Qt.Key_Home, Qt.Key_End,
                                       Qt.Key_PageUp, Qt.Key_PageDown]
    readonly property bool sessionHasWork: session.dirty
    property bool closeConfirmed: false
    function confirmDiscard(what, then) {
        if (!sessionHasWork) { then(); return }
        discardDialog.what = what
        discardDialog.then = then
        discardDialog.open()
    }
    onClosing: (close) => {
        if (closeConfirmed || !sessionHasWork) return
        close.accepted = false
        confirmDiscard("Close OmaMovie", () => { root.closeConfirmed = true; root.close() })
    }
    Dialog {
        id: discardDialog
        objectName: "discardDialog"
        property string what
        property var then: null
        anchors.centerIn: parent
        width: Math.min(460, root.width - 48)
        modal: true
        title: what + "?"
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: session.projectPath === "" ? "This project has not been saved. Its media and timeline will be lost."
                                             : "Changes since the last save will be lost."
        }
        // Qt's standard Discard reads "Close without Saving"; name what actually happens.
        footer: DialogButtonBox {
            Button { text: "Discard changes"; DialogButtonBox.buttonRole: DialogButtonBox.DestructiveRole }
            Button { text: "Cancel"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
        onDiscarded: { close(); if (then) then() }
    }

    // Opens the context menu of the selected clip at the clip (Shift+F10 or the Menu key).
    function openClipMenu() {
        for (const c of session.clips) {
            if (c.id !== session.selectedClip) continue
            storylineMenu.at = session.position
            storylineMenu.popup(timelineScroll.contentItem, timelinePanel.origin + c.start * timelinePanel.scale, 4 + timelinePanel.storylineHeight)
            return
        }
        for (let lane = 0; lane < session.audioTracks.length; ++lane) {
            for (const c of session.audioTracks[lane].clips) {
                if (c.id !== session.selectedClip) continue
                soundMenu.at = session.position
                soundMenu.popup(timelineScroll.contentItem, timelinePanel.origin + c.start * timelinePanel.scale,
                                timelinePanel.laneY(lane) + timelinePanel.laneHeight)
                return
            }
        }
    }

    // Opens an adjustment for the selected clip; the timeline's context menus use it.
    function openDrawer(name) {
        root.drawer = name
        if (name === "effects") session.requestFilterPreviews()
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
        property OmaAction detachAudio: OmaAction {
            text: "Detach audio"; keys: "Ctrl+Shift+S"
            enabled: actions.editing && !!session.info.canDetach
            onTriggered: session.detachAudio()
        }
        property OmaAction addDissolve: OmaAction {
            text: "Add cross dissolve"; keys: "Ctrl+T"; enabled: actions.editing && session.clips.length > 1
            onTriggered: session.addDissolveAtPlayhead()
        }
        property OmaAction previousClip: OmaAction {
            text: "Select the previous clip"; keys: "Up"; enabled: actions.editing && session.clips.length > 0
            onTriggered: session.selectAdjacentClip(-1)
        }
        property OmaAction nextClip: OmaAction {
            text: "Select the next clip"; keys: "Down"; enabled: actions.editing && session.clips.length > 0
            onTriggered: session.selectAdjacentClip(1)
        }
        property OmaAction clipMenu: OmaAction {
            text: "Clip menu"; keys: "Shift+F10"; enabled: actions.editing && session.selectedClip > 0
            onTriggered: root.openClipMenu()
        }
        property OmaAction clipMenuKey: OmaAction {
            text: "Clip menu"; keys: "Menu"; enabled: actions.clipMenu.enabled
            onTriggered: root.openClipMenu()
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
        // Project files (ADR-0007): the first save asks where, later ones replace the file.
        property OmaAction save: OmaAction {
            text: "Save"; keys: "Ctrl+S"; enabled: actions.editing && session.media.length > 0
            onTriggered: session.projectPath === "" ? saveDialog.open() : session.saveProject("")
        }
        property OmaAction openProject: OmaAction {
            text: "Open project…"; keys: "Ctrl+O"
            onTriggered: root.confirmDiscard("Open another project", () => openDialog.open())
        }
        property OmaAction exportMovie: OmaAction { text: "Export (M7)"; keys: "Ctrl+E"; enabled: false }
        // Shows or hides the library: an overlay in narrow windows, the sidebar in wide ones (the
        // viewer then takes the whole width, centered).
        property OmaAction toggleLibrary: OmaAction {
            text: "Library"; keys: "Ctrl+1"; enabled: actions.editing
            onTriggered: root.compact ? root.libraryOverlay = !root.libraryOverlay : root.libraryHidden = !root.libraryHidden
        }
        property OmaAction closeDrawer: OmaAction {
            text: "Done"; keys: "Escape"; enabled: actions.editing && root.drawer !== "" && !root.viewerOnly
            onTriggered: root.drawer = ""
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
        property OmaAction snapping: OmaAction {
            text: "Snapping"; keys: "N"; enabled: actions.editing; checkable: true; checked: root.snapping
            onTriggered: root.snapping = !root.snapping
        }
        property OmaAction volume: OmaAction {
            text: "Volume"; keys: "Ctrl+Shift+V"
            enabled: actions.editing && !!session.info.clip && !!session.info.hasAudio
            onTriggered: root.drawer = root.drawer === "volume" ? "" : "volume"
        }
        readonly property bool videoClip: actions.editing && !!session.info.clip && !!session.info.hasVideo
        property OmaAction color: OmaAction {
            text: "Color"; enabled: actions.videoClip
            onTriggered: root.drawer = root.drawer === "color" ? "" : "color"
        }
        property OmaAction crop: OmaAction {
            text: "Crop"; enabled: actions.videoClip
            onTriggered: root.drawer = root.drawer === "crop" ? "" : "crop"
        }
        property OmaAction effects: OmaAction {
            text: "Effects"; enabled: actions.videoClip
            onTriggered: {
                root.drawer = root.drawer === "effects" ? "" : "effects"
                if (root.drawer === "effects") session.requestFilterPreviews()
            }
        }
        property OmaAction info: OmaAction {
            text: "Info"; keys: "Ctrl+Shift+I"; enabled: actions.editing && !!session.info.name; checkable: true
            checked: root.drawer === "info"
            onTriggered: root.drawer = root.drawer === "info" ? "" : "info"
        }
    }

    FileDialog {
        id: saveDialog
        title: "Save project"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "omamovie"
        nameFilters: ["OmaMovie project (*.omamovie)"]
        onAccepted: session.saveProject(selectedFile)
    }

    FileDialog {
        id: openDialog
        title: "Open project"
        nameFilters: ["OmaMovie project (*.omamovie)", "All files (*)"]
        onAccepted: session.openProject(selectedFile)
    }

    FileDialog {
        id: lutDialog
        title: "Load LUT"
        nameFilters: ["3D LUT (*.cube)", "All files (*)"]
        onAccepted: session.importLut(selectedFile)
    }

    FileDialog {
        id: fileDialog
        title: "Import"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Videos, pictures and sound (*.mp4 *.mkv *.mov *.webm *.avi *.m4v *.y4m *.png *.jpg *.jpeg *.wav *.mp3 *.flac *.ogg *.opus *.m4a *.aac)",
                      "Sound (*.wav *.mp3 *.flac *.ogg *.opus *.m4a *.aac)", "All files (*)"]
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
            Text {
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
                    Text {
                        text: session.projectName + (session.dirty ? " · unsaved changes" : "")
                        color: root.fg
                        font.pixelSize: 13
                        font.bold: true
                    }
                    Text {
                        text: session.media.length + " item" + (session.media.length === 1 ? "" : "s") + " in the library"
                        color: root.muted
                        font.pixelSize: 11
                    }
                }
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: session.continueProject() }
            }
            Text {
                visible: session.media.length === 0
                text: "Nothing open yet. Start a new project or open a saved one."
                color: root.muted
                font.pixelSize: 12
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
                    objectName: "projectName"
                    text: session.projectName + (session.dirty ? " •" : "")
                    color: root.fg
                    font.pixelSize: 13
                    font.bold: true
                    elide: Text.ElideRight
                }
                OmaButton { action: actions.undo; iconName: "undo"; showLabel: false }
                OmaButton { action: actions.redo; iconName: "redo"; showLabel: false }
                Separator { Layout.leftMargin: 6; Layout.rightMargin: 6 }
                OmaButton {
                    objectName: "toggleLibrary"
                    action: actions.toggleLibrary
                    iconName: "library"
                    showLabel: false
                    selected: root.compact ? root.libraryOverlay : !root.libraryHidden
                }
                OmaButton { action: actions.save; iconName: "save"; showLabel: !root.compact }
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
                                // The selected item onto the timeline; ▾ offers insert and overwrite.
                                OmaButton {
                                    visible: actions.mediaChosen
                                    action: actions.append
                                    text: "Add"
                                    iconName: "append"
                                }
                                OmaButton {
                                    id: placeMore
                                    visible: actions.mediaChosen
                                    text: "▾"
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
                                            Icon { // sound has no picture
                                                anchors.centerIn: parent
                                                visible: modelData.audioOnly
                                                name: "music"
                                                size: 28
                                                color: colors.green || root.fg
                                            }
                                        }
                                        Text { Layout.fillWidth: true; text: modelData.name; color: root.fg; font.pixelSize: 12; elide: Text.ElideMiddle }
                                        Text { text: root.timecode(modelData.duration); color: root.muted; font.pixelSize: 11 }
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
                                // What a library item can do on the timeline (ui-design §4).
                                Menu {
                                    id: libraryMenu
                                    objectName: "libraryMenu"
                                    MenuItem { action: actions.append }
                                    MenuItem { action: actions.insert }
                                    MenuItem { action: actions.overwrite }
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
                    objectName: "viewerColumn"
                    x: root.compact || root.viewerOnly || root.libraryHidden ? 0 : library.width
                    width: upper.width - x
                    height: upper.height
                    spacing: 0

                    // The open drawer's title strip (§6): adjustments open from the clip's context menu
                    // (or their shortcuts), so there is no permanent bar of icons; this names what is
                    // open and closes it.
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        visible: !root.viewerOnly && root.drawer !== ""
                        color: colors.background
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 6
                            spacing: 6
                            Text {
                                Layout.fillWidth: true
                                text: ({ color: "Color", crop: "Crop and framing", volume: "Volume", effects: "Effects",
                                         info: "Info" })[root.drawer] + (session.info.name ? " — " + session.info.name : "")
                                color: root.fg
                                font.pixelSize: 12
                                font.bold: true
                                elide: Text.ElideRight
                            }
                            OmaButton { objectName: "closeDrawer"; action: actions.closeDrawer; text: "Done" }
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

                    // Volume drawer (§6): volume, fades and mute for the selected clip; "More" adds the
                    // equalizer, noise reduction and normalize. Values follow the clip; a change is
                    // committed as one command on release.
                    Rectangle {
                        id: volumeDrawer
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.volumeMore ? 124 : 64
                        visible: !root.viewerOnly && root.drawer === "volume" && actions.volume.enabled
                        color: colors.dark_background
                        function commit() {
                            session.setClipAudio(root.gainOf(gainControl.slider.value), fadeInControl.slider.value,
                                                 fadeOutControl.slider.value, !!session.info.muted)
                        }
                        function commitEq() {
                            session.setClipEq(lowControl.slider.value, midControl.slider.value, highControl.slider.value)
                        }
                        component DrawerSlider: ColumnLayout {
                            id: drawerSlider
                            property alias slider: control
                            property string label
                            property string readout
                            signal committed()
                            spacing: 0
                            Layout.fillWidth: true
                            RowLayout {
                                Text { text: drawerSlider.label; color: root.muted; font.pixelSize: 10; font.bold: true }
                                Item { Layout.fillWidth: true }
                                Text { text: drawerSlider.readout; color: root.fg; font.pixelSize: 11 }
                            }
                            Slider {
                                id: control
                                Layout.fillWidth: true
                                // Tab reaches it; while focused, the arrows and Home/End move it
                                // instead of the playhead, and each key step is one command.
                                focusPolicy: Qt.TabFocus
                                Keys.onShortcutOverride: (event) => event.accepted = root.sliderKeys.includes(event.key)
                                onPressedChanged: if (!pressed) drawerSlider.committed()
                                onMoved: if (!pressed) drawerSlider.committed()
                            }
                        }
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            anchors.topMargin: 6
                            anchors.bottomMargin: 6
                            spacing: 6
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 18
                                DrawerSlider {
                                    id: gainControl
                                    label: "VOLUME"
                                    readout: root.decibels(root.gainOf(gainControl.slider.value))
                                    onCommitted: volumeDrawer.commit()
                                }
                                DrawerSlider {
                                    id: fadeInControl
                                    label: "FADE IN"
                                    readout: fadeInControl.slider.value.toFixed(2) + " s"
                                    onCommitted: volumeDrawer.commit()
                                }
                                DrawerSlider {
                                    id: fadeOutControl
                                    label: "FADE OUT"
                                    readout: fadeOutControl.slider.value.toFixed(2) + " s"
                                    onCommitted: volumeDrawer.commit()
                                }
                                OmaButton {
                                    text: session.info.muted ? "Unmute" : "Mute"
                                    selected: !!session.info.muted
                                    onClicked: session.setClipAudio(root.gainOf(gainControl.slider.value), fadeInControl.slider.value,
                                                                    fadeOutControl.slider.value, !session.info.muted)
                                }
                                OmaButton {
                                    text: "More"
                                    selected: root.volumeMore
                                    tip: "Equalizer, noise reduction, normalize"
                                    onClicked: root.volumeMore = !root.volumeMore
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                visible: root.volumeMore
                                spacing: 18
                                ColumnLayout {
                                    spacing: 2
                                    Text { text: "EQUALIZER"; color: root.muted; font.pixelSize: 10; font.bold: true }
                                    ComboBox {
                                        id: eqPreset
                                        Layout.preferredWidth: 140
                                        font.pixelSize: 11
                                        focusPolicy: Qt.TabFocus
                                        Keys.onShortcutOverride: (event) => event.accepted = root.sliderKeys.includes(event.key) || event.key === Qt.Key_Space
                                        model: root.eqPresets.map(p => p.name).concat(["Custom"])
                                        currentIndex: root.eqPresetIndex()
                                        onActivated: (index) => {
                                            if (index < root.eqPresets.length) {
                                                const b = root.eqPresets[index].bands
                                                session.setClipEq(b[0], b[1], b[2])
                                            }
                                        }
                                    }
                                }
                                DrawerSlider {
                                    id: lowControl
                                    label: "LOW"
                                    readout: lowControl.slider.value.toFixed(1) + " dB"
                                    onCommitted: volumeDrawer.commitEq()
                                }
                                DrawerSlider {
                                    id: midControl
                                    label: "MID"
                                    readout: midControl.slider.value.toFixed(1) + " dB"
                                    onCommitted: volumeDrawer.commitEq()
                                }
                                DrawerSlider {
                                    id: highControl
                                    label: "HIGH"
                                    readout: highControl.slider.value.toFixed(1) + " dB"
                                    onCommitted: volumeDrawer.commitEq()
                                }
                                DrawerSlider {
                                    id: noiseControl
                                    label: "NOISE REDUCTION"
                                    readout: noiseControl.slider.value > 0 ? Math.round(noiseControl.slider.value * 100) + "%" : "Off"
                                    onCommitted: session.setClipNoise(noiseControl.slider.value)
                                }
                                OmaButton {
                                    text: "Normalize"
                                    tip: "Raise or lower the volume so the loudest peak reaches −1 dB"
                                    onClicked: session.normalizeClip()
                                }
                            }
                        }
                        // The sliders follow the selected clip, except while one is being dragged.
                        Binding { target: gainControl.slider; property: "from"; value: root.volumeFloorDb }
                        Binding { target: gainControl.slider; property: "to"; value: 12 }
                        Binding { target: gainControl.slider; property: "value"; value: root.dbOf(session.info.gain || 0); when: !gainControl.slider.pressed }
                        Binding { target: fadeInControl.slider; property: "to"; value: Math.max(0.01, session.info.clipDuration || 0) }
                        Binding { target: fadeInControl.slider; property: "value"; value: session.info.fadeIn || 0; when: !fadeInControl.slider.pressed }
                        Binding { target: fadeOutControl.slider; property: "to"; value: Math.max(0.01, session.info.clipDuration || 0) }
                        Binding { target: fadeOutControl.slider; property: "value"; value: session.info.fadeOut || 0; when: !fadeOutControl.slider.pressed }
                        Binding { target: lowControl.slider; property: "from"; value: -12 }
                        Binding { target: lowControl.slider; property: "to"; value: 12 }
                        Binding { target: lowControl.slider; property: "stepSize"; value: 0.5 }
                        Binding { target: lowControl.slider; property: "value"; value: session.info.eqLow || 0; when: !lowControl.slider.pressed }
                        Binding { target: midControl.slider; property: "from"; value: -12 }
                        Binding { target: midControl.slider; property: "to"; value: 12 }
                        Binding { target: midControl.slider; property: "stepSize"; value: 0.5 }
                        Binding { target: midControl.slider; property: "value"; value: session.info.eqMid || 0; when: !midControl.slider.pressed }
                        Binding { target: highControl.slider; property: "from"; value: -12 }
                        Binding { target: highControl.slider; property: "to"; value: 12 }
                        Binding { target: highControl.slider; property: "stepSize"; value: 0.5 }
                        Binding { target: highControl.slider; property: "value"; value: session.info.eqHigh || 0; when: !highControl.slider.pressed }
                        Binding { target: noiseControl.slider; property: "value"; value: session.info.noise || 0; when: !noiseControl.slider.pressed }
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: root.line }
                    }

                    // Color drawer (§6): exposure, contrast, saturation and temperature of the clip;
                    // "More" adds grading (ADR-0012): lift/gamma/gain wheels, curves and a LUT.
                    Rectangle {
                        id: colorDrawer
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.colorMore ? 64 + 150 : 64
                        visible: !root.viewerOnly && root.drawer === "color" && actions.color.enabled
                        color: colors.dark_background
                        function commit() {
                            session.setClipColor(exposureControl.slider.value, contrastControl.slider.value,
                                                 saturationControl.slider.value, temperatureControl.slider.value)
                        }
                        function signed(v) { return (v > 0 ? "+" : "") + Math.round(v * 100) }
                        // The three wheels as the session wants them, with one changed.
                        function commitWheel(name, x, y, level) {
                            const w = session.info.wheels || {}
                            const all = { lift: w.lift || {}, gamma: w.gamma || {}, gain: w.gain || {} }
                            all[name] = { x: x, y: y, level: level }
                            session.setClipWheels(all)
                        }
                        RowLayout {
                            id: colorBasics
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            height: 64
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 18
                            DrawerSlider {
                                id: exposureControl
                                label: "EXPOSURE"
                                readout: (exposureControl.slider.value > 0 ? "+" : "") + exposureControl.slider.value.toFixed(2) + " EV"
                                onCommitted: colorDrawer.commit()
                            }
                            DrawerSlider {
                                id: contrastControl
                                label: "CONTRAST"
                                readout: colorDrawer.signed(contrastControl.slider.value)
                                onCommitted: colorDrawer.commit()
                            }
                            DrawerSlider {
                                id: saturationControl
                                label: "SATURATION"
                                readout: colorDrawer.signed(saturationControl.slider.value)
                                onCommitted: colorDrawer.commit()
                            }
                            DrawerSlider {
                                id: temperatureControl
                                label: "TEMPERATURE"
                                readout: temperatureControl.slider.value < 0 ? "Cooler " + Math.round(-temperatureControl.slider.value * 100)
                                       : temperatureControl.slider.value > 0 ? "Warmer " + Math.round(temperatureControl.slider.value * 100) : "Neutral"
                                onCommitted: colorDrawer.commit()
                            }
                            OmaButton {
                                text: "Reset"
                                enabled: !!session.info.colorAdjusted
                                onClicked: { session.setClipColor(0, 0, 0, 0); session.resetClipGrade() }
                            }
                            OmaButton {
                                text: "More"
                                selected: root.colorMore
                                activeDot: !!session.info.graded
                                tip: "Color wheels, curves and LUTs"
                                onClicked: root.colorMore = !root.colorMore
                            }
                        }
                        // Grading: one tab at a time, so the drawer stays short.
                        RowLayout {
                            visible: root.colorMore
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: colorBasics.bottom
                            anchors.bottom: parent.bottom
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            anchors.bottomMargin: 8
                            spacing: 14
                            ColumnLayout {
                                Layout.alignment: Qt.AlignTop
                                Layout.preferredWidth: 84
                                Layout.fillWidth: false
                                spacing: 2
                                Repeater {
                                    model: [{ name: "Wheels", tab: "wheels" }, { name: "Curves", tab: "curves" }, { name: "LUT", tab: "lut" }]
                                    delegate: OmaButton {
                                        Layout.fillWidth: true
                                        text: modelData.name
                                        selected: root.gradeTab === modelData.tab
                                        onClicked: root.gradeTab = modelData.tab
                                    }
                                }
                            }
                            RowLayout { // lift, gamma, gain
                                visible: root.gradeTab === "wheels"
                                Layout.fillWidth: true
                                spacing: 18
                                Repeater {
                                    model: [{ name: "LIFT", key: "lift" }, { name: "GAMMA", key: "gamma" }, { name: "GAIN", key: "gain" }]
                                    delegate: ColorWheel {
                                        required property var modelData
                                        readonly property var value: (session.info.wheels || {})[modelData.key] || {}
                                        label: modelData.name
                                        tintX: value.x || 0
                                        tintY: value.y || 0
                                        level: value.level || 0
                                        onCommitted: (x, y, level) => colorDrawer.commitWheel(modelData.key, x, y, level)
                                    }
                                }
                                Item { Layout.fillWidth: true }
                            }
                            RowLayout { // curves
                                visible: root.gradeTab === "curves"
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 10
                                ColumnLayout {
                                    Layout.alignment: Qt.AlignTop
                                    spacing: 2
                                    Repeater {
                                        model: ["Master", "Red", "Green", "Blue"]
                                        delegate: OmaButton {
                                            Layout.fillWidth: true
                                            text: modelData
                                            selected: root.curveChannel === index
                                            activeDot: ((session.info.curves || {})[String(index)] || []).length > 0
                                            onClicked: root.curveChannel = index
                                        }
                                    }
                                }
                                CurveEditor {
                                    Layout.preferredWidth: 180
                                    Layout.fillHeight: true
                                    channel: root.curveChannel
                                    points: (session.info.curves || {})[String(root.curveChannel)] || []
                                    // Re-read whenever the selection or its properties change.
                                    samples: session.info && session.info.clip ? session.curveSamples(root.curveChannel, 96) : []
                                }
                                Text {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignTop
                                    wrapMode: Text.WordWrap
                                    text: "Click to add a point, drag to move it, double-click to remove it."
                                    color: root.muted
                                    font.pixelSize: 11
                                }
                            }
                            ColumnLayout { // LUT
                                visible: root.gradeTab === "lut"
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignTop
                                spacing: 8
                                RowLayout {
                                    spacing: 8
                                    ComboBox {
                                        id: lutChoice
                                        Layout.preferredWidth: 200
                                        font.pixelSize: 11
                                        focusPolicy: Qt.TabFocus
                                        Keys.onShortcutOverride: (event) => event.accepted = root.sliderKeys.includes(event.key) || event.key === Qt.Key_Space
                                        textRole: "name"
                                        model: [{ id: 0, name: "No LUT" }].concat(session.luts)
                                        currentIndex: Math.max(0, model.findIndex(l => l.id === (session.info.lut || 0)))
                                        onActivated: (index) => session.setClipLut(model[index].id, lutAmount.slider.value)
                                    }
                                    OmaButton { text: "Load LUT…"; tip: "A 3D LUT in the .cube format"; onClicked: lutDialog.open() }
                                }
                                DrawerSlider {
                                    id: lutAmount
                                    Layout.maximumWidth: 280
                                    enabled: (session.info.lut || 0) > 0
                                    label: "AMOUNT"
                                    readout: Math.round(lutAmount.slider.value * 100) + "%"
                                    onCommitted: session.setClipLut(session.info.lut || 0, lutAmount.slider.value)
                                }
                            }
                        }
                        Binding { target: lutAmount.slider; property: "value"; value: session.info.lutAmount === undefined ? 1 : session.info.lutAmount; when: !lutAmount.slider.pressed }
                        Binding { target: exposureControl.slider; property: "from"; value: -2 }
                        Binding { target: exposureControl.slider; property: "to"; value: 2 }
                        Binding { target: exposureControl.slider; property: "value"; value: session.info.exposure || 0; when: !exposureControl.slider.pressed }
                        Binding { target: contrastControl.slider; property: "from"; value: -1 }
                        Binding { target: contrastControl.slider; property: "value"; value: session.info.contrast || 0; when: !contrastControl.slider.pressed }
                        Binding { target: saturationControl.slider; property: "from"; value: -1 }
                        Binding { target: saturationControl.slider; property: "value"; value: session.info.saturation || 0; when: !saturationControl.slider.pressed }
                        Binding { target: temperatureControl.slider; property: "from"; value: -1 }
                        Binding { target: temperatureControl.slider; property: "value"; value: session.info.temperature || 0; when: !temperatureControl.slider.pressed }
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: root.line }
                    }

                    // Crop drawer (§6): fit mode and edges; "More" adds position, scale and rotation.
                    Rectangle {
                        id: cropDrawer
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.cropMore ? 124 : 64
                        visible: !root.viewerOnly && root.drawer === "crop" && actions.crop.enabled
                        color: colors.dark_background
                        function commitFraming(fit) {
                            session.setClipFraming(fit, cropLeft.slider.value, cropTop.slider.value,
                                                   cropRight.slider.value, cropBottom.slider.value)
                        }
                        function commitTransform() {
                            session.setClipTransform(posX.slider.value, posY.slider.value, scaleControl.slider.value,
                                                     rotationControl.slider.value)
                        }
                        function percent(v) { return Math.round(v * 100) + "%" }
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            anchors.topMargin: 6
                            anchors.bottomMargin: 6
                            spacing: 6
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 14
                                Row {
                                    spacing: 2
                                    Repeater {
                                        model: [{ name: "Fit", fit: 0, tip: "Whole picture, letterboxed" },
                                                { name: "Fill", fit: 1, tip: "Fill the frame, cropping what overflows" },
                                                { name: "Stretch", fit: 2, tip: "Fill the frame, changing the proportions" }]
                                        delegate: OmaButton {
                                            text: modelData.name
                                            tip: modelData.tip
                                            selected: (session.info.fit || 0) === modelData.fit
                                            onClicked: cropDrawer.commitFraming(modelData.fit)
                                        }
                                    }
                                }
                                DrawerSlider { id: cropLeft; label: "LEFT"; readout: cropDrawer.percent(cropLeft.slider.value); onCommitted: cropDrawer.commitFraming(session.info.fit || 0) }
                                DrawerSlider { id: cropRight; label: "RIGHT"; readout: cropDrawer.percent(cropRight.slider.value); onCommitted: cropDrawer.commitFraming(session.info.fit || 0) }
                                DrawerSlider { id: cropTop; label: "TOP"; readout: cropDrawer.percent(cropTop.slider.value); onCommitted: cropDrawer.commitFraming(session.info.fit || 0) }
                                DrawerSlider { id: cropBottom; label: "BOTTOM"; readout: cropDrawer.percent(cropBottom.slider.value); onCommitted: cropDrawer.commitFraming(session.info.fit || 0) }
                                OmaButton {
                                    text: "More"
                                    selected: root.cropMore
                                    tip: "Position, scale and rotation"
                                    onClicked: root.cropMore = !root.cropMore
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                visible: root.cropMore
                                spacing: 18
                                DrawerSlider { id: posX; label: "POSITION X"; readout: Math.round(posX.slider.value) + " px"; onCommitted: cropDrawer.commitTransform() }
                                DrawerSlider { id: posY; label: "POSITION Y"; readout: Math.round(posY.slider.value) + " px"; onCommitted: cropDrawer.commitTransform() }
                                DrawerSlider { id: scaleControl; label: "SCALE"; readout: Math.round(scaleControl.slider.value * 100) + "%"; onCommitted: cropDrawer.commitTransform() }
                                DrawerSlider { id: rotationControl; label: "ROTATION"; readout: rotationControl.slider.value.toFixed(1) + "°"; onCommitted: cropDrawer.commitTransform() }
                                // Keyframes (M8): with keys, the sliders above set the one at the playhead.
                                OmaButton {
                                    objectName: "transformKey"
                                    text: session.motion.keyHere ? "◆ Remove key" : "◇ Add key"
                                    tip: session.motion.keys > 0
                                         ? session.motion.keys + " keys: the sliders set the one at the playhead"
                                         : "Animate: keep this framing at the playhead"
                                    selected: !!session.motion.keyHere
                                    onClicked: session.toggleTransformKey()
                                }
                                OmaButton {
                                    text: "Ken Burns"
                                    tip: "A slow push in over the whole clip"
                                    onClicked: session.kenBurns()
                                }
                                OmaButton {
                                    text: "Reset"
                                    enabled: !!session.info.framingAdjusted
                                    onClicked: session.resetClipFraming()
                                }
                            }
                        }
                        Binding { target: cropLeft.slider; property: "to"; value: 0.45 }
                        Binding { target: cropLeft.slider; property: "value"; value: session.info.cropLeft || 0; when: !cropLeft.slider.pressed }
                        Binding { target: cropRight.slider; property: "to"; value: 0.45 }
                        Binding { target: cropRight.slider; property: "value"; value: session.info.cropRight || 0; when: !cropRight.slider.pressed }
                        Binding { target: cropTop.slider; property: "to"; value: 0.45 }
                        Binding { target: cropTop.slider; property: "value"; value: session.info.cropTop || 0; when: !cropTop.slider.pressed }
                        Binding { target: cropBottom.slider; property: "to"; value: 0.45 }
                        Binding { target: cropBottom.slider; property: "value"; value: session.info.cropBottom || 0; when: !cropBottom.slider.pressed }
                        Binding { target: posX.slider; property: "from"; value: -session.canvasWidth }
                        Binding { target: posX.slider; property: "to"; value: session.canvasWidth }
                        Binding { target: posX.slider; property: "value"; value: session.motion.posX || 0; when: !posX.slider.pressed }
                        Binding { target: posY.slider; property: "from"; value: -session.canvasHeight }
                        Binding { target: posY.slider; property: "to"; value: session.canvasHeight }
                        Binding { target: posY.slider; property: "value"; value: session.motion.posY || 0; when: !posY.slider.pressed }
                        Binding { target: scaleControl.slider; property: "from"; value: 0.1 }
                        Binding { target: scaleControl.slider; property: "to"; value: 4 }
                        Binding { target: scaleControl.slider; property: "value"; value: session.motion.scale || 1; when: !scaleControl.slider.pressed }
                        Binding { target: rotationControl.slider; property: "from"; value: -180 }
                        Binding { target: rotationControl.slider; property: "to"; value: 180 }
                        Binding { target: rotationControl.slider; property: "value"; value: session.motion.rotation || 0; when: !rotationControl.slider.pressed }
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: root.line }
                    }

                    // Effects drawer (§6): one filter per clip, chosen from previews of the clip itself.
                    Rectangle {
                        id: effectsDrawer
                        Layout.fillWidth: true
                        Layout.preferredHeight: 104
                        visible: !root.viewerOnly && root.drawer === "effects" && actions.effects.enabled
                        color: colors.dark_background
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 14
                            ListView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.topMargin: 8
                                orientation: ListView.Horizontal
                                spacing: 8
                                clip: true
                                model: root.filterNames
                                delegate: Item {
                                    width: 92
                                    height: 84
                                    readonly property bool chosen: (session.info.filterKind || 0) === index
                                    Rectangle {
                                        id: tile
                                        width: parent.width
                                        height: 54
                                        radius: 4
                                        color: root.viewerBackground
                                        border.width: chosen ? 2 : 0
                                        border.color: root.accent
                                        Image {
                                            anchors.fill: parent
                                            anchors.margins: chosen ? 2 : 0
                                            fillMode: Image.PreserveAspectFit
                                            source: session.filterPreviews.length > index ? session.filterPreviews[index] : ""
                                            asynchronous: true
                                        }
                                    }
                                    Text {
                                        anchors.top: tile.bottom
                                        anchors.topMargin: 4
                                        width: parent.width
                                        horizontalAlignment: Text.AlignHCenter
                                        text: modelData
                                        color: chosen ? root.accent : root.fg
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: session.setClipFilter(index, index === 0 ? 1 : amountControl.slider.value)
                                    }
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: false
                                Layout.preferredWidth: 180
                                spacing: 6
                                DrawerSlider {
                                    id: amountControl
                                    enabled: (session.info.filterKind || 0) !== 0
                                    label: "AMOUNT"
                                    readout: Math.round(amountControl.slider.value * 100) + "%"
                                    onCommitted: session.setClipFilter(session.info.filterKind || 0, amountControl.slider.value)
                                }
                                DrawerSlider {
                                    id: sharpnessControl
                                    label: "SOFTEN · SHARPEN"
                                    readout: sharpnessControl.slider.value < 0 ? "Soften " + Math.round(-sharpnessControl.slider.value * 100)
                                           : sharpnessControl.slider.value > 0 ? "Sharpen " + Math.round(sharpnessControl.slider.value * 100) : "Off"
                                    onCommitted: session.setClipSharpness(sharpnessControl.slider.value)
                                }
                            }
                        }
                        Binding { target: amountControl.slider; property: "value"; value: session.info.filterAmount === undefined ? 1 : session.info.filterAmount; when: !amountControl.slider.pressed }
                        Binding { target: sharpnessControl.slider; property: "from"; value: -1 }
                        Binding { target: sharpnessControl.slider; property: "value"; value: session.info.sharpness || 0; when: !sharpnessControl.slider.pressed }
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
                        // Timecode on the left, the playback controls centered on the viewer, the
                        // full-screen toggle on the right.
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.timecode(session.position) + (root.narrow ? "" : " / " + root.timecode(session.duration))
                            color: session.hasMedia ? root.fg : root.muted
                            font.pixelSize: 12
                        }
                        Row {
                            anchors.centerIn: parent
                            spacing: 2
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
                        }
                        Row {
                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 6
                            Text { // shuttle speed (J/K/L), shown only when not normal
                                anchors.verticalCenter: parent.verticalCenter
                                visible: session.speed !== 0 && session.speed !== 1
                                text: (session.speed < 0 ? "−" : "") + Math.abs(session.speed) + "×"
                                color: root.accent
                                font.pixelSize: 12
                                font.bold: true
                            }
                            OmaButton {
                                action: root.viewerOnly ? actions.leaveFullViewer : actions.fullViewer
                                iconName: "fullscreen"
                                showLabel: false
                                selected: root.viewerOnly
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

            // Timeline (§7): minimap, storyline and zoom. Edits come from the context menus and
            // the shortcuts.
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
                // The storyline on top, audio lanes below it (ui-design §7.1).
                readonly property real laneHeight: 34
                readonly property real laneGap: 4
                readonly property int lanes: session.audioTracks.length
                readonly property real storylineHeight:
                    Math.min(86, Math.max(44, timelineScroll.height - 12 - lanes * (laneHeight + laneGap)))
                function laneY(index) { return 4 + storylineHeight + 8 + index * (laneHeight + laneGap) }
                // Where a dragged span starting at `seconds` lands, snapped within 8 px when
                // snapping is on; shows the snap line while it holds. Cleared by unsnap().
                property real snapLine: -1
                function snap(seconds, length, exclude) {
                    if (!root.snapping) { snapLine = -1; return seconds }
                    const r = session.snapSpan(seconds, length, exclude, 8 / scale)
                    snapLine = r.line
                    return r.start
                }
                function unsnap() { snapLine = -1 }
                // Sets the zoom keeping the time under content x `anchorX` where it is on screen.
                function zoomAround(pixelsPerSecond, anchorX) {
                    const seconds = timelineScroll.secondsAt(anchorX)
                    const onScreen = anchorX - timelineScroll.contentX
                    root.fitTimeline = false
                    root.pixelsPerSecond = Math.min(2400, Math.max(4, pixelsPerSecond))
                    const x = origin + seconds * root.pixelsPerSecond - onScreen
                    timelineScroll.contentX = Math.max(0, Math.min(timelineScroll.contentWidth - timelineScroll.width, x))
                }

                // Drag a clip edge to trim; committed as one command on release. The storyline is
                // magnetic (ripple trim, ui-design §7.3); lanes are not.
                component TrimEdge: MouseArea {
                    required property Item owner
                    required property var clip
                    readonly property double clipId: clip.id
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
                        const edge = head ? clip.start : clip.start + clip.duration
                        const dx = mapToItem(timelineScroll.contentItem, mouse.x, 0).x - pressX
                        const snapped = timelinePanel.snap(edge + dx / timelinePanel.scale, 0, clipId)
                        if (head) owner.headDrag = (snapped - edge) * timelinePanel.scale
                        else owner.tailDrag = (snapped - edge) * timelinePanel.scale
                    }
                    onReleased: {
                        const dx = head ? owner.headDrag : owner.tailDrag
                        owner.headDrag = 0
                        owner.tailDrag = 0
                        timelinePanel.unsnap()
                        const frames = Math.round(dx / timelinePanel.scale * session.frameRate)
                        if (frames !== 0) session.trimClip(clipId, head, frames)
                    }
                }

                // The sound of a clip: the media range it shows, scaled by its volume.
                component ClipWaveform: WaveformItem {
                    required property var clip
                    store: session.waveforms
                    media: clip.media
                    from: clip.sourceIn
                    length: clip.duration * clip.speed
                    gain: clip.gain
                }

                // A fade ramp over each end of a clip, as long as the fade.
                component FadeRamps: Shape {
                    id: ramps
                    required property var clip
                    anchors.fill: parent
                    visible: clip.fadeIn > 0 || clip.fadeOut > 0
                    preferredRendererType: Shape.CurveRenderer
                    readonly property real fadeInWidth: clip.fadeIn * timelinePanel.scale
                    readonly property real fadeOutWidth: clip.fadeOut * timelinePanel.scale
                    ShapePath {
                        strokeColor: "transparent"
                        fillColor: Qt.rgba(0, 0, 0, 0.45)
                        startX: 0; startY: 0
                        PathLine { x: ramps.fadeInWidth; y: 0 }
                        PathLine { x: 0; y: ramps.height }
                        PathLine { x: 0; y: 0 }
                    }
                    ShapePath {
                        strokeColor: "transparent"
                        fillColor: Qt.rgba(0, 0, 0, 0.45)
                        startX: ramps.width; startY: 0
                        PathLine { x: ramps.width - ramps.fadeOutWidth; y: 0 }
                        PathLine { x: ramps.width; y: ramps.height }
                        PathLine { x: ramps.width; y: 0 }
                    }
                }

                // Minimap: the whole project, with the visible region highlighted.
                Item {
                    id: minimap
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 12
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    height: 14
                    visible: session.hasMedia
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
                        // Mouse-centered (ui-design §7.2): the time under the pointer stays put.
                        onWheel: (event) => timelinePanel.zoomAround(
                            timelinePanel.scale * (event.angleDelta.y > 0 ? 1.25 : 0.8), event.x + timelineScroll.contentX)
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
                    // Library items dropped on the timeline (ui-design §7.3): pictures are inserted at
                    // the nearest storyline cut, sound lands on the lane under the pointer.
                    DropArea {
                        id: mediaDrop
                        width: timelineScroll.contentWidth
                        height: timelineScroll.height
                        keys: ["oma/media"]
                        readonly property var item: mediaDrag.item
                        readonly property bool sound: !!item && item.audioOnly
                        readonly property real seconds: timelineScroll.secondsAt(drag.x)
                        // The audio lane under the pointer, top first; -1 over the storyline.
                        readonly property int lane: drag.y < timelinePanel.laneY(0) - timelinePanel.laneGap ? -1
                            : Math.floor((drag.y - timelinePanel.laneY(0)) / (timelinePanel.laneHeight + timelinePanel.laneGap))
                        onPositionChanged: if (item && !sound) dropMarker.cut = session.storylineCut(seconds, 0)
                        onContainsDragChanged: dropMarker.shown = containsDrag && !!item && !sound
                        onDropped: (drop) => {
                            dropMarker.shown = false
                            if (!item) return
                            session.dropMedia(mediaDrag.index, seconds, lane)
                            root.libraryOverlay = false
                            drop.accept()
                        }
                        Rectangle { // where dropped sound lands
                            visible: mediaDrop.containsDrag && mediaDrop.sound
                            x: timelinePanel.origin + mediaDrop.seconds * timelinePanel.scale
                            y: timelinePanel.laneY(Math.max(0, Math.min(mediaDrop.lane, timelinePanel.lanes)))
                            width: (mediaDrop.item ? mediaDrop.item.duration : 0) * timelinePanel.scale
                            height: timelinePanel.laneHeight
                            radius: 4
                            color: "transparent"
                            border.color: root.accent
                            border.width: 2
                        }
                    }
                    Repeater {
                        model: session.clips
                        delegate: Rectangle {
                            id: clipItem
                            objectName: "storylineClip"
                            readonly property bool chosen: modelData.id === session.selectedClip
                            // Live feedback while an edge is dragged; committed as one trim on release.
                            property real headDrag: 0
                            property real tailDrag: 0
                            property real moveX: 0 // live feedback while the clip is dragged to a new place
                            x: timelinePanel.origin + modelData.start * timelinePanel.scale + moveX
                            z: moveX !== 0 ? 2 : 0
                            opacity: moveX !== 0 ? 0.8 : 1
                            y: 4
                            width: Math.max(6, modelData.duration * timelinePanel.scale - 2 - headDrag + tailDrag)
                            height: timelinePanel.storylineHeight
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
                            ClipWaveform {
                                clip: modelData
                                visible: modelData.hasAudio
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: 18
                                color: Qt.rgba(0.55, 0.75, 1, 0.45)
                            }
                            FadeRamps { clip: modelData }
                            Text {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 6
                                anchors.bottomMargin: 4
                                text: modelData.name
                                color: root.fg
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                preventStealing: true
                                property real pressX: 0
                                property bool moving: false
                                cursorShape: moving ? Qt.ClosedHandCursor : Qt.ArrowCursor
                                // A click selects the clip and moves the playhead to it; a right
                                // click opens its menu; a drag moves it to another cut (magnetic).
                                onPressed: (mouse) => {
                                    session.selectClip(modelData.id)
                                    const at = modelData.start + mouse.x / timelinePanel.scale
                                    if (mouse.button === Qt.RightButton) {
                                        storylineMenu.at = at
                                        storylineMenu.popup()
                                        return
                                    }
                                    pressX = mapToItem(timelineScroll.contentItem, mouse.x, 0).x
                                    moving = false
                                    session.seek(at)
                                }
                                onPositionChanged: (mouse) => {
                                    if (!(pressedButtons & Qt.LeftButton)) return
                                    const x = mapToItem(timelineScroll.contentItem, mouse.x, 0).x
                                    if (!moving && Math.abs(x - pressX) < 6) return
                                    moving = true
                                    clipItem.moveX = x - pressX
                                    dropMarker.cut = session.storylineCut(timelineScroll.secondsAt(x), modelData.id)
                                    dropMarker.shown = true
                                }
                                function settle() {
                                    moving = false
                                    clipItem.moveX = 0
                                    dropMarker.shown = false
                                }
                                onReleased: (mouse) => {
                                    const seconds = timelineScroll.secondsAt(mapToItem(timelineScroll.contentItem, mouse.x, 0).x)
                                    const wasMoving = moving
                                    settle() // before the edit, which rebuilds this delegate
                                    if (wasMoving) session.reorderClip(modelData.id, seconds)
                                }
                                onCanceled: settle()
                            }
                            TrimEdge { owner: clipItem; clip: modelData; head: true; anchors.left: parent.left }
                            TrimEdge { owner: clipItem; clip: modelData; head: false; anchors.right: parent.right }
                        }
                    }
                    // The cut where a dragged clip or library item will land on the storyline.
                    Rectangle {
                        id: dropMarker
                        property real cut: 0
                        property bool shown: false
                        visible: shown
                        x: timelinePanel.origin + cut * timelinePanel.scale - width / 2
                        z: 4
                        width: 3
                        height: timelinePanel.storylineHeight + 8
                        radius: 1.5
                        color: root.accent
                    }
                    // Cuts between touching storyline clips (ui-design §7.2): the span a transition
                    // covers, and a ⋈ marker that edits it.
                    Repeater {
                        model: session.clips
                        delegate: Item {
                            id: junction
                            required property var modelData
                            visible: modelData.joined
                            readonly property real cutX: timelinePanel.origin + modelData.start * timelinePanel.scale
                            Rectangle { // the transition's span
                                visible: junction.modelData.transitionKind >= 0
                                x: junction.cutX - width / 2
                                y: 4
                                width: junction.modelData.transitionSpan * timelinePanel.scale
                                height: timelinePanel.storylineHeight
                                color: Qt.rgba(1, 1, 1, 0.12)
                                border.color: root.accent
                                border.width: 1
                                radius: 3
                            }
                            Rectangle {
                                x: junction.cutX - width / 2
                                y: 4 + timelinePanel.storylineHeight / 2 - height / 2
                                width: 20
                                height: 20
                                radius: 10
                                z: 3
                                color: junction.modelData.transitionSet ? root.accent : colors.dark_background
                                border.color: root.line
                                Icon {
                                    anchors.centerIn: parent
                                    name: "transition"
                                    size: 12
                                    color: junction.modelData.transitionSet ? colors.background : root.fg
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                    cursorShape: Qt.PointingHandCursor
                                    ToolTip.visible: containsMouse
                                    ToolTip.text: "Transition"
                                    hoverEnabled: true
                                    onClicked: {
                                        transitionMenu.clipId = junction.modelData.id
                                        transitionMenu.kind = junction.modelData.transitionSet ? junction.modelData.transitionKind : -1
                                        transitionMenu.popup()
                                    }
                                }
                            }
                        }
                    }
                    Menu {
                        id: transitionMenu
                        property double clipId: 0 // `clip` is a Menu property
                        property int kind: -1
                        property real seconds: 1
                        Repeater {
                            model: [{ name: "No transition", kind: -1 }, { name: "Cross dissolve", kind: 0 },
                                    { name: "Dip to black", kind: 1 }, { name: "Wipe", kind: 2 }]
                            delegate: MenuItem {
                                text: modelData.name
                                checkable: true
                                checked: transitionMenu.kind === modelData.kind
                                onTriggered: session.setTransition(transitionMenu.clipId, modelData.kind, transitionMenu.seconds)
                            }
                        }
                        MenuSeparator {}
                        Repeater {
                            model: [0.5, 1, 2]
                            delegate: MenuItem {
                                text: modelData + " s"
                                checkable: true
                                checked: transitionMenu.seconds === modelData
                                onTriggered: {
                                    transitionMenu.seconds = modelData
                                    if (transitionMenu.kind >= 0)
                                        session.setTransition(transitionMenu.clipId, transitionMenu.kind, modelData)
                                }
                            }
                        }
                    }
                    // Context menus (ui-design §7.3): what applies to the clicked item, through the same
                    // actions as the bars and shortcuts. `at` is where the click fell, in seconds.
                    component AdjustItem: MenuItem {
                        property OmaAction adjustment
                        property string drawerName
                        // The adjusted mark the bar used to show, next to the adjustment's name.
                        readonly property bool adjusted: drawerName === "color" ? !!session.info.colorAdjusted
                            : drawerName === "crop" ? !!session.info.framingAdjusted
                            : drawerName === "volume" ? root.audioAdjusted
                            : drawerName === "effects" ? !!session.info.filtered : false
                        text: adjustment.text + "…" + (adjusted ? "  ●" : "")
                        enabled: adjustment.enabled
                        onTriggered: root.openDrawer(drawerName)
                    }
                    Menu {
                        id: storylineMenu
                        objectName: "storylineMenu"
                        property real at: 0
                        AdjustItem { adjustment: actions.color; drawerName: "color" }
                        AdjustItem { adjustment: actions.crop; drawerName: "crop" }
                        AdjustItem { adjustment: actions.volume; drawerName: "volume" }
                        AdjustItem { adjustment: actions.effects; drawerName: "effects" }
                        AdjustItem { adjustment: actions.info; drawerName: "info" }
                        MenuSeparator {}
                        MenuItem { objectName: "menuSplitHere"; text: "Split here"; onTriggered: { session.seek(storylineMenu.at); session.splitAtPlayhead() } }
                        MenuItem { action: actions.detachAudio }
                        MenuSeparator {}
                        MenuItem { action: actions.remove }
                        MenuItem { action: actions.lift }
                    }
                    Menu {
                        id: soundMenu
                        property real at: 0
                        AdjustItem { adjustment: actions.volume; drawerName: "volume" }
                        AdjustItem { adjustment: actions.info; drawerName: "info" }
                        MenuSeparator {}
                        MenuItem { text: "Split here"; onTriggered: { session.seek(soundMenu.at); session.splitAtPlayhead() } }
                        MenuItem { text: "Delete"; onTriggered: session.deleteSelected(false) }
                    }
                    // Audio lanes below the storyline: music, voiceover, sound effects.
                    Repeater {
                        model: session.audioTracks
                        delegate: Item {
                            id: lane
                            required property var modelData
                            required property int index
                            x: 0
                            y: timelinePanel.laneY(index)
                            width: timelineScroll.contentWidth
                            height: timelinePanel.laneHeight
                            Rectangle { // the lane's band
                                x: timelinePanel.origin
                                width: parent.width - timelinePanel.origin
                                height: parent.height
                                radius: 4
                                color: Qt.rgba(1, 1, 1, 0.025)
                            }
                            Repeater {
                                model: lane.modelData.clips
                                delegate: Rectangle {
                                    id: soundItem
                                    required property var modelData
                                    readonly property bool chosen: modelData.id === session.selectedClip
                                    property real headDrag: 0
                                    property real tailDrag: 0
                                    // Live feedback while the clip is moved; committed on release.
                                    property real moveX: 0
                                    property real moveY: 0
                                    x: timelinePanel.origin + modelData.start * timelinePanel.scale + headDrag + moveX
                                    y: moveY
                                    z: moveX !== 0 || moveY !== 0 ? 2 : 0
                                    width: Math.max(6, modelData.duration * timelinePanel.scale - 2 - headDrag + tailDrag)
                                    height: lane.height
                                    radius: 4
                                    clip: true
                                    color: Qt.tint(colors.lighter_background, Qt.rgba(0.35, 0.8, 0.45, 0.22))
                                    border.width: chosen ? 2 : 0
                                    border.color: root.accent
                                    ClipWaveform {
                                        clip: soundItem.modelData
                                        anchors.fill: parent
                                        anchors.topMargin: 2
                                        anchors.bottomMargin: 2
                                        color: Qt.rgba(0.55, 0.95, 0.6, 0.55)
                                    }
                                    FadeRamps { clip: soundItem.modelData }
                                    Row {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        anchors.leftMargin: 8
                                        spacing: 5
                                        Icon { name: "music"; size: 13; color: root.fg; anchors.verticalCenter: parent.verticalCenter }
                                        Text {
                                            width: parent.width - 30
                                            text: soundItem.modelData.name
                                            color: root.fg
                                            font.pixelSize: 11
                                            elide: Text.ElideRight
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        property point pressAt
                                        property bool moving: false
                                        preventStealing: true
                                        cursorShape: moving ? Qt.ClosedHandCursor : Qt.ArrowCursor
                                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                                        onPressed: (mouse) => {
                                            session.selectClip(soundItem.modelData.id)
                                            const at = soundItem.modelData.start + mouse.x / timelinePanel.scale
                                            if (mouse.button === Qt.RightButton) {
                                                soundMenu.at = at
                                                soundMenu.popup()
                                                return
                                            }
                                            pressAt = mapToItem(timelineScroll.contentItem, mouse.x, mouse.y)
                                            moving = false
                                            session.seek(at)
                                        }
                                        onPositionChanged: (mouse) => {
                                            if (!(pressedButtons & Qt.LeftButton)) return
                                            const p = mapToItem(timelineScroll.contentItem, mouse.x, mouse.y)
                                            if (!moving && Math.abs(p.x - pressAt.x) < 4 && Math.abs(p.y - pressAt.y) < 6) return
                                            moving = true
                                            const m = soundItem.modelData
                                            const start = timelinePanel.snap(m.start + (p.x - pressAt.x) / timelinePanel.scale, m.duration, m.id)
                                            soundItem.moveX = (start - m.start) * timelinePanel.scale
                                            soundItem.moveY = p.y - pressAt.y
                                        }
                                        onReleased: {
                                            if (!moving) return
                                            const lanes = Math.round(soundItem.moveY / (timelinePanel.laneHeight + timelinePanel.laneGap))
                                            const frames = Math.round(soundItem.moveX / timelinePanel.scale * session.frameRate)
                                            soundItem.moveX = 0
                                            soundItem.moveY = 0
                                            timelinePanel.unsnap()
                                            if (moving && (lanes !== 0 || frames !== 0))
                                                session.moveClip(soundItem.modelData.id, lanes, frames)
                                            moving = false
                                        }
                                    }
                                    TrimEdge { owner: soundItem; clip: soundItem.modelData; head: true; anchors.left: parent.left }
                                    TrimEdge { owner: soundItem; clip: soundItem.modelData; head: false; anchors.right: parent.right }
                                }
                            }
                        }
                    }
                    Rectangle { // the edge a drag snapped to
                        visible: timelinePanel.snapLine >= 0
                        x: timelinePanel.origin + timelinePanel.snapLine * timelinePanel.scale - 0.5
                        z: 5
                        width: 1
                        height: timelineScroll.height
                        color: colors.yellow || root.fg
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
                    OmaButton { action: actions.snapping; iconName: "snap"; showLabel: false; selected: root.snapping }
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

    // A library item on its way to the timeline (ui-design §7.3); the library's MouseArea moves
    // it and the timeline's DropArea receives it.
    Rectangle {
        id: mediaDrag
        property int index: -1
        readonly property var item: index >= 0 && index < session.media.length ? session.media[index] : null
        visible: Drag.active
        z: 100
        width: 96
        height: 54
        radius: 4
        color: root.viewerBackground
        border.color: root.accent
        border.width: 2
        opacity: 0.85
        Drag.keys: ["oma/media"]
        Drag.hotSpot.y: height / 2
        Image {
            anchors.fill: parent
            anchors.margins: 2
            fillMode: Image.PreserveAspectFit
            source: mediaDrag.item ? mediaDrag.item.thumbnail : ""
        }
        Icon {
            anchors.centerIn: parent
            visible: !!mediaDrag.item && mediaDrag.item.audioOnly
            name: "music"
            size: 24
            color: root.fg
        }
    }

    // Every action's shortcut, registered in one place.
    Repeater {
        model: [actions.playPause, actions.pause, actions.stop, actions.playForward, actions.playBackward, actions.previousFrame,
                actions.nextFrame, actions.back10, actions.forward10, actions.toStart, actions.toEnd,
                actions.append, actions.insert, actions.overwrite, actions.split, actions.remove,
                actions.lift, actions.detachAudio, actions.addDissolve, actions.undo, actions.redo, actions.importMedia,
                actions.save, actions.openProject, actions.exportMovie, actions.toggleLibrary, actions.fullViewer, actions.leaveFullViewer,
                actions.closeDrawer, actions.zoomIn, actions.zoomOut, actions.zoomFit, actions.snapping, actions.volume, actions.info,
                actions.color, actions.crop, actions.effects, actions.previousClip, actions.nextClip,
                actions.clipMenu, actions.clipMenuKey]
        delegate: Item {
            Shortcut {
                sequence: modelData.keys
                enabled: modelData.enabled && (session.editing || modelData === actions.importMedia
                                                 || modelData === actions.openProject)
                onActivated: modelData.trigger()
            }
        }
    }
}

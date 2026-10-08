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
    font.family: uiFont // Controls and popups inherit it

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
    property string librarySource: "project" // the library's source: "project" or "recordings"
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
    // Runs once the save the discard dialog started has finished (dirty goes false).
    property var afterSave: null
    Connections {
        target: session
        function onProjectChanged() {
            if (root.afterSave && !session.dirty) { const next = root.afterSave; root.afterSave = null; next() }
        }
    }
    function confirmDiscard(what, then) {
        afterSave = null
        if (!sessionHasWork) { then(); return }
        discardDialog.what = what
        discardDialog.then = then
        discardDialog.open()
    }
    onClosing: (close) => {
        if (closeConfirmed || !sessionHasWork) return
        close.accepted = false
        // Discarding on close also drops the autosave; a crash leaves it for recovery.
        confirmDiscard("Close OmaMovie", () => { session.discardAutosave(); root.closeConfirmed = true; root.close() })
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
            Button { objectName: "saveFirst"; text: "Save"; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            Button { text: "Discard changes"; DialogButtonBox.buttonRole: DialogButtonBox.DestructiveRole }
            Button { text: "Cancel"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
        onDiscarded: { close(); if (then) then() }
        // Save, then go on; a new project asks where first. A failed save keeps the work here.
        onAccepted: {
            root.afterSave = then
            if (session.projectPath === "") saveDialog.open()
            else session.saveProject("")
        }
    }

    // Shift+F10 or the Menu key: the context menu of the selection, the cut or the timeline.
    function openClipMenu() { timelinePanel.openClipMenu() }

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
    // shortcuts both go through it. Each action's id is its property name: ActionRegistry
    // (C++) reads them on completion, applies the user's remapped keys and searches them for
    // the command palette.
    QtObject {
        id: actions
        Component.onCompleted: actionRegistry.attach(actions)
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
            onTriggered: session.source.open ? session.seekSource(0) : session.seek(0)
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
        // Final Cut's Connect (Q): above the storyline (sound below it), following the clip there.
        property OmaAction connect: OmaAction {
            text: "Connect at the playhead"; keys: "Q"; enabled: actions.mediaChosen
            onTriggered: { session.connectSelected(); root.libraryOverlay = false }
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
            // Not Ctrl+Shift+S: that is Save As everywhere on the desktop (shortcut audit, 2026-10-06).
            text: "Detach audio"; keys: "Ctrl+Alt+S"
            enabled: actions.editing && !!session.info.canDetach
            onTriggered: session.detachAudio()
        }
        property OmaAction addDissolve: OmaAction {
            text: "Add cross dissolve"; keys: "Ctrl+T"; enabled: actions.editing && session.storylineClips.count > 1
            onTriggered: session.addDissolveAtPlayhead()
        }
        property OmaAction previousClip: OmaAction {
            text: "Select the previous clip"; keys: "Up"; enabled: actions.editing && session.storylineClips.count > 0
            onTriggered: session.selectAdjacentClip(-1)
        }
        property OmaAction nextClip: OmaAction {
            text: "Select the next clip"; keys: "Down"; enabled: actions.editing && session.storylineClips.count > 0
            onTriggered: session.selectAdjacentClip(1)
        }
        // The menu of what is selected; with nothing selected, the cut under the playhead's
        // (transitions) or the timeline's.
        property OmaAction clipMenu: OmaAction {
            text: "Context menu"; keys: "Shift+F10"; enabled: actions.editing && session.clips.length > 0
            onTriggered: root.openClipMenu()
        }
        property OmaAction clipMenuKey: OmaAction {
            text: "Context menu"; keys: "Menu"; enabled: actions.clipMenu.enabled
            onTriggered: root.openClipMenu()
        }
        property OmaAction deselect: OmaAction {
            text: "Deselect"; keys: "Ctrl+Shift+A"; enabled: actions.editing && session.selectedClip > 0
            onTriggered: session.selectClip(0)
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
        property OmaAction saveAs: OmaAction {
            text: "Save As…"; keys: "Ctrl+Shift+S"; enabled: actions.editing && session.media.length > 0
            onTriggered: saveDialog.open()
        }
        property OmaAction openProject: OmaAction {
            text: "Open project…"; keys: "Ctrl+O"
            onTriggered: root.confirmDiscard("Open another project", () => openDialog.open())
        }
        property OmaAction exportMovie: OmaAction {
            text: "Export…"; keys: "Ctrl+E"; enabled: actions.editing && session.clips.length > 0 && session.exportProgress < 0
            onTriggered: exportDialog.open()
        }
        // Shows or hides the library: an overlay in narrow windows, the sidebar in wide ones (the
        // viewer then takes the whole width, centered).
        property OmaAction toggleLibrary: OmaAction {
            text: "Library"; keys: "Ctrl+1"; enabled: actions.editing
            onTriggered: root.compact ? root.libraryOverlay = !root.libraryOverlay : root.libraryHidden = !root.libraryHidden
        }
        // Source viewer (M6 pilot): mark a range of a library item before placing it.
        // Canvas proportion (ADR-0010): keeps the short side; positions follow.
        property OmaAction canvasWide: OmaAction {
            text: "Canvas 16:9 (landscape)"; enabled: actions.editing && session.clips.length > 0
            onTriggered: session.setCanvasAspect(16, 9)
        }
        property OmaAction canvasVertical: OmaAction {
            text: "Canvas 9:16 (vertical)"; enabled: actions.editing && session.clips.length > 0
            onTriggered: session.setCanvasAspect(9, 16)
        }
        property OmaAction canvasSquare: OmaAction {
            text: "Canvas 1:1 (square)"; enabled: actions.editing && session.clips.length > 0
            onTriggered: session.setCanvasAspect(1, 1)
        }
        property OmaAction canvasPortrait: OmaAction {
            text: "Canvas 4:5 (portrait)"; enabled: actions.editing && session.clips.length > 0
            onTriggered: session.setCanvasAspect(4, 5)
        }
        // Captions (ADR-0017).
        property OmaAction addCaption: OmaAction {
            text: "Add caption"; keys: "Ctrl+Alt+C"; enabled: actions.editing && session.clips.length > 0
            onTriggered: { const id = session.addCaption(); if (id > 0) root.editCaption(id) }
        }
        property OmaAction editCaption: OmaAction {
            text: "Edit the caption at the playhead"; keys: "Ctrl+Alt+E"
            enabled: actions.editing && session.captionIdAt(session.position) > 0
            onTriggered: root.editCaption(session.captionIdAt(session.position))
        }
        property OmaAction importCaptions: OmaAction {
            text: "Import captions…"; enabled: actions.editing && session.clips.length > 0
            onTriggered: captionsOpenDialog.open()
        }
        property OmaAction exportCaptions: OmaAction {
            text: "Export captions…"; enabled: actions.editing && session.captions.length > 0
            onTriggered: captionsSaveDialog.open()
        }
        property OmaAction openSource: OmaAction {
            text: "Mark a range in the source…"; keys: "Shift+O"
            enabled: actions.editing && session.selectedMedia >= 0 && !session.source.open
            onTriggered: session.openSource(session.selectedMedia)
        }
        property OmaAction markIn: OmaAction {
            text: "Mark in"; keys: "I"; enabled: actions.editing && !!session.source.open
            onTriggered: session.markIn()
        }
        property OmaAction markOut: OmaAction {
            text: "Mark out"; keys: "O"; enabled: actions.editing && !!session.source.open
            onTriggered: session.markOut()
        }
        property OmaAction clearMarks: OmaAction {
            text: "Clear in and out"; keys: "Alt+X"; enabled: actions.editing && !!session.source.open
            onTriggered: session.clearMarks()
        }
        property OmaAction closeSource: OmaAction {
            text: "Back to the sequence"; keys: "Escape"
            enabled: actions.editing && !!session.source.open && root.drawer === "" && !root.viewerOnly
            onTriggered: session.closeSource()
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
        // Titles (ADR-0015): a title at the playhead above the storyline, and its drawer.
        property OmaAction addTitle: OmaAction {
            text: "Add title"; keys: "Ctrl+Alt+T"; enabled: actions.editing && session.hasMedia
            onTriggered: { session.addTitle(); root.openDrawer("title") }
        }
        property OmaAction title: OmaAction {
            text: "Title"; enabled: actions.editing && !!session.info.isTitle
            onTriggered: root.drawer = root.drawer === "title" ? "" : "title"
        }
        property OmaAction settings: OmaAction {
            text: "Settings"; keys: "Ctrl+,"
            onTriggered: settingsDialog.opened ? settingsDialog.close() : settingsDialog.open()
        }
        property OmaAction commandPalette: OmaAction {
            text: "Command palette"; keys: "Ctrl+K"
            onTriggered: palette.opened ? palette.close() : palette.open()
        }
        property OmaAction info: OmaAction {
            text: "Info"; keys: "Ctrl+Shift+I"; enabled: actions.editing && !!session.info.name; checkable: true
            checked: root.drawer === "info"
            onTriggered: root.drawer = root.drawer === "info" ? "" : "info"
        }
    }

    FileDialog {
        id: captionsOpenDialog
        title: "Import captions"
        nameFilters: ["Captions (*.srt *.vtt)"]
        onAccepted: session.importCaptions(selectedFile)
    }
    FileDialog {
        id: captionsSaveDialog
        title: "Export captions"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "srt"
        nameFilters: ["SubRip (*.srt)", "WebVTT (*.vtt)"]
        onAccepted: session.exportCaptions(selectedFile)
    }
    // The caption editor: its text (line breaks allowed), Save or Delete. One edit each.
    function editCaption(id) {
        const c = session.captions.find(x => x.id === id)
        if (!c) return
        captionEditor.captionId = id
        captionText.text = c.text
        captionEditor.open()
        captionText.forceActiveFocus()
        captionText.selectAll()
    }
    Dialog {
        id: captionEditor
        objectName: "captionEditor"
        property double captionId: 0
        anchors.centerIn: parent
        width: Math.min(460, root.width - 48)
        modal: true
        title: "Caption"
        TextArea {
            id: captionText
            objectName: "captionText"
            width: parent.width
            wrapMode: TextEdit.Wrap
            placeholderText: "What is said"
        }
        footer: DialogButtonBox {
            Button { text: "Save"; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            Button { text: "Delete"; DialogButtonBox.buttonRole: DialogButtonBox.DestructiveRole }
            Button { text: "Cancel"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
        onAccepted: session.setCaptionText(captionId, captionText.text)
        onDiscarded: { session.removeCaption(captionId); close() }
    }
    FileDialog {
        id: exportDialog
        title: "Export movie"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "mp4"
        nameFilters: ["MP4 video (*.mp4)"]
        onAccepted: session.exportMovie(selectedFile)
    }
    // Export progress (M7): the editor stays usable; the export renders the project as it was
    // when it started.
    Rectangle {
        objectName: "exportBar"
        visible: session.exportProgress >= 0 || session.exportedFile !== ""
        z: 20
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 16
        width: 300
        height: exportColumn.implicitHeight + 20
        radius: 8
        color: colors.dark_background
        border.color: root.line
        ColumnLayout {
            id: exportColumn
            anchors.fill: parent
            anchors.margins: 10
            spacing: 6
            UiText {
                Layout.fillWidth: true
                text: session.exportProgress >= 0 ? "Exporting… " + Math.round(session.exportProgress * 100) + "%"
                                                  : "Exported " + session.exportedFile.split("/").pop()
                color: root.fg
                font.pixelSize: 12
                elide: Text.ElideMiddle
            }
            ProgressBar { Layout.fillWidth: true; visible: session.exportProgress >= 0; value: Math.max(0, session.exportProgress) }
            RowLayout {
                Item { Layout.fillWidth: true }
                OmaButton { visible: session.exportProgress >= 0; text: "Cancel"; onClicked: session.cancelExport() }
                OmaButton {
                    visible: session.exportProgress < 0
                    text: "Show file"
                    onClicked: Qt.openUrlExternally("file://" + session.exportedFile.substring(0, session.exportedFile.lastIndexOf("/")))
                }
                OmaButton { visible: session.exportProgress < 0; text: "Close"; onClicked: session.clearExported() }
            }
        }
    }
    FileDialog {
        id: saveDialog
        title: "Save project"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "omamovie"
        nameFilters: ["OmaMovie project (*.omamovie)"]
        onAccepted: session.saveProject(selectedFile)
        onRejected: root.afterSave = null
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
    ProjectsScreen {}

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
                UiText {
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
                MediaPanel { id: library }

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
                            UiText {
                                Layout.fillWidth: true
                                text: ({ color: "Color", crop: "Crop and framing", volume: "Volume", effects: "Effects",
                                         info: "Info", title: "Title" })[root.drawer] + (session.info.name ? " — " + session.info.name : "")
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
                    InfoDrawer {}

                    // Volume drawer (§6): volume, fades and mute for the selected clip; "More" adds the
                    // equalizer, noise reduction and normalize. Values follow the clip; a change is
                    // committed as one command on release.
                    VolumeDrawer { id: volumeDrawer }

                    // Color drawer (§6): exposure, contrast, saturation and temperature of the clip;
                    // "More" adds grading (ADR-0012): lift/gamma/gain wheels, curves and a LUT.
                    ColorDrawer { id: colorDrawer }

                    // Crop drawer (§6): fit mode and edges; "More" adds position, scale and rotation.
                    CropDrawer { id: cropDrawer }

                    // Effects drawer (§6): one filter per clip, chosen from previews of the clip itself.
                    EffectsDrawer { id: effectsDrawer }

                    // Title drawer (ADR-0015): text, size, colour and placement of a title clip.
                    TitleDrawer {}

                    // Viewer (§5): neutral background, untouched by the theme.
                    PreviewPanel {}

                    // Transport (§5): the same actions serve buttons and keyboard shortcuts.
                    TransportControls { id: transport }
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
            TimelineView { id: timelinePanel }
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

    // Files from another launch (single instance, ui-design §3.1): the same rules as a fresh
    // start, in this window, after the unsaved-work guard.
    Connections {
        target: session
        function onOpenRequested(paths) {
            root.raise()
            root.requestActivate()
            if (paths.length > 0) root.confirmDiscard("Open " + paths.length + (paths.length === 1 ? " file" : " files"),
                                                      () => session.openFiles(paths))
        }
    }

    CommandPalette { id: palette }
    SettingsDialog { id: settingsDialog; commandPalette: palette }

    // Every action's shortcut, registered in one place: the registry lists every declared action.
    Repeater {
        model: actionRegistry.actions
        delegate: Item {
            Shortcut {
                sequence: modelData.keys
                // The palette takes the keyboard while open; only Ctrl+K reaches through to close it.
                enabled: modelData.enabled && (!palette.opened || modelData === actions.commandPalette)
                         && (!settingsDialog.opened || modelData === actions.settings)
                         && (session.editing || modelData === actions.importMedia
                             || modelData === actions.openProject || modelData === actions.commandPalette
                             || modelData === actions.settings)
                onActivated: modelData.trigger()
            }
        }
    }
}

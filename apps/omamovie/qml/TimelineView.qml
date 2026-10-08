import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import OmaMovie

// The timeline (ui-design §7): minimap, video lanes, storyline, audio lanes, zoom and the clip
// context menus. Reads the window's ids (root, actions, session) from the document that places it.
Rectangle {
    id: timelinePanel
    visible: !root.viewerOnly
    // Shift+F10 or the Menu key: the selected clip's menu at the clip; with nothing selected,
    // the cut's or the timeline's at the playhead.
    function openClipMenu() {
        for (const c of session.clips) {
            if (c.id !== session.selectedClip) continue
            storylineMenu.at = session.position
            storylineMenu.popup(timelineScroll.contentItem, timelinePanel.origin + c.start * timelinePanel.scale, timelinePanel.storylineY + timelinePanel.storylineHeight)
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
        for (let lane = 0; lane < session.videoTracks.length; ++lane) {
            for (const c of session.videoTracks[lane].clips) {
                if (c.id !== session.selectedClip) continue
                overlayMenu.at = session.position
                overlayMenu.popup(timelineScroll.contentItem, timelinePanel.origin + c.start * timelinePanel.scale,
                                  timelinePanel.videoLaneY(lane) + timelinePanel.laneHeight)
                return
            }
        }
        // Nothing selected: the cut under the playhead (within half a frame), else the timeline.
        const x = timelinePanel.origin + session.position * timelinePanel.scale
        const y = timelinePanel.storylineY + timelinePanel.storylineHeight
        const cuts = session.clips
        for (let i = 1; i < cuts.length; ++i) {
            if (Math.abs(cuts[i].start - session.position) * session.frameRate > 0.5) continue
            transitionMenu.clipId = cuts[i].id
            transitionMenu.kind = cuts[i].transitionSet ? cuts[i].transitionKind : -1
            transitionMenu.popup(timelineScroll.contentItem, x, y)
            return
        }
        emptyMenu.at = session.position
        emptyMenu.popup(timelineScroll.contentItem, x, y)
    }

    // The storyline clip a connected clip follows (ADR-0014): the hovered connected clip's,
    // else the selected one's. That clip gets an outline, so removing it is no surprise.
    property real hoverPrimary: -1
    readonly property real followed: hoverPrimary >= 0 ? hoverPrimary
                                   : session.info.connected ? session.info.primary : -1
    anchors.top: splitter.bottom
    anchors.bottom: parent.bottom
    width: parent.width
    color: colors.darker_background

    readonly property real total: session.duration
    readonly property real scale: root.fitTimeline && total > 0
        ? Math.min(2400, Math.max(4, (timelineScroll.width - 48) / total)) : root.pixelsPerSecond
    readonly property real origin: 24 // left margin of time zero, in content pixels
    // Video lanes on top (titles, cutaways), the storyline, audio lanes below it
    // (ui-design §7.1).
    readonly property real laneHeight: 34
    readonly property real laneGap: 4
    readonly property int lanes: session.audioTracks.length
    readonly property int videoLanes: session.videoTracks.length
    readonly property real storylineY: 4 + videoLanes * (laneHeight + laneGap) + (videoLanes > 0 ? 4 : 0)
    readonly property int captionRows: session.captions.length > 0 ? 1 : 0 // below the audio lanes
    readonly property real storylineHeight:
        Math.min(86, Math.max(44, timelineScroll.height - 12 - (lanes + videoLanes + captionRows) * (laneHeight + laneGap)))
    function laneY(index) { return storylineY + storylineHeight + 8 + index * (laneHeight + laneGap) }
    // Delegates exist only for clips near the view (M6 long-form audit: recreating one
    // per clip after every edit cost 330-660 ms at 500 clips). The range moves in
    // whole view widths, so scrolling rebuilds them once per page, not per pixel.
    readonly property int page: Math.floor(timelineScroll.contentX / Math.max(1, timelineScroll.width))
    readonly property real viewFrom: Math.max(0, ((page - 1) * timelineScroll.width - origin) / scale)
    readonly property real viewTo: ((page + 3) * timelineScroll.width - origin) / scale
    // The storyline model follows the view range (Session::setStorylineView).
    onViewFromChanged: session.setStorylineView(viewFrom, viewTo)
    onViewToChanged: session.setStorylineView(viewFrom, viewTo)
    Component.onCompleted: session.setStorylineView(viewFrom, viewTo)
    function near(list) {
        const from = viewFrom
        const to = viewTo
        return list.filter(c => c.start + c.duration >= from && c.start <= to)
    }
    // Video lane `index` (0 nearest the storyline) sits that many lanes above it.
    function videoLaneY(index) { return storylineY - 4 - (index + 1) * (laneHeight + laneGap) + laneGap }
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
            session.previewTrim(clipId, head, Math.round((snapped - edge) * session.frameRate))
        }
        onReleased: {
            const dx = head ? owner.headDrag : owner.tailDrag
            owner.headDrag = 0
            owner.tailDrag = 0
            timelinePanel.unsnap()
            const frames = Math.round(dx / timelinePanel.scale * session.frameRate)
            if (frames !== 0) session.trimClip(clipId, head, frames)
            else session.clearEditScope()
        }
    }

    // What a pending edit does to this clip (session.editScope, simulated with the real command):
    // an arrow when it will move, a red outline when it will go. Shown before trims and deletes.
    component ScopeMark: Rectangle {
        required property var clip
        readonly property int shift: session.editScope.active && session.editScope.moved[clip.id] ? session.editScope.moved[clip.id] : 0
        readonly property bool going: !!session.editScope.active && session.editScope.removed.indexOf(clip.id) >= 0
        objectName: "scopeMark"
        visible: shift !== 0 || going
        anchors.fill: parent
        radius: 4
        color: going ? Qt.rgba(1, 0.3, 0.3, 0.18) : Qt.rgba(1, 1, 1, 0.10)
        border.color: going ? (colors.red || "#ff6b6b") : root.accent
        border.width: going ? 2 : 1
        z: 5
        Icon {
            visible: parent.shift !== 0
            anchors.centerIn: parent
            name: parent.shift > 0 ? "arrowRight" : "arrowLeft"
            color: root.accent
            size: 16
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

    // A lane clip's connection to the storyline (ADR-0014): end it, or make one at its start.
    component ConnectionItem: MenuItem {
        text: session.info.connected ? "Disconnect from the storyline" : "Connect to the storyline"
        onTriggered: session.info.connected ? session.disconnectSelectedClip() : session.connectSelectedClip()
    }
    // Clip timing (ADR-0013): constant speeds, a two-second freeze at the clicked point, reverse.
    component ClipTimingMenu: Menu {
        property real at: 0
        title: "Speed"
        MenuItem { text: "Normal"; onTriggered: session.setClipSpeed(1, 1) }
        MenuItem { text: "Slow (½×)"; onTriggered: session.setClipSpeed(1, 2) }
        MenuItem { text: "Fast (2×)"; onTriggered: session.setClipSpeed(2, 1) }
        MenuItem { text: "Faster (4×)"; onTriggered: session.setClipSpeed(4, 1) }
        MenuSeparator {}
        MenuItem { objectName: "menuRampUp"; text: "Accelerate (½× → 1½×)"; onTriggered: session.setSpeedRamp(0) }
        MenuItem { text: "Decelerate (1½× → ½×)"; onTriggered: session.setSpeedRamp(1) }
        MenuItem { text: "Burst (1× → 2× → 1×)"; onTriggered: session.setSpeedRamp(2) }
        MenuSeparator {}
        MenuItem { objectName: "menuFreezeFrame"; text: "Freeze frame here"; onTriggered: { session.seek(at); session.freezeFrame(2) } }
        MenuItem { objectName: "menuReverse"; text: "Reverse"; onTriggered: session.reverseClip() }
    }
    // A clip's retiming at a glance: "2×", "Reverse", "Freeze".
    component TimingBadge: Rectangle {
        property string timing: ""
        visible: timing !== ""
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 4
        width: badgeText.implicitWidth + 10
        height: 16
        radius: 8
        color: Qt.rgba(0, 0, 0, 0.55)
        UiText { id: badgeText; anchors.centerIn: parent; text: parent.timing; color: "white"; font.pixelSize: 10 }
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
        // One canvas rather than an item per clip: the minimap shows every clip.
        Canvas {
            id: minimapClips
            anchors.fill: parent
            opacity: 0.85
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                const total = Math.max(0.001, timelinePanel.total)
                const y = height / 2 - 3
                const spans = session.clipSpans
                ctx.fillStyle = colors.blue
                for (let i = 0; i + 1 < spans.length; i += 2) {
                    ctx.fillRect(width * spans[i] / total, y, Math.max(2, width * spans[i + 1] / total - 1), 6)
                }
                // The storyline clip a connection follows, as on the clip items.
                const followed = timelinePanel.followed >= 0 ? session.clipSpan(timelinePanel.followed) : []
                if (followed.length === 2) {
                    ctx.strokeStyle = root.fg
                    ctx.lineWidth = 2
                    ctx.strokeRect(followed[0] * s - left + 1, 1, Math.max(2, followed[1] * s - 2), height - 2)
                }
                const sel = session.selectedSpan
                if (sel.length === 2) {
                    ctx.fillStyle = root.accent
                    ctx.fillRect(width * sel[0] / total, y, Math.max(2, width * sel[1] / total - 1), 6)
                }
            }
            Connections {
                target: session
                function onSequenceChanged() { minimapClips.requestPaint() }
                function onSelectionChanged() { minimapClips.requestPaint() }
            }
            onWidthChanged: requestPaint()
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
        // A right click on the empty timeline: what applies to the timeline itself.
        MouseArea {
            width: timelineScroll.contentWidth
            height: timelineScroll.height
            z: -1
            acceptedButtons: Qt.RightButton
            onPressed: (mouse) => {
                emptyMenu.at = timelineScroll.secondsAt(mouse.x)
                emptyMenu.popup()
            }
        }
        Menu {
            id: emptyMenu
            objectName: "emptyTimelineMenu"
            property real at: 0
            MenuItem { text: "Move the playhead here"; onTriggered: session.seek(emptyMenu.at) }
            MenuItem { action: actions.importMedia }
            MenuItem { action: actions.addCaption }
            Menu {
                title: "Canvas (" + session.canvasWidth + " × " + session.canvasHeight + ")"
                MenuItem { action: actions.canvasWide }
                MenuItem { action: actions.canvasVertical }
                MenuItem { action: actions.canvasSquare }
                MenuItem { action: actions.canvasPortrait }
            }
            MenuItem { action: actions.importCaptions }
            MenuSeparator {}
            MenuItem { action: actions.zoomFit }
            MenuItem { action: actions.snapping }
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
            // The lane under the pointer: audio lanes 0.. top first, -1 the storyline, and
            // -2 - n video lane n above it (a new one past the top).
            readonly property int lane: drag.y < timelinePanel.storylineY - 4
                ? -2 - Math.max(0, Math.floor((timelinePanel.storylineY - 4 - drag.y) / (timelinePanel.laneHeight + timelinePanel.laneGap)))
                : drag.y < timelinePanel.laneY(0) - timelinePanel.laneGap ? -1
                : Math.floor((drag.y - timelinePanel.laneY(0)) / (timelinePanel.laneHeight + timelinePanel.laneGap))
            onPositionChanged: if (item && !sound) { dropMarker.moving = 0; dropMarker.cut = session.storylineCut(seconds, 0) }
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
        // Captions (ADR-0017): one row under the audio lanes, in sequence time.
        Repeater {
            model: timelinePanel.near(session.captions)
            delegate: Rectangle {
                id: captionItem
                required property var modelData
                objectName: "captionItem"
                x: timelinePanel.origin + modelData.start * timelinePanel.scale
                y: timelinePanel.laneY(timelinePanel.lanes)
                width: Math.max(4, modelData.duration * timelinePanel.scale - 2)
                height: timelinePanel.laneHeight - 8
                radius: 4
                color: Qt.tint(colors.lighter_background, Qt.rgba(1, 0.85, 0.3, 0.22))
                border.width: 1
                border.color: Qt.rgba(1, 0.85, 0.3, 0.6)
                clip: true
                UiText {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    verticalAlignment: Text.AlignVCenter
                    text: captionItem.modelData.text.replace(/\n/g, " · ")
                    color: root.fg
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: session.seek(captionItem.modelData.start)
                    onDoubleClicked: root.editCaption(captionItem.modelData.id)
                }
            }
        }
        Repeater {
            model: session.storylineClips
            delegate: Rectangle {
                id: clipItem
                objectName: "storylineClip"
                readonly property real clipId: modelData.id
                readonly property bool chosen: modelData.id === session.selectedClip
                // Live feedback while an edge is dragged; committed as one trim on release.
                property real headDrag: 0
                property real tailDrag: 0
                property real moveX: 0 // live feedback while the clip is dragged to a new place
                x: timelinePanel.origin + modelData.start * timelinePanel.scale + moveX
                // Only clips near the view go to the GPU (M6 long-form audit), and only
                // clips wide enough to show them carry a thumbnail and a waveform.
                visible: modelData.start + modelData.duration >= timelinePanel.viewFrom &&
                         modelData.start <= timelinePanel.viewTo
                readonly property bool detailed: width >= 24
                z: moveX !== 0 ? 2 : 0
                opacity: moveX !== 0 ? 0.8 : 1
                y: timelinePanel.storylineY
                width: Math.max(6, modelData.duration * timelinePanel.scale - 2 - headDrag + tailDrag)
                height: timelinePanel.storylineHeight
                radius: 5
                clip: true
                color: Qt.tint(colors.lighter_background, Qt.rgba(0.31, 0.55, 1, 0.18))
                readonly property bool followed: modelData.id === timelinePanel.followed
                border.width: chosen || followed ? 2 : 0
                border.color: chosen ? root.accent : root.fg
                Image {
                    visible: clipItem.detailed
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
                    visible: modelData.hasAudio && clipItem.detailed
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 18
                    color: Qt.rgba(0.55, 0.75, 1, 0.45)
                }
                FadeRamps { clip: modelData; visible: clipItem.detailed }
                ScopeMark { clip: modelData }
                TimingBadge { timing: modelData.timing; visible: timing !== "" && clipItem.detailed }
                Rectangle { // adjusted: color, framing, effects or sound differ from the plain clip
                    visible: modelData.adjusted && clipItem.detailed
                    x: 5; y: 5; width: 7; height: 7; radius: 3.5
                    color: root.accent
                }
                // Before a drop: this clip will move (and which way).
                Rectangle {
                    readonly property int shift: dropMarker.shiftOf(modelData)
                    objectName: "willMove"
                    visible: shift !== 0
                    anchors.fill: parent
                    radius: 5
                    color: Qt.rgba(1, 1, 1, 0.10)
                    border.color: root.accent
                    border.width: 1
                    Icon {
                        anchors.centerIn: parent
                        name: parent.shift > 0 ? "arrowRight" : "arrowLeft"
                        color: root.accent
                        size: 16
                    }
                }
                UiText {
                    visible: clipItem.detailed
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
                        dropMarker.moving = modelData.id
                        dropMarker.movingStart = modelData.start
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
                TrimEdge { owner: clipItem; clip: modelData; head: true; anchors.left: parent.left; visible: clipItem.detailed }
                TrimEdge { owner: clipItem; clip: modelData; head: false; anchors.right: parent.right; visible: clipItem.detailed }
            }
        }
        // Too many clips in view for one item each: the storyline as one strip, drawn
        // over the visible part only; a click selects the clip under it.
        Canvas {
            id: compactStoryline
            objectName: "compactStoryline"
            visible: session.storylineCompact
            x: timelineScroll.contentX
            y: timelinePanel.storylineY
            width: timelineScroll.width
            height: timelinePanel.storylineHeight
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                if (!visible) return
                const spans = session.clipSpans
                const s = timelinePanel.scale
                const left = x - timelinePanel.origin
                ctx.fillStyle = Qt.tint(colors.lighter_background, Qt.rgba(0.31, 0.55, 1, 0.18))
                for (let i = 0; i + 1 < spans.length; i += 2) {
                    const a = spans[i] * s - left
                    const w = Math.max(1, spans[i + 1] * s - 1)
                    if (a + w < 0 || a > width) continue
                    ctx.fillRect(a, 0, w, height)
                }
                const sel = session.selectedSpan
                if (sel.length === 2) {
                    ctx.strokeStyle = root.accent
                    ctx.lineWidth = 2
                    ctx.strokeRect(sel[0] * s - left + 1, 1, Math.max(2, sel[1] * s - 2), height - 2)
                }
            }
            onXChanged: requestPaint()
            onWidthChanged: requestPaint()
            onVisibleChanged: requestPaint()
            Connections {
                target: session
                function onSequenceChanged() { compactStoryline.requestPaint() }
                function onSelectionChanged() { compactStoryline.requestPaint() }
            }
            Connections {
                target: timelinePanel
                function onScaleChanged() { compactStoryline.requestPaint() }
                function onFollowedChanged() { compactStoryline.requestPaint() }
            }
            MouseArea {
                anchors.fill: parent
                onPressed: (mouse) => session.selectClipAt(timelineScroll.secondsAt(compactStoryline.x + mouse.x))
            }
        }
        // The cut where a dragged clip or library item will land on the storyline.
        Rectangle {
            id: dropMarker
            property real cut: 0
            property bool shown: false
            // The storyline clip being dragged (0 for a library item) and where it
            // starts, so the clips that will move can say so before the drop.
            property real moving: 0
            property real movingStart: 0
            // -1 a clip will move left, 1 right, 0 it stays.
            function shiftOf(clip) {
                if (!shown || clip.id === moving) return 0
                if (moving === 0) return clip.start >= cut - 1e-9 ? 1 : 0
                if (cut > movingStart) return clip.start > movingStart && clip.start < cut - 1e-9 ? -1 : 0
                return clip.start >= cut - 1e-9 && clip.start < movingStart ? 1 : 0
            }
            visible: shown
            x: timelinePanel.origin + cut * timelinePanel.scale - width / 2
            y: timelinePanel.storylineY - 4
            z: 4
            width: 3
            height: timelinePanel.storylineHeight + 8
            radius: 1.5
            color: root.accent
        }
        // Cuts between touching storyline clips (ui-design §7.2): the span a transition
        // covers, and a ⋈ marker that edits it.
        Repeater {
            model: session.storylineClips
            delegate: Item {
                id: junction
                required property var modelData
                // Near the view and with room for the marker (M6 long-form audit).
                visible: modelData.joined && modelData.duration * timelinePanel.scale >= 24 &&
                         modelData.start >= timelinePanel.viewFrom && modelData.start <= timelinePanel.viewTo
                readonly property real cutX: timelinePanel.origin + modelData.start * timelinePanel.scale
                Rectangle { // the transition's span
                    visible: junction.modelData.transitionKind >= 0
                    x: junction.cutX - width / 2
                    y: timelinePanel.storylineY
                    width: junction.modelData.transitionSpan * timelinePanel.scale
                    height: timelinePanel.storylineHeight
                    color: Qt.rgba(1, 1, 1, 0.12)
                    border.color: root.accent
                    border.width: 1
                    radius: 3
                }
                Rectangle {
                    x: junction.cutX - width / 2
                    y: timelinePanel.storylineY + timelinePanel.storylineHeight / 2 - height / 2
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
            objectName: "transitionMenu"
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
            ClipTimingMenu { at: storylineMenu.at }
            MenuSeparator {}
            MenuItem { objectName: "menuRemove"; action: actions.remove; onHighlightedChanged: highlighted ? session.previewDelete(true) : session.clearEditScope() }
            MenuItem { action: actions.lift; onHighlightedChanged: highlighted ? session.previewDelete(false) : session.clearEditScope() }
        }
        // Video lane clips: picture adjustments, not the sound ones.
        Menu {
            id: overlayMenu
            objectName: "overlayMenu"
            property real at: 0
            AdjustItem { adjustment: actions.title; drawerName: "title"; visible: !!session.info.isTitle; height: visible ? implicitHeight : 0 }
            AdjustItem { adjustment: actions.color; drawerName: "color" }
            AdjustItem { adjustment: actions.crop; drawerName: "crop" }
            AdjustItem { adjustment: actions.effects; drawerName: "effects" }
            AdjustItem { adjustment: actions.info; drawerName: "info" }
            MenuSeparator {}
            MenuItem { text: "Split here"; onTriggered: { session.seek(overlayMenu.at); session.splitAtPlayhead() } }
            ClipTimingMenu { at: overlayMenu.at }
            ConnectionItem {}
            MenuSeparator {}
            MenuItem { text: "Delete"; onTriggered: session.deleteSelected(false); onHighlightedChanged: highlighted ? session.previewDelete(false) : session.clearEditScope() }
        }
        Menu {
            id: soundMenu
            objectName: "soundMenu"
            property real at: 0
            AdjustItem { adjustment: actions.volume; drawerName: "volume" }
            AdjustItem { adjustment: actions.info; drawerName: "info" }
            MenuSeparator {}
            MenuItem { text: "Split here"; onTriggered: { session.seek(soundMenu.at); session.splitAtPlayhead() } }
            ClipTimingMenu { at: soundMenu.at }
            ConnectionItem {}
            MenuItem { text: "Delete"; onTriggered: session.deleteSelected(false); onHighlightedChanged: highlighted ? session.previewDelete(false) : session.clearEditScope() }
        }
        // Video lanes above the storyline. A connected clip (ADR-0014) shows a stem down
        // to the storyline clip it follows.
        Repeater {
            model: session.videoTracks
            delegate: Item {
                id: videoLane
                required property var modelData
                required property int index
                x: 0
                y: timelinePanel.videoLaneY(index)
                width: timelineScroll.contentWidth
                height: timelinePanel.laneHeight
                Rectangle {
                    x: timelinePanel.origin
                    width: parent.width - timelinePanel.origin
                    height: parent.height
                    radius: 4
                    color: Qt.rgba(1, 1, 1, 0.025)
                }
                Repeater {
                    model: timelinePanel.near(videoLane.modelData.clips)
                    delegate: Rectangle {
                        id: overlayItem
                        required property var modelData
                        objectName: "overlayClip"
                        readonly property bool chosen: modelData.id === session.selectedClip
                        property real headDrag: 0
                        property real tailDrag: 0
                        property real moveX: 0
                        property real moveY: 0
                        x: timelinePanel.origin + modelData.start * timelinePanel.scale + headDrag + moveX
                        y: moveY
                        z: moveX !== 0 || moveY !== 0 ? 2 : 0
                        width: Math.max(6, modelData.duration * timelinePanel.scale - 2 - headDrag + tailDrag)
                        height: videoLane.height
                        radius: 4
                        clip: true
                        color: Qt.tint(colors.lighter_background, Qt.rgba(0.65, 0.45, 1, 0.22))
                        border.width: chosen ? 2 : 0
                        border.color: root.accent
                        HoverHandler {
                            onHoveredChanged: {
                                const m = parent.modelData
                                if (hovered && m.connected) timelinePanel.hoverPrimary = m.primary
                                else if (!hovered && timelinePanel.hoverPrimary === m.primary) timelinePanel.hoverPrimary = -1
                            }
                        }
                        ScopeMark { clip: overlayItem.modelData }
                        Image {
                            anchors.fill: parent
                            anchors.margins: 2
                            source: overlayItem.modelData.thumbnail
                            fillMode: Image.TileHorizontally
                            asynchronous: true
                            sourceSize.height: height
                            opacity: 0.6
                        }
                        TimingBadge { timing: overlayItem.modelData.timing }
                        UiText {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 8
                            text: overlayItem.modelData.name
                            color: root.fg
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                        MouseArea {
                            anchors.fill: parent
                            property point pressAt
                            property bool moving: false
                            preventStealing: true
                            cursorShape: moving ? Qt.ClosedHandCursor : Qt.ArrowCursor
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onPressed: (mouse) => {
                                session.selectClip(overlayItem.modelData.id)
                                const at = overlayItem.modelData.start + mouse.x / timelinePanel.scale
                                if (mouse.button === Qt.RightButton) {
                                    overlayMenu.at = at
                                    overlayMenu.popup()
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
                                const m = overlayItem.modelData
                                const start = timelinePanel.snap(m.start + (p.x - pressAt.x) / timelinePanel.scale, m.duration, m.id)
                                overlayItem.moveX = (start - m.start) * timelinePanel.scale
                                overlayItem.moveY = p.y - pressAt.y
                            }
                            onReleased: {
                                if (!moving) return
                                // Up is the next lane away from the storyline.
                                const lanes = -Math.round(overlayItem.moveY / (timelinePanel.laneHeight + timelinePanel.laneGap))
                                const frames = Math.round(overlayItem.moveX / timelinePanel.scale * session.frameRate)
                                overlayItem.moveX = 0
                                overlayItem.moveY = 0
                                timelinePanel.unsnap()
                                if (lanes !== 0 || frames !== 0) session.moveClip(overlayItem.modelData.id, lanes, frames)
                                moving = false
                            }
                        }
                        TrimEdge { owner: overlayItem; clip: overlayItem.modelData; head: true; anchors.left: parent.left }
                        TrimEdge { owner: overlayItem; clip: overlayItem.modelData; head: false; anchors.right: parent.right }
                    }
                }
                // Connection stems: from each connected clip's start down to the storyline.
                Repeater {
                    model: timelinePanel.near(videoLane.modelData.clips)
                    delegate: Rectangle {
                        required property var modelData
                        visible: modelData.connected
                        x: timelinePanel.origin + modelData.start * timelinePanel.scale
                        y: videoLane.height
                        width: 2
                        height: timelinePanel.storylineY - (videoLane.y + videoLane.height)
                        color: root.accent
                        opacity: 0.8
                    }
                }
            }
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
                    model: timelinePanel.near(lane.modelData.clips)
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
                        HoverHandler {
                            onHoveredChanged: {
                                const m = parent.modelData
                                if (hovered && m.connected) timelinePanel.hoverPrimary = m.primary
                                else if (!hovered && timelinePanel.hoverPrimary === m.primary) timelinePanel.hoverPrimary = -1
                            }
                        }
                        ClipWaveform {
                            clip: soundItem.modelData
                            anchors.fill: parent
                            anchors.topMargin: 2
                            anchors.bottomMargin: 2
                            color: Qt.rgba(0.55, 0.95, 0.6, 0.55)
                        }
                        FadeRamps { clip: soundItem.modelData }
                        ScopeMark { clip: soundItem.modelData }
                        Row {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 8
                            spacing: 5
                            Icon { name: "music"; size: 13; color: root.fg; anchors.verticalCenter: parent.verticalCenter }
                            UiText {
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
        UiText {
            visible: session.storylineClips.count === 0
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

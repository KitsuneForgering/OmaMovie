import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import OmaMovie

// The timeline (ui-design §7): minimap, video lanes, storyline, audio lanes, zoom and the clip
// context menus. Reads the window's ids (root, actions, session) from the document that places it.
Rectangle {
    id: timelinePanel
    objectName: "timelinePanel"
    visible: !root.viewerOnly
    // Shift+F10 or the Menu key: the selected clip's menu at the clip; with nothing selected,
    // the cut's or the timeline's at the playhead.
    function openClipMenu() {
        const target = session.clipMenuTarget()
        const at = timelinePanel.origin + (target.start ?? session.position) * timelinePanel.scale
        const host = timelineScroll.contentItem
        const storylineBottom = timelinePanel.storylineY + timelinePanel.storylineHeight
        if (target.kind === "storyline") {
            storylineMenu.at = session.position
            storylineMenu.popup(host, at, storylineBottom)
        } else if (target.kind === "audio") {
            soundMenu.at = session.position
            soundMenu.popup(host, at, timelinePanel.laneY(target.lane) + timelinePanel.laneHeight)
        } else if (target.kind === "video") {
            const menu = target.title ? titleMenu : overlayMenu
            menu.at = session.position
            menu.popup(host, at, timelinePanel.videoLaneY(target.lane) + timelinePanel.laneHeight)
        } else if (target.kind === "cut") {
            transitionMenu.clipId = target.clipId
            transitionMenu.kind = target.transitionKind
            transitionMenu.popup(host, at, storylineBottom)
        } else {
            emptyMenu.at = session.position
            emptyMenu.popup(host, at, storylineBottom)
        }
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
    readonly property int lanes: session.audioLaneCount
    readonly property int videoLanes: session.videoLaneCount
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
    function syncStorylineView() { session.setStorylineView(viewFrom, viewTo) }
    onViewFromChanged: Qt.callLater(syncStorylineView)
    onViewToChanged: Qt.callLater(syncStorylineView)
    Component.onCompleted: syncStorylineView()
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
    // magnetic (ripple trim, ui-design §7.3); lanes are not. Alt+drag rolls the cut instead:
    // the neighbour on that side gives or takes the frames and nothing else moves.
    component TrimEdge: MouseArea {
        required property Item owner
        required property var clip
        readonly property double clipId: clip.id
        property bool head
        property bool rolling: false
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
        onPressed: (mouse) => {
            pressX = mapToItem(timelineScroll.contentItem, mouse.x, 0).x
            rolling = (mouse.modifiers & Qt.AltModifier) !== 0
        }
        onPositionChanged: (mouse) => {
            if (!pressed) return
            const edge = head ? clip.start : clip.start + clip.duration
            const dx = mapToItem(timelineScroll.contentItem, mouse.x, 0).x - pressX
            const snapped = timelinePanel.snap(edge + dx / timelinePanel.scale, 0, clipId)
            if (head) owner.headDrag = (snapped - edge) * timelinePanel.scale
            else owner.tailDrag = (snapped - edge) * timelinePanel.scale
            const frames = Math.round((snapped - edge) * session.frameRate)
            if (rolling) session.previewRoll(clipId, head, frames)
            else session.previewTrim(clipId, head, frames)
        }
        onReleased: {
            const dx = head ? owner.headDrag : owner.tailDrag
            owner.headDrag = 0
            owner.tailDrag = 0
            timelinePanel.unsnap()
            const frames = Math.round(dx / timelinePanel.scale * session.frameRate)
            if (frames === 0) session.clearEditScope()
            else if (rolling) session.rollCut(clipId, head, frames)
            else session.trimClip(clipId, head, frames)
        }
    }

    // One clip's pointer gestures, shared by the storyline and the lanes: a click selects it and
    // moves the playhead there, a right click asks for its menu, Alt+drag slips its content (the
    // picture follows the pointer) and Alt+Shift+drag slides it between its neighbours, with the
    // scope preview while dragging (Session::slipClip). A plain drag belongs to the user of the
    // component: `dragged` gives the pointer's offset from the press in timeline content pixels;
    // `dropped` gives the clip's last drag offset (owner.moveX/moveY) and the pointer's x, after
    // the visuals were reset, so the edit may rebuild this delegate.
    component ClipDrag: MouseArea {
        required property Item owner // the clip item: moveX (and moveY on lanes)
        required property var clip // its model entry: id, start, duration
        property real slack: 6 // horizontal pixels before a drag starts
        property real verticalSlack: Infinity // lanes also start on vertical movement
        signal menu(real at)
        signal dragged(real dx, real dy, real x)
        signal dropped(real moveX, real moveY, real x)
        signal settled()
        property point pressAt
        property bool moving: false
        property int shift: 0 // Alt: slip, Alt+Shift: slide
        property int shiftFrames: 0
        anchors.fill: parent
        preventStealing: true
        cursorShape: moving ? Qt.ClosedHandCursor : Qt.ArrowCursor
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: (mouse) => {
            session.selectClip(clip.id)
            const at = clip.start + mouse.x / timelinePanel.scale
            if (mouse.button === Qt.RightButton) {
                menu(at)
                return
            }
            pressAt = mapToItem(timelineScroll.contentItem, mouse.x, mouse.y)
            moving = false
            shift = !(mouse.modifiers & Qt.AltModifier) ? 0 : (mouse.modifiers & Qt.ShiftModifier) ? 2 : 1
            shiftFrames = 0
            session.seek(at)
        }
        onPositionChanged: (mouse) => {
            if (!(pressedButtons & Qt.LeftButton)) return
            const p = mapToItem(timelineScroll.contentItem, mouse.x, mouse.y)
            if (!moving && Math.abs(p.x - pressAt.x) < slack && Math.abs(p.y - pressAt.y) < verticalSlack) return
            moving = true
            if (shift !== 0) {
                const frames = Math.round((p.x - pressAt.x) / timelinePanel.scale * session.frameRate)
                shiftFrames = shift === 1 ? -frames : frames
                if (shift === 2) owner.moveX = frames / session.frameRate * timelinePanel.scale
                session.previewSlip(clip.id, shift === 2, shiftFrames)
                return
            }
            dragged(p.x - pressAt.x, p.y - pressAt.y, p.x)
        }
        function settle() {
            moving = false
            owner.moveX = 0
            settled()
        }
        onReleased: (mouse) => {
            const x = mapToItem(timelineScroll.contentItem, mouse.x, 0).x
            const id = clip.id
            const wasMoving = moving
            const slipping = shift
            const frames = shiftFrames
            const moveX = owner.moveX
            const moveY = owner.moveY || 0
            settle() // before the edit, which rebuilds this delegate
            if (slipping !== 0 && frames !== 0) session.slipClip(id, slipping === 2, frames)
            else if (slipping !== 0) session.clearEditScope()
            else if (wasMoving) dropped(moveX, moveY, x)
        }
        onCanceled: { if (shift !== 0) session.clearEditScope(); settle() }
    }

    // What a pending edit does to this clip (session.editScope, simulated with the real command):
    // an arrow when it will move, a red outline when it will go, a yellow one when part of it
    // will be replaced. Shown before trims, deletes and placements.
    component ScopeMark: Rectangle {
        required property var clip
        readonly property int shift: session.editScope.active && session.editScope.moved[clip.id] ? session.editScope.moved[clip.id] : 0
        readonly property bool going: !!session.editScope.active && session.editScope.removed.indexOf(clip.id) >= 0
        readonly property bool cut: !!session.editScope.active && session.editScope.cut.indexOf(clip.id) >= 0
        objectName: "scopeMark"
        visible: shift !== 0 || going || cut
        anchors.fill: parent
        radius: 4
        color: going ? Qt.rgba(1, 0.3, 0.3, 0.18) : Qt.rgba(1, 1, 1, 0.10)
        border.color: going ? (colors.red || "#ff6b6b") : cut ? (colors.yellow || root.accent) : root.accent
        border.width: going || cut ? 2 : 1
        z: 5
        Icon {
            visible: parent.shift !== 0
            anchors.centerIn: parent
            name: parent.shift > 0 ? "arrowRight" : "arrowLeft"
            color: root.accent
            size: 16
        }
    }

    // Keyframes (M8): a small diamond where each key falls in the clip (fractions of its length).
    component KeyMarks: Repeater {
        required property var clip
        model: clip.keys || []
        delegate: Rectangle {
            required property real modelData
            objectName: "keyMark"
            x: modelData * parent.width - 3
            y: parent.height - 24
            width: 6; height: 6
            rotation: 45
            color: root.fg
            border.color: colors.dark_background
            border.width: 1
            z: 4
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
        width: Math.min(320, root.width - 24)
        MenuItem { text: "Normal"; onTriggered: session.setClipSpeed(1, 1) }
        MenuItem { text: "Slow (½×)"; onTriggered: session.setClipSpeed(1, 2) }
        MenuItem { action: actions.speedDouble }
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
        UiText { id: badgeText; anchors.centerIn: parent; text: parent.timing; color: "white"; font.pixelSize: Math.round(10 * appSettings.textScale) }
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
                    ctx.strokeRect(width * followed[0] / total + 1, 1,
                                   Math.max(2, width * followed[1] / total - 2), height - 2)
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
            Connections {
                target: timelinePanel
                function onFollowedChanged() { minimapClips.requestPaint() }
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
        // 24 px to aim at (WCAG 2.5.8) while the strip itself stays 14 px.
        MouseArea {
            anchors.fill: parent
            anchors.topMargin: -5
            anchors.bottomMargin: -5
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
            MenuItem { action: actions.addTitle }
            Menu {
                title: "Canvas (" + session.canvasWidth + " × " + session.canvasHeight + ")"
                MenuItem { action: actions.canvasWide }
                MenuItem { action: actions.canvasVertical }
                MenuItem { action: actions.canvasSquare }
                MenuItem { action: actions.canvasPortrait }
                MenuItem { action: actions.canvasCustom }
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
                    font.pixelSize: Math.round(11 * appSettings.textScale)
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
                KeyMarks { clip: modelData; visible: clipItem.detailed }
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
                    font.pixelSize: Math.round(11 * appSettings.textScale)
                    elide: Text.ElideRight
                }
                // A drag moves the clip to another cut (magnetic); the rest is ClipDrag's.
                ClipDrag {
                    owner: clipItem
                    clip: modelData
                    onMenu: (at) => {
                        storylineMenu.at = at
                        storylineMenu.popup()
                    }
                    onDragged: (dx, dy, x) => {
                        clipItem.moveX = dx
                        dropMarker.moving = modelData.id
                        dropMarker.movingStart = modelData.start
                        dropMarker.cut = session.storylineCut(timelineScroll.secondsAt(x), modelData.id)
                        dropMarker.shown = true
                    }
                    onSettled: dropMarker.shown = false
                    onDropped: (moveX, moveY, x) => session.reorderClip(modelData.id, timelineScroll.secondsAt(x))
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
            property bool applicable: adjustment.enabled
            // The adjusted mark the bar used to show, next to the adjustment's name.
            readonly property bool adjusted: drawerName === "color" ? !!session.info.colorAdjusted
                : drawerName === "crop" ? !!session.info.framingAdjusted
                : drawerName === "volume" ? root.audioAdjusted
                : drawerName === "effects" ? !!session.info.filtered : false
            text: adjustment.text + "…" + (adjusted ? "  ●" : "")
            visible: applicable
            height: visible ? implicitHeight : 0
            enabled: applicable
            onTriggered: adjustment.trigger()
        }
        // A shallow context tree: adjustment families have one level, while editing commands
        // stay in the root. Both keyboard and pointer paths invoke the registered actions.
        component PictureMenu: Menu {
            title: "Picture"
            width: Math.min(280, root.width - 24)
            AdjustItem { objectName: "pictureCropItem"; adjustment: actions.crop; drawerName: "crop"; text: "Framing and transform…" + (adjusted ? "  ●" : "") }
            AdjustItem { objectName: "pictureColorItem"; adjustment: actions.color; drawerName: "color"; text: "Color and grading…" + (adjusted ? "  ●" : "") }
            AdjustItem { objectName: "pictureEffectsItem"; adjustment: actions.effects; drawerName: "effects"; text: "Stylize and effects…" + (adjusted ? "  ●" : "") }
        }
        component SoundMenu: Menu {
            title: "Sound"
            width: Math.min(320, root.width - 24)
            AdjustItem { objectName: "soundLevelsItem"; adjustment: actions.volume; drawerName: "volume"; text: "Levels and fades…" + (adjusted ? "  ●" : "") }
            MenuItem { objectName: "soundMuteItem"; action: actions.toggleMute }
            MenuSeparator {}
            MenuItem { objectName: "soundEqItem"; action: actions.equalizer; text: actions.equalizer.text + "…" }
            MenuItem { objectName: "soundNoiseItem"; action: actions.noiseReduction; text: actions.noiseReduction.text + "…" }
            MenuItem { action: actions.normalizeAudio }
        }
        component PreciseEditMenu: Menu {
            title: "Precise edit"
            width: Math.min(330, root.width - 24)
            MenuItem { action: actions.rollEarlier }
            MenuItem { action: actions.rollLater }
            MenuSeparator {}
            MenuItem { action: actions.slipEarlier }
            MenuItem { action: actions.slipLater }
            MenuSeparator {}
            MenuItem { action: actions.slideEarlier }
            MenuItem { action: actions.slideLater }
        }
        Menu {
            id: storylineMenu
            objectName: "storylineMenu"
            width: Math.min(280, root.width - 24)
            property real at: 0
            PictureMenu { objectName: "storylinePictureMenu" }
            Instantiator {
                model: session.info.hasAudio ? 1 : 0
                delegate: SoundMenu { objectName: "storylineSoundMenu" }
                onObjectAdded: (index, object) => storylineMenu.insertMenu(1, object)
                onObjectRemoved: (index, object) => storylineMenu.removeMenu(object)
            }
            AdjustItem { adjustment: actions.info; drawerName: "info" }
            MenuSeparator {}
            MenuItem { objectName: "menuSplitHere"; text: "Split here"; onTriggered: { session.seek(storylineMenu.at); actions.split.trigger() } }
            PreciseEditMenu {}
            MenuItem { action: actions.detachAudio; visible: enabled; height: visible ? implicitHeight : 0 }
            ClipTimingMenu { at: storylineMenu.at }
            MenuSeparator {}
            MenuItem { objectName: "menuRemove"; action: actions.remove; onHighlightedChanged: highlighted ? session.previewDelete(true) : session.clearEditScope() }
            MenuItem { action: actions.lift; onHighlightedChanged: highlighted ? session.previewDelete(false) : session.clearEditScope() }
        }
        // Video lane clips expose sound only when their media actually has audio.
        Menu {
            id: overlayMenu
            objectName: "overlayMenu"
            width: Math.min(280, root.width - 24)
            property real at: 0
            PictureMenu { objectName: "overlayPictureMenu" }
            Instantiator {
                model: session.info.hasAudio ? 1 : 0
                delegate: SoundMenu { objectName: "overlaySoundMenu" }
                onObjectAdded: (index, object) => overlayMenu.insertMenu(1, object)
                onObjectRemoved: (index, object) => overlayMenu.removeMenu(object)
            }
            AdjustItem { adjustment: actions.info; drawerName: "info" }
            Menu {
                title: "Layout"
                MenuItem { objectName: "layoutPipItem"; action: actions.layoutPip }
                MenuItem { action: actions.layoutSplit }
            }
            MenuSeparator {}
            MenuItem { text: "Split here"; onTriggered: { session.seek(overlayMenu.at); actions.split.trigger() } }
            ClipTimingMenu { at: overlayMenu.at }
            ConnectionItem {}
            MenuSeparator {}
            MenuItem { text: "Delete"; onTriggered: session.deleteSelected(false); onHighlightedChanged: highlighted ? session.previewDelete(false) : session.clearEditScope() }
        }
        Menu {
            id: titleMenu
            objectName: "titleMenu"
            width: Math.min(280, root.width - 24)
            property real at: 0
            AdjustItem { objectName: "overlayTitleItem"; adjustment: actions.title; drawerName: "title" }
            AdjustItem { adjustment: actions.info; drawerName: "info" }
            MenuSeparator {}
            MenuItem { text: "Split here"; onTriggered: { session.seek(titleMenu.at); actions.split.trigger() } }
            ConnectionItem {}
            MenuItem { text: "Delete"; onTriggered: session.deleteSelected(false); onHighlightedChanged: highlighted ? session.previewDelete(false) : session.clearEditScope() }
        }
        Menu {
            id: soundMenu
            objectName: "soundMenu"
            width: Math.min(280, root.width - 24)
            property real at: 0
            SoundMenu { objectName: "laneSoundMenu" }
            AdjustItem { adjustment: actions.info; drawerName: "info" }
            MenuSeparator {}
            MenuItem { text: "Split here"; onTriggered: { session.seek(soundMenu.at); actions.split.trigger() } }
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
                        KeyMarks { clip: overlayItem.modelData }
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
                            font.pixelSize: Math.round(11 * appSettings.textScale)
                            elide: Text.ElideRight
                        }
                        // A drag moves the clip in time (snapping) and across lanes.
                        ClipDrag {
                            owner: overlayItem
                            clip: overlayItem.modelData
                            slack: 4
                            verticalSlack: 6
                            onMenu: (at) => {
                                const menu = session.info.isTitle ? titleMenu : overlayMenu
                                menu.at = at
                                menu.popup()
                            }
                            onDragged: (dx, dy, x) => {
                                const m = overlayItem.modelData
                                const start = timelinePanel.snap(m.start + dx / timelinePanel.scale, m.duration, m.id)
                                overlayItem.moveX = (start - m.start) * timelinePanel.scale
                                overlayItem.moveY = dy
                            }
                            onSettled: {
                                overlayItem.moveY = 0
                                timelinePanel.unsnap()
                            }
                            // Away from the storyline: up for video lanes (-1), down for sound lanes (1).
                            onDropped: (moveX, moveY, x) => {
                                const lanes = -1 * Math.round(moveY / (timelinePanel.laneHeight + timelinePanel.laneGap))
                                const frames = Math.round(moveX / timelinePanel.scale * session.frameRate)
                                if (lanes !== 0 || frames !== 0) session.moveClip(overlayItem.modelData.id, lanes, frames)
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
                        KeyMarks { clip: soundItem.modelData }
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
                                font.pixelSize: Math.round(11 * appSettings.textScale)
                                elide: Text.ElideRight
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        // A drag moves the clip in time (snapping) and across lanes.
                        ClipDrag {
                            owner: soundItem
                            clip: soundItem.modelData
                            slack: 4
                            verticalSlack: 6
                            onMenu: (at) => {
                                soundMenu.at = at
                                soundMenu.popup()
                            }
                            onDragged: (dx, dy, x) => {
                                const m = soundItem.modelData
                                const start = timelinePanel.snap(m.start + dx / timelinePanel.scale, m.duration, m.id)
                                soundItem.moveX = (start - m.start) * timelinePanel.scale
                                soundItem.moveY = dy
                            }
                            onSettled: {
                                soundItem.moveY = 0
                                timelinePanel.unsnap()
                            }
                            // Away from the storyline: up for video lanes (-1), down for sound lanes (1).
                            onDropped: (moveX, moveY, x) => {
                                const lanes = 1 * Math.round(moveY / (timelinePanel.laneHeight + timelinePanel.laneGap))
                                const frames = Math.round(moveX / timelinePanel.scale * session.frameRate)
                                if (lanes !== 0 || frames !== 0) session.moveClip(soundItem.modelData.id, lanes, frames)
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
        MarkerFlags { panel: timelinePanel }
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
            font.pixelSize: Math.round(11 * appSettings.textScale)
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
            objectName: "zoomSlider"
            Layout.preferredWidth: 110
            implicitHeight: 24 // the target to aim at; the groove stays thin (WCAG 2.5.8)
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

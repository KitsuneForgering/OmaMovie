import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import OmaMovie

// Viewer (ui-design §5): neutral background, untouched by the theme; the GPU preview with
// the direct-manipulation overlay. Reads the window's ids from the document that places it.
Rectangle {
    Layout.fillWidth: true
    Layout.fillHeight: true
    color: root.viewerBackground
    PreviewItem { id: previewItem; objectName: "preview"; anchors.fill: parent }
    // Direct manipulation (UX-07): with Crop & framing open, the selected clip's
    // picture shows a box; drag inside to move it, drag the corner to scale it.
    // The picture follows (a preview); release makes one undoable edit.
    Item {
        id: framingOverlay
        objectName: "framingOverlay"
        visible: root.drawer === "crop" && session.selectedClip > 0 && !session.playing &&
                 session.canvasWidth > 0
        // Where the canvas sits inside the viewer (fitted, centered).
        readonly property real k: Math.min(previewItem.width / Math.max(1, session.canvasWidth),
                                           previewItem.height / Math.max(1, session.canvasHeight))
        x: (previewItem.width - session.canvasWidth * k) / 2
        y: (previewItem.height - session.canvasHeight * k) / 2
        width: session.canvasWidth * k
        height: session.canvasHeight * k
        // The transform being dragged; the committed one otherwise.
        property real posX: session.motion.posX || 0
        property real posY: session.motion.posY || 0
        property real scaleNow: session.motion.scale || 1
        property real rotationNow: session.motion.rotation || 0
        property bool dragging: false
        // Crop handles: the crop being dragged, the committed one otherwise.
        property bool cropping: false
        property var cropNow: [0, 0, 0, 0]
        readonly property var cropShown: cropping ? cropNow
            : [session.info.cropLeft || 0, session.info.cropTop || 0,
               session.info.cropRight || 0, session.info.cropBottom || 0]
        // Re-read when the transform at the playhead changes too.
        readonly property var edges: (session.motion, session.cropBox(cropShown[0], cropShown[1], cropShown[2], cropShown[3]))
        function previewCrop(edge, value) {
            const next = cropNow.slice()
            next[edge] = value
            cropNow = next
            session.previewClipFraming(next[0], next[1], next[2], next[3])
        }
        function commitCrop() {
            cropping = false
            dragging = false
            const c = cropNow
            if (c.every(isFinite)) session.setClipFraming(session.info.fit || 0, c[0], c[1], c[2], c[3])
        }
        // The committed box, moved and scaled by the gesture in progress.
        readonly property var base: session.selectedBox
        readonly property var box: cropping ? edges : base.w === undefined ? ({}) : ({
            x: base.x + base.w / 2 * (1 - scaleNow / (session.motion.scale || 1)) +
               (posX - (session.motion.posX || 0)) / Math.max(1, session.canvasWidth),
            y: base.y + base.h / 2 * (1 - scaleNow / (session.motion.scale || 1)) +
               (posY - (session.motion.posY || 0)) / Math.max(1, session.canvasHeight),
            w: base.w * scaleNow / (session.motion.scale || 1),
            h: base.h * scaleNow / (session.motion.scale || 1)
        })
        function preview(x, y, scale, rotation) {
            posX = x
            posY = y
            scaleNow = scale
            rotationNow = rotation === undefined ? (session.motion.rotation || 0) : rotation
            session.previewClipTransform(x, y, scale, rotationNow)
        }
        function commit(x, y, scale, rotation) {
            dragging = false
            const turn = rotation === undefined ? (session.motion.rotation || 0) : rotation
            if (isFinite(x) && isFinite(y) && isFinite(scale) && isFinite(turn))
                session.setClipTransform(x, y, scale, turn)
            // Follow the model again (the drag replaced the bindings).
            posX = Qt.binding(() => session.motion.posX || 0)
            posY = Qt.binding(() => session.motion.posY || 0)
            scaleNow = Qt.binding(() => session.motion.scale || 1)
            rotationNow = Qt.binding(() => session.motion.rotation || 0)
        }
        Rectangle {
            id: layerBox
            objectName: "layerBoxRect"
            visible: framingOverlay.box.w !== undefined
            x: (framingOverlay.box.x || 0) * framingOverlay.width
            y: (framingOverlay.box.y || 0) * framingOverlay.height
            width: (framingOverlay.box.w || 0) * framingOverlay.width
            height: (framingOverlay.box.h || 0) * framingOverlay.height
            color: "transparent"
            border.color: root.accent
            border.width: 2
            // Turned about its center, the compositor's pivot for clip rotation.
            rotation: framingOverlay.cropping ? (session.motion.rotation || 0) : framingOverlay.rotationNow
            MouseArea {
                objectName: "framingMove"
                anchors.fill: parent
                preventStealing: true
                cursorShape: Qt.SizeAllCursor
                property point from
                property real startX: 0
                property real startY: 0
                property real startScale: 1
                function moved(mouse) {
                    const p = mapToItem(framingOverlay, mouse.x, mouse.y)
                    return [Math.round(startX + (p.x - from.x) / framingOverlay.k),
                            Math.round(startY + (p.y - from.y) / framingOverlay.k)]
                }
                onPressed: (mouse) => {
                    from = mapToItem(framingOverlay, mouse.x, mouse.y)
                    startX = session.motion.posX || 0
                    startY = session.motion.posY || 0
                    startScale = session.motion.scale || 1
                    framingOverlay.dragging = true
                }
                onPositionChanged: (mouse) => {
                    const m = moved(mouse)
                    framingOverlay.preview(m[0], m[1], startScale)
                }
                onReleased: (mouse) => {
                    const m = moved(mouse)
                    framingOverlay.commit(m[0], m[1], startScale)
                }
                onCanceled: framingOverlay.commit(framingOverlay.posX, framingOverlay.posY, framingOverlay.scaleNow)
            }
            Rectangle { // scale from the bottom-right corner, around the box's center
                objectName: "framingScale"
                width: 14; height: 14; radius: 3
                x: parent.width - width / 2; y: parent.height - height / 2
                color: root.accent
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -6
                    preventStealing: true
                    cursorShape: Qt.SizeFDiagCursor
                    property real startScale: 1
                    property real startDistance: 1
                    function distance(mouse) {
                        const p = mapToItem(framingOverlay, mouse.x, mouse.y)
                        const cx = layerBox.x + layerBox.width / 2
                        const cy = layerBox.y + layerBox.height / 2
                        return Math.max(1, Math.hypot(p.x - cx, p.y - cy))
                    }
                    function scaled(mouse) {
                        return Math.max(0.1, Math.min(4, startScale * distance(mouse) / startDistance))
                    }
                    onPressed: (mouse) => {
                        startScale = session.motion.scale || 1
                        startDistance = distance(mouse)
                        framingOverlay.dragging = true
                    }
                    onPositionChanged: (mouse) => framingOverlay.preview(session.motion.posX || 0, session.motion.posY || 0, scaled(mouse))
                    onReleased: (mouse) => framingOverlay.commit(session.motion.posX || 0, session.motion.posY || 0, scaled(mouse))
                    onCanceled: framingOverlay.commit(framingOverlay.posX, framingOverlay.posY, framingOverlay.scaleNow)
                }
            }
            // Rotate: a knob above the top edge; Shift snaps to 15° steps.
            Rectangle {
                x: parent.width / 2 - 1
                y: -22
                width: 2
                height: 22
                color: root.accent
            }
            Rectangle {
                objectName: "framingRotate"
                width: 14; height: 14; radius: 7
                x: parent.width / 2 - width / 2; y: -22 - height / 2
                color: root.accent
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -6
                    preventStealing: true
                    cursorShape: Qt.CrossCursor
                    property real startRotation: 0
                    property real startAngle: 0
                    function angle(mouse) {
                        const p = mapToItem(framingOverlay, mouse.x, mouse.y)
                        const cx = layerBox.x + layerBox.width / 2
                        const cy = layerBox.y + layerBox.height / 2
                        return Math.atan2(p.y - cy, p.x - cx) * 180 / Math.PI
                    }
                    function turned(mouse) {
                        let r = startRotation + angle(mouse) - startAngle
                        r = ((r + 180) % 360 + 360) % 360 - 180
                        return mouse.modifiers & Qt.ShiftModifier ? Math.round(r / 15) * 15 : r
                    }
                    onPressed: (mouse) => {
                        startRotation = session.motion.rotation || 0
                        startAngle = angle(mouse)
                        framingOverlay.dragging = true
                    }
                    onPositionChanged: (mouse) => framingOverlay.preview(session.motion.posX || 0, session.motion.posY || 0,
                                                                         session.motion.scale || 1, turned(mouse))
                    onReleased: (mouse) => framingOverlay.commit(session.motion.posX || 0, session.motion.posY || 0,
                                                                 session.motion.scale || 1, turned(mouse))
                    onCanceled: framingOverlay.commit(framingOverlay.posX, framingOverlay.posY, framingOverlay.scaleNow,
                                                      framingOverlay.rotationNow)
                }
            }
        }
    }
    // Crop handles (UX-07): one per edge, at its middle; dragging one crops
    // that edge of the source, mapped back exactly through the clip's geometry.
    Repeater {
        model: ["left", "top", "right", "bottom"]
        delegate: Rectangle {
            id: cropHandle
            required property string modelData
            required property int index
            readonly property var at: framingOverlay.edges[modelData]
            readonly property bool across: modelData === "left" || modelData === "right"
            objectName: "cropHandle"
            visible: framingOverlay.visible && at !== undefined && !(framingOverlay.dragging && !framingOverlay.cropping)
            parent: framingOverlay
            width: across ? 6 : 28
            height: across ? 28 : 6
            radius: 3
            x: (at ? at.x : 0) * framingOverlay.width - width / 2
            y: (at ? at.y : 0) * framingOverlay.height - height / 2
            color: "white"
            border.color: root.accent
            border.width: 1
            MouseArea {
                anchors.fill: parent
                anchors.margins: -8
                preventStealing: true
                cursorShape: cropHandle.across ? Qt.SizeHorCursor : Qt.SizeVerCursor
                property var from: [0, 0, 0, 0]
                function valueAt(mouse) {
                    const p = mapToItem(framingOverlay, mouse.x, mouse.y)
                    return session.cropEdgeAt(cropHandle.index, p.x / framingOverlay.width, p.y / framingOverlay.height,
                                              from[0], from[1], from[2], from[3])
                }
                onPressed: {
                    from = framingOverlay.cropShown.slice()
                    framingOverlay.cropNow = from
                    framingOverlay.cropping = true
                    framingOverlay.dragging = true
                }
                onPositionChanged: (mouse) => framingOverlay.previewCrop(cropHandle.index, valueAt(mouse))
                onReleased: (mouse) => { framingOverlay.previewCrop(cropHandle.index, valueAt(mouse)); framingOverlay.commitCrop() }
                onCanceled: framingOverlay.commitCrop()
            }
        }
    }
    UiText {
        anchors.centerIn: parent
        visible: !session.hasMedia && !session.failed
        text: session.media.length ? "Opening…" : "Import a video to start" + root.shortcutText(actions.importMedia)
        color: "#8f9095"
        font.pixelSize: 12
    }
    // Notices (Session::notice): a short explanation after an action that did less than asked,
    // and the lasting one after the GPU device is lost.
    Rectangle {
        objectName: "notice"
        visible: !session.failed && session.notice !== ""
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        width: Math.min(parent.width - 24, noticeText.implicitWidth + 24)
        height: noticeText.implicitHeight + 14
        radius: 6
        color: colors.dark_background
        border.width: 1
        border.color: root.line
        UiText {
            id: noticeText
            anchors.centerIn: parent
            width: Math.min(implicitWidth, parent.parent.width - 48)
            text: session.notice
            color: root.fg
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
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
        UiText {
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

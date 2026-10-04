import QtQuick

// One grading curve (ADR-0012) for the selected clip: click to add a point, drag a point to move
// it, double-click an inner point to remove it. Point editing and the curve's shape live in the
// session (C++); this only draws what it gets and reports gestures. The channel colors are
// fixed references, never themed.
Item {
    id: editor
    property int channel: 0               // 0 master, 1-3 red, green, blue
    property var points: []               // [{x, y}] of the clip's curve
    property var samples: []              // the curve across [0, 1], for drawing
    readonly property color tint: ["#d8d8dc", "#e06060", "#60c060", "#6090e0"][channel]

    property int dragIndex: -1
    property point dragAt: Qt.point(0, 0) // the dragged point, in curve coordinates

    function toX(v) { return v * width }
    function toY(v) { return (1 - v) * height }
    function fromMouse(mx, my) {
        return Qt.point(Math.max(0, Math.min(1, mx / width)), Math.max(0, Math.min(1, 1 - my / height)))
    }

    onSamplesChanged: canvas.requestPaint()
    onChannelChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.25)
        border.color: colors.lighter_background
        radius: 3
    }
    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.08)
            ctx.lineWidth = 1
            for (let i = 1; i < 4; ++i) { // quarter grid
                ctx.beginPath(); ctx.moveTo(width * i / 4, 0); ctx.lineTo(width * i / 4, height); ctx.stroke()
                ctx.beginPath(); ctx.moveTo(0, height * i / 4); ctx.lineTo(width, height * i / 4); ctx.stroke()
            }
            ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.18)
            ctx.beginPath(); ctx.moveTo(0, height); ctx.lineTo(width, 0); ctx.stroke()
            const s = editor.samples
            if (s.length < 2) return
            ctx.strokeStyle = editor.tint
            ctx.lineWidth = 2
            ctx.beginPath()
            for (let i = 0; i < s.length; ++i) {
                const px = editor.toX(i / (s.length - 1))
                const py = editor.toY(s[i])
                if (i === 0) ctx.moveTo(px, py)
                else ctx.lineTo(px, py)
            }
            ctx.stroke()
        }
    }
    MouseArea { // empty area: add a point
        anchors.fill: parent
        onClicked: (mouse) => {
            const p = editor.fromMouse(mouse.x, mouse.y)
            session.addCurvePoint(editor.channel, p.x, p.y)
        }
    }
    Repeater {
        model: editor.points
        delegate: Rectangle {
            required property var modelData
            required property int index
            readonly property bool dragged: editor.dragIndex === index
            width: 10
            height: 10
            radius: 5
            x: editor.toX(dragged ? editor.dragAt.x : modelData.x) - width / 2
            y: editor.toY(dragged ? editor.dragAt.y : modelData.y) - height / 2
            color: dragged ? "white" : editor.tint
            border.color: "black"
            MouseArea {
                anchors.fill: parent
                anchors.margins: -4
                preventStealing: true
                cursorShape: Qt.SizeAllCursor
                onPressed: {
                    editor.dragAt = Qt.point(modelData.x, modelData.y)
                    editor.dragIndex = index
                }
                onPositionChanged: (mouse) => {
                    if (!pressed) return
                    const p = mapToItem(editor, mouse.x, mouse.y)
                    editor.dragAt = editor.fromMouse(p.x, p.y)
                }
                onReleased: {
                    const i = editor.dragIndex
                    editor.dragIndex = -1
                    session.moveCurvePoint(editor.channel, i, editor.dragAt.x, editor.dragAt.y)
                }
                onDoubleClicked: session.removeCurvePoint(editor.channel, index)
            }
        }
    }
}

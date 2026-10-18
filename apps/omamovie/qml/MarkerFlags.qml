import QtQuick
import QtQuick.Controls
import OmaMovie

// Sequence markers (M8) as small flags along the top of the timeline content; activating one
// (pointer, or Tab then Space) moves the playhead onto it. Placed by the timeline panel, whose
// `origin` and `scale` map seconds to content pixels.
Item {
    id: flags
    required property Item panel
    Repeater {
        model: session.markers
        delegate: AbstractButton {
            id: flag
            required property var modelData
            objectName: "markerFlag"
            x: flags.panel.origin + modelData.time * flags.panel.scale - 5
            y: 0
            width: 10
            height: 12
            z: 6
            activeFocusOnTab: true
            Accessible.name: modelData.name + " at " + root.timecode(modelData.time)
            ToolTip.text: modelData.name
            ToolTip.visible: hovered
            onClicked: session.seek(modelData.time)
            background: Canvas {
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.fillStyle = colors.yellow || root.accent
                    ctx.beginPath()
                    ctx.moveTo(0, 0); ctx.lineTo(width, 0); ctx.lineTo(width / 2, height); ctx.closePath()
                    ctx.fill()
                    if (flag.activeFocus) { ctx.strokeStyle = root.fg; ctx.stroke() }
                }
                Connections { target: flag; function onActiveFocusChanged() { parent.requestPaint() } }
            }
        }
    }
}

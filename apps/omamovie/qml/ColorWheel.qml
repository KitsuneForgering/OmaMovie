import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

// A lift, gamma or gain wheel (ADR-0012): dragging the dot tints toward a hue (x, y in the unit
// disc, red to the right, green and blue 120° apart counter-clockwise), the slider sets the
// level. Double-click resets the tint. The values come from the clip; `committed` reports a
// finished drag or slider move, and the session turns the three wheels into the clip's CDL.
// The hue ring is a fixed color reference: never themed.
ColumnLayout {
    id: wheel
    property string label
    property real tintX: 0
    property real tintY: 0
    property real level: 0
    signal committed(real x, real y, real level)

    property bool dragging: false
    property real dragX: 0
    property real dragY: 0
    readonly property real shownX: dragging ? dragX : tintX
    readonly property real shownY: dragging ? dragY : tintY
    spacing: 2

    Text {
        Layout.alignment: Qt.AlignHCenter
        text: wheel.label
        color: colors.dark_foreground
        font.pixelSize: 10
        font.bold: true
    }
    Item {
        id: disc
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: 84
        Layout.preferredHeight: 84
        readonly property real radius: width / 2
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: "transparent"
                fillGradient: ConicalGradient {
                    centerX: disc.radius
                    centerY: disc.radius
                    angle: 0
                    GradientStop { position: 0.0; color: "#c84b4b" }
                    GradientStop { position: 0.333; color: "#4bc84b" }
                    GradientStop { position: 0.667; color: "#4b6ec8" }
                    GradientStop { position: 1.0; color: "#c84b4b" }
                }
                PathAngleArc { centerX: disc.radius; centerY: disc.radius; radiusX: disc.radius; radiusY: disc.radius; startAngle: 0; sweepAngle: 360 }
            }
        }
        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.62
            height: width
            radius: width / 2
            color: Qt.rgba(0.11, 0.11, 0.12, 0.85)
        }
        Rectangle { // the tint
            width: 12
            height: 12
            radius: 6
            x: disc.radius + wheel.shownX * disc.radius * 0.9 - width / 2
            y: disc.radius - wheel.shownY * disc.radius * 0.9 - height / 2
            color: "white"
            border.color: "black"
            border.width: 1
        }
        MouseArea {
            anchors.fill: parent
            preventStealing: true
            cursorShape: Qt.CrossCursor
            function place(mouse) {
                let x = (mouse.x - disc.radius) / (disc.radius * 0.9)
                let y = -(mouse.y - disc.radius) / (disc.radius * 0.9)
                const r = Math.hypot(x, y)
                if (r > 1) { x /= r; y /= r }
                wheel.dragX = x
                wheel.dragY = y
            }
            onPressed: (mouse) => { wheel.dragging = true; place(mouse) }
            onPositionChanged: (mouse) => { if (pressed) place(mouse) }
            onReleased: {
                wheel.dragging = false
                wheel.committed(wheel.dragX, wheel.dragY, wheel.level)
            }
            onDoubleClicked: wheel.committed(0, 0, wheel.level)
        }
    }
    Slider {
        id: levelSlider
        Layout.preferredWidth: 96
        Layout.alignment: Qt.AlignHCenter
        from: -1
        to: 1
        focusPolicy: Qt.TabFocus
        Keys.onShortcutOverride: (event) => event.accepted = root.sliderKeys.includes(event.key)
        onPressedChanged: if (!pressed) wheel.committed(wheel.tintX, wheel.tintY, value)
        onMoved: if (!pressed) wheel.committed(wheel.tintX, wheel.tintY, value)
        Binding on value { value: wheel.level; when: !levelSlider.pressed }
    }
}

import QtQuick

// Two-up after a roll, slip or slide (Session::edgeFrames): the two source frames the edit is
// judged by, ungraded, side by side over the viewer until two seconds after the last nudge.
Row {
    id: edge
    objectName: "edgeFrames"
    property int shown: -1
    spacing: 4
    visible: shown === session.edgeFrames.serial
    Repeater {
        model: ["left", "right"]
        delegate: Rectangle {
            required property string modelData
            width: 320
            height: 180
            color: "black"
            border.width: 1
            border.color: root.line
            Image {
                anchors.fill: parent
                anchors.margins: 1
                fillMode: Image.PreserveAspectFit
                cache: false
                source: session.edgeFrames[parent.modelData] || ""
            }
        }
    }
    Timer {
        id: hide
        interval: 2000
        onTriggered: edge.shown = -1
    }
    Connections {
        target: session
        function onEdgeFramesChanged() {
            edge.shown = session.edgeFrames.serial
            hide.restart()
        }
    }
}

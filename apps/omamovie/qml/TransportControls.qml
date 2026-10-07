import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Transport (ui-design §5): the same actions serve buttons and keyboard shortcuts.
// Reads the window's ids (root, actions, session) from the document that places it.
Rectangle {
    id: transport
    Layout.fillWidth: true
    Layout.preferredHeight: 42
    color: colors.background
    // Timecode on the left, the playback controls centered on the viewer, the
    // full-screen toggle on the right.
    UiText {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        // The source viewer has its own playhead: its time, not the sequence's.
        text: session.source.open ? root.timecode(session.source.position) + (root.narrow ? "" : " / " + root.timecode(session.source.duration))
                                  : root.timecode(session.position) + (root.narrow ? "" : " / " + root.timecode(session.duration))
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
        UiText { // shuttle speed (J/K/L), shown only when not normal
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

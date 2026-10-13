import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Shapes
import OmaMovie

// Info drawer (ui-design §6): the selected clip's media facts.
// Reads the window's ids (root, actions, session) from the document that places it.
Rectangle {
    Layout.fillWidth: true
    Layout.preferredHeight: 64 + (dopeSheet.visible ? dopeSheet.implicitHeight + 8 : 0)
    visible: !root.viewerOnly && root.drawer === "info" && !!session.info.name
    color: colors.dark_background
    GridLayout {
        id: facts
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 10
        columns: root.compact ? 2 : 4
        columnSpacing: 22
        rowSpacing: 4
        Field { label: "NAME"; value: session.info.name || "" }
        Field { label: "DURATION"; value: root.timecode(session.info.duration || 0) }
        Field { label: "FORMAT"; value: (session.info.codec || "") + "  " + (session.info.resolution || "") + "  " + (session.info.frameRate || "") }
        Field { label: "DECODE PATH"; value: session.info.decodePath || "" }
    }
    DopeSheet {
        id: dopeSheet
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: facts.bottom
        anchors.margins: 10
    }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: root.line }
}

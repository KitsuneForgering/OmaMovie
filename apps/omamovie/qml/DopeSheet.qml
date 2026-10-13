import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import OmaMovie

// A simple dopesheet (M8): one row per animated parameter of the selected clip, a diamond per
// key across the clip's length. Activating a diamond (pointer, or Tab then Space) moves the
// playhead onto that key, where the Crop and Volume drawers' key toggles and sliders edit it.
ColumnLayout {
    id: sheet
    spacing: 4
    visible: session.keyTracks.length > 0
    UiText { text: "KEYS"; color: root.muted; font.pixelSize: 11; font.bold: true }
    Repeater {
        model: session.keyTracks
        delegate: RowLayout {
            id: row
            required property var modelData
            Layout.fillWidth: true
            UiText { text: row.modelData.name; color: root.fg; font.pixelSize: 12; Layout.preferredWidth: 80 }
            Item {
                id: lane
                Layout.fillWidth: true
                Layout.preferredHeight: 16
                Rectangle { anchors.verticalCenter: parent.verticalCenter; width: parent.width; height: 1; color: root.line }
                Repeater {
                    model: row.modelData.times
                    delegate: AbstractButton {
                        id: key
                        required property real modelData
                        objectName: "dopeKey"
                        readonly property real span: Math.max(1e-6, row.modelData.end - row.modelData.start)
                        x: (modelData - row.modelData.start) / span * (lane.width - width)
                        width: 12; height: 12
                        anchors.verticalCenter: parent.verticalCenter
                        activeFocusOnTab: true
                        Accessible.name: row.modelData.name + " key at " + root.timecode(modelData)
                        onClicked: session.seek(modelData)
                        background: Rectangle {
                            rotation: 45
                            color: Math.abs(session.position - key.modelData) < 0.5 / session.frameRate ? root.accent : root.fg
                            border.color: key.activeFocus ? root.accent : colors.dark_background
                            border.width: key.activeFocus ? 2 : 1
                        }
                    }
                }
            }
        }
    }
}

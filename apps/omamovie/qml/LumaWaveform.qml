import QtQuick
import QtQuick.Layouts
import OmaMovie

// The luma waveform scope (M9) of the frame at the playhead, beside the grading controls. It
// asks for a new one shortly after the playhead stops or an edit changes the frame, never
// while playing; the image is measured, so it is shown untinted (ui-design §10).
ColumnLayout {
    id: scope
    spacing: 4
    Layout.alignment: Qt.AlignTop
    UiText { text: "LUMA WAVEFORM"; color: root.muted; font.pixelSize: 11; font.bold: true }
    Rectangle {
        Layout.preferredWidth: 192
        Layout.preferredHeight: 108
        color: "black" // a measurement background, not the theme's
        Image {
            objectName: "lumaWaveform"
            anchors.fill: parent
            fillMode: Image.Stretch
            cache: false
            source: session.scopeImage
        }
    }
    Timer {
        id: refresh
        interval: 120
        onTriggered: if (scope.visible && !session.playing) session.requestScopes()
    }
    onVisibleChanged: refresh.restart()
    Connections {
        target: session
        function onPositionChanged() { refresh.restart() }
        function onSequenceChanged() { refresh.restart() }
        function onSelectionChanged() { refresh.restart() }
    }
}

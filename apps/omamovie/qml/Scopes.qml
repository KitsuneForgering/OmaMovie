import QtQuick
import QtQuick.Layouts
import OmaMovie

// Scopes (M9) of the frame at the playhead, beside the grading controls: a luma waveform (or an
// RGB parade) and a vectorscope. New ones are asked for shortly after the playhead stops or an edit changes the
// frame, never while playing; they are measurements, so they are shown untinted (ui-design §10).
RowLayout {
    id: scopes
    spacing: 8
    Layout.alignment: Qt.AlignTop
    component Scope: ColumnLayout {
        property alias label: title.text
        property alias source: picture.source
        property real scopeWidth: 108
        spacing: 4
        UiText { id: title; color: root.muted; font.pixelSize: 11; font.bold: true }
        Rectangle {
            Layout.preferredWidth: parent.scopeWidth
            Layout.preferredHeight: 108
            color: "black" // a measurement background, not the theme's
            Image { id: picture; anchors.fill: parent; fillMode: Image.Stretch; cache: false }
        }
    }
    // Luma or RGB parade in the same place: the title or the palette's action switches them.
    Scope {
        objectName: "lumaWaveform"
        label: root.scopeParade ? "RGB PARADE ⇄" : "LUMA ⇄"
        source: root.scopeParade ? session.paradeImage : session.scopeImage
        scopeWidth: 192
        TapHandler { onTapped: actions.toggleParade.trigger() }
    }
    Scope { objectName: "vectorscope"; label: "VECTORSCOPE"; source: session.vectorscopeImage }
    Timer {
        id: refresh
        interval: 120
        onTriggered: if (scopes.visible && !session.playing) session.requestScopes()
    }
    onVisibleChanged: refresh.restart()
    Connections {
        target: session
        function onPositionChanged() { refresh.restart() }
        function onSequenceChanged() { refresh.restart() }
        function onSelectionChanged() { refresh.restart() }
    }
}

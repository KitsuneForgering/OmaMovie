import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A labelled drawer slider (ui-design §6): a readout, and `committed` once per gesture or key step.
ColumnLayout {
    id: drawerSlider
    property alias slider: control
    property string label
    property string readout
    signal committed()
    spacing: 0
    Layout.fillWidth: true
    RowLayout {
        UiText { text: drawerSlider.label; color: root.muted; font.pixelSize: 10; font.bold: true }
        Item { Layout.fillWidth: true }
        UiText { text: drawerSlider.readout; color: root.fg; font.pixelSize: 11 }
    }
    Slider {
        id: control
        Layout.fillWidth: true
        // Tab reaches it; while focused, the arrows and Home/End move it
        // instead of the playhead, and each key step is one command.
        focusPolicy: Qt.TabFocus
        Keys.onShortcutOverride: (event) => event.accepted = root.sliderKeys.includes(event.key)
        onPressedChanged: if (!pressed) drawerSlider.committed()
        onMoved: if (!pressed) drawerSlider.committed()
    }
}

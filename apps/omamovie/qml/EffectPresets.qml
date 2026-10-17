import QtQuick
import QtQuick.Controls
import OmaMovie

// Built-in effect presets (M8) as one button with a menu in the Effects drawer's header, where
// it costs no width from the looks (two whole tiles must stay visible at 640 px). Each item adds or updates its effects on the selected clip
// in one undoable edit (Session::applyEffectPreset); the looks and the stack then show and
// edit them like any other effect.
OmaButton {
    id: button
    objectName: "effectPresets"
    text: "Presets"
    implicitHeight: 24
    onClicked: menu.popup(button, 0, button.height)
    Menu {
        id: menu
        Repeater {
            model: session.effectPresets
            delegate: MenuItem {
                required property var modelData
                objectName: "preset:" + modelData.id
                text: modelData.name
                onTriggered: session.applyEffectPreset(modelData.id)
            }
        }
    }
}

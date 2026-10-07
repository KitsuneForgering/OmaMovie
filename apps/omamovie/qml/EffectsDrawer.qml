import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Shapes
import OmaMovie

// Effects drawer (ui-design §6, ADR-0016): looks shown as previews of the clip itself; a tile
// adds or removes its look, and the clip's stack lists its effects in render order with bypass,
// amount, reordering within a stage and removal. Effects this build does not know stay listed so
// their settings remain visible. Reads the window's ids (root, actions, session) from the
// document that places it.
Rectangle {
    id: effectsDrawer
    Layout.fillWidth: true
    Layout.preferredHeight: 112
    visible: !root.viewerOnly && root.drawer === "effects" && actions.effects.enabled
    color: colors.dark_background
    readonly property var stack: session.info.effects || []
    function applied(id) { return stack.some(e => e.id === id) }
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 14
        ListView {
            objectName: "lookTiles"
            Layout.fillWidth: true
            Layout.minimumWidth: 2 * 92 + 8 // two whole looks even at 640 px (UX-06)
            Layout.fillHeight: true
            Layout.topMargin: 8
            orientation: ListView.Horizontal
            spacing: 8
            clip: true
            model: session.lookEffects
            // Tab reaches each look and Enter toggles it, like OmaButton (UX-05).
            delegate: AbstractButton {
                id: lookTile
                objectName: "look:" + modelData.id
                width: 92
                height: 84
                focusPolicy: Qt.TabFocus
                Keys.onReturnPressed: click()
                Keys.onEnterPressed: click()
                onActiveFocusChanged: if (activeFocus) ListView.view.positionViewAtIndex(index, ListView.Contain)
                onClicked: session.setClipEffect(modelData.id, !chosen)
                readonly property bool chosen: effectsDrawer.applied(modelData.id)
                Accessible.name: modelData.name
                Rectangle {
                    id: tile
                    width: parent.width
                    height: 54
                    radius: 4
                    color: root.viewerBackground
                    border.width: chosen || lookTile.visualFocus ? 2 : 0
                    border.color: lookTile.visualFocus && !chosen ? colors.accent : root.accent
                    Image {
                        anchors.fill: parent
                        anchors.margins: chosen ? 2 : 0
                        fillMode: Image.PreserveAspectFit
                        source: session.filterPreviews.length > index ? session.filterPreviews[index] : ""
                        asynchronous: true
                    }
                }
                UiText {
                    anchors.top: tile.bottom
                    anchors.topMargin: 4
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: modelData.name
                    color: chosen ? root.accent : root.fg
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
                HoverHandler { cursorShape: Qt.PointingHandCursor }
            }
        }
        ListView {
            id: stackView
            // Shrinks before the slider is pushed out: down to one effect, the rest scroll.
            Layout.fillWidth: true
            Layout.preferredWidth: Math.min(contentWidth, 3 * 178)
            Layout.maximumWidth: Math.min(contentWidth, 3 * 178)
            Layout.minimumWidth: Math.min(contentWidth, 170)
            Layout.fillHeight: true
            Layout.topMargin: 6
            visible: count > 0
            orientation: ListView.Horizontal
            spacing: 8
            clip: true
            model: effectsDrawer.stack
            delegate: ColumnLayout {
                width: 170
                spacing: 2
                opacity: modelData.enabled ? 1 : 0.6
                RowLayout {
                    spacing: 0
                    UiText {
                        Layout.fillWidth: true
                        text: modelData.known ? modelData.name : modelData.name + " (unavailable)"
                        color: modelData.known ? root.fg : root.muted
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                    OmaButton {
                        iconName: "arrowLeft"; showLabel: false; implicitHeight: 24
                        tip: "Apply earlier"
                        enabled: modelData.known && index > 0
                        onClicked: session.moveClipEffect(modelData.id, -1)
                    }
                    OmaButton {
                        iconName: "arrowRight"; showLabel: false; implicitHeight: 24
                        tip: "Apply later"
                        enabled: modelData.known && index < stackView.count - 1
                        onClicked: session.moveClipEffect(modelData.id, 1)
                    }
                }
                DrawerSlider {
                    id: amount
                    visible: modelData.known
                    label: "AMOUNT"
                    readout: Math.round(amount.slider.value * 100) + "%"
                    onCommitted: session.setClipEffectParam(modelData.id, "amount", amount.slider.value)
                    Binding { target: amount.slider; property: "value"; value: modelData.amount; when: !amount.slider.pressed }
                }
                RowLayout {
                    spacing: 4
                    OmaButton {
                        text: modelData.enabled ? "On" : "Off"; implicitHeight: 24
                        selected: modelData.enabled
                        tip: modelData.enabled ? "Bypass this effect" : "Apply this effect"
                        onClicked: session.setClipEffectEnabled(modelData.id, !modelData.enabled)
                    }
                    OmaButton {
                        text: "Remove"; implicitHeight: 24
                        onClicked: session.setClipEffect(modelData.id, false)
                    }
                }
            }
        }
        DrawerSlider {
            id: sharpnessControl
            objectName: "sharpnessControl"
            Layout.fillWidth: false
            Layout.preferredWidth: 180
            Layout.minimumWidth: 120
            label: "SOFTEN · SHARPEN"
            readout: sharpnessControl.slider.value < 0 ? "Soften " + Math.round(-sharpnessControl.slider.value * 100)
                   : sharpnessControl.slider.value > 0 ? "Sharpen " + Math.round(sharpnessControl.slider.value * 100) : "Off"
            onCommitted: session.setClipSharpness(sharpnessControl.slider.value)
        }
    }
    Binding { target: sharpnessControl.slider; property: "from"; value: -1 }
    Binding { target: sharpnessControl.slider; property: "value"; value: session.info.sharpness || 0; when: !sharpnessControl.slider.pressed }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: root.line }
}

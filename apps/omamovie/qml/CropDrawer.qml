import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Shapes
import OmaMovie

// Crop drawer (ui-design §6): fit mode and edges; More adds position, scale, rotation and opacity.
// Reads the window's ids (root, actions, session) from the document that places it.
Rectangle {
    id: cropDrawer
    Layout.fillWidth: true
    Layout.preferredHeight: root.cropMore ? 124 : 64
    visible: !root.viewerOnly && root.drawer === "crop" && actions.crop.enabled
    color: colors.dark_background
    function commitFraming(fit) {
        session.setClipFraming(fit, cropLeft.slider.value, cropTop.slider.value,
                               cropRight.slider.value, cropBottom.slider.value)
    }
    function commitTransform() {
        session.setClipTransform(posX.slider.value, posY.slider.value, scaleControl.slider.value,
                                 rotationControl.slider.value)
    }
    function percent(v) { return Math.round(v * 100) + "%" }
    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 6
        anchors.bottomMargin: 6
        spacing: 6
        RowLayout {
            Layout.fillWidth: true
            spacing: 14
            Row {
                spacing: 2
                Repeater {
                    model: [{ name: "Fit", fit: 0, tip: "Whole picture, letterboxed" },
                            { name: "Fill", fit: 1, tip: "Fill the frame, cropping what overflows" },
                            { name: "Stretch", fit: 2, tip: "Fill the frame, changing the proportions" }]
                    delegate: OmaButton {
                        text: modelData.name
                        tip: modelData.tip
                        selected: (session.info.fit || 0) === modelData.fit
                        onClicked: cropDrawer.commitFraming(modelData.fit)
                    }
                }
            }
            DrawerSlider { id: cropLeft; label: "LEFT"; readout: cropDrawer.percent(cropLeft.slider.value); onCommitted: cropDrawer.commitFraming(session.info.fit || 0) }
            DrawerSlider { id: cropRight; label: "RIGHT"; readout: cropDrawer.percent(cropRight.slider.value); onCommitted: cropDrawer.commitFraming(session.info.fit || 0) }
            DrawerSlider { id: cropTop; label: "TOP"; readout: cropDrawer.percent(cropTop.slider.value); onCommitted: cropDrawer.commitFraming(session.info.fit || 0) }
            DrawerSlider { id: cropBottom; label: "BOTTOM"; readout: cropDrawer.percent(cropBottom.slider.value); onCommitted: cropDrawer.commitFraming(session.info.fit || 0) }
            OmaButton {
                text: "More"
                selected: root.cropMore
                tip: "Position, scale and rotation"
                onClicked: root.cropMore = !root.cropMore
            }
        }
        RowLayout {
            Layout.fillWidth: true
            visible: root.cropMore
            spacing: 18
            DrawerSlider { id: posX; label: "POSITION X"; readout: Math.round(posX.slider.value) + " px"; onCommitted: cropDrawer.commitTransform() }
            DrawerSlider { id: posY; label: "POSITION Y"; readout: Math.round(posY.slider.value) + " px"; onCommitted: cropDrawer.commitTransform() }
            DrawerSlider { id: scaleControl; label: "SCALE"; readout: Math.round(scaleControl.slider.value * 100) + "%"; onCommitted: cropDrawer.commitTransform() }
            DrawerSlider { id: rotationControl; label: "ROTATION"; readout: rotationControl.slider.value.toFixed(1) + "°"; onCommitted: cropDrawer.commitTransform() }
            DrawerSlider { id: opacityControl; objectName: "opacityControl"; label: "OPACITY"; readout: cropDrawer.percent(opacityControl.slider.value); onCommitted: session.setClipOpacity(opacityControl.slider.value) }
            // Keyframes (M8): with keys, the sliders above set the one at the playhead.
            OmaButton {
                objectName: "transformKey"
                text: session.motion.keyHere ? "Remove key" : "Add key"
                iconName: "keyframe"
                iconFilled: !!session.motion.keyHere
                tip: session.motion.keys > 0
                     ? session.motion.keys + " keys: the sliders set the one at the playhead"
                     : "Animate: keep this framing at the playhead"
                selected: !!session.motion.keyHere
                onClicked: session.toggleTransformKey()
            }
            OmaButton {
                text: "Ken Burns"
                tip: "A slow push in over the whole clip"
                onClicked: session.kenBurns()
            }
            OmaButton {
                text: "Reset"
                enabled: !!session.info.framingAdjusted
                onClicked: session.resetClipFraming()
            }
        }
    }
    Binding { target: cropLeft.slider; property: "to"; value: 0.45 }
    Binding { target: cropLeft.slider; property: "value"; value: session.info.cropLeft || 0; when: !cropLeft.slider.pressed }
    Binding { target: cropRight.slider; property: "to"; value: 0.45 }
    Binding { target: cropRight.slider; property: "value"; value: session.info.cropRight || 0; when: !cropRight.slider.pressed }
    Binding { target: cropTop.slider; property: "to"; value: 0.45 }
    Binding { target: cropTop.slider; property: "value"; value: session.info.cropTop || 0; when: !cropTop.slider.pressed }
    Binding { target: cropBottom.slider; property: "to"; value: 0.45 }
    Binding { target: cropBottom.slider; property: "value"; value: session.info.cropBottom || 0; when: !cropBottom.slider.pressed }
    Binding { target: posX.slider; property: "from"; value: -session.canvasWidth }
    Binding { target: posX.slider; property: "to"; value: session.canvasWidth }
    Binding { target: posX.slider; property: "value"; value: session.motion.posX || 0; when: !posX.slider.pressed }
    Binding { target: posY.slider; property: "from"; value: -session.canvasHeight }
    Binding { target: posY.slider; property: "to"; value: session.canvasHeight }
    Binding { target: posY.slider; property: "value"; value: session.motion.posY || 0; when: !posY.slider.pressed }
    Binding { target: scaleControl.slider; property: "from"; value: 0.1 }
    Binding { target: scaleControl.slider; property: "to"; value: 4 }
    Binding { target: scaleControl.slider; property: "value"; value: session.motion.scale || 1; when: !scaleControl.slider.pressed }
    Binding { target: rotationControl.slider; property: "from"; value: -180 }
    Binding { target: rotationControl.slider; property: "to"; value: 180 }
    Binding { target: rotationControl.slider; property: "value"; value: session.motion.rotation || 0; when: !rotationControl.slider.pressed }
    Binding { target: opacityControl.slider; property: "value"; value: session.info.opacity === undefined ? 1 : session.info.opacity; when: !opacityControl.slider.pressed }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: root.line }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Shapes
import OmaMovie

// Color drawer (ui-design §6): exposure, contrast, saturation, temperature; More adds grading.
// Reads the window's ids (root, actions, session) from the document that places it.
Rectangle {
    id: colorDrawer
    Layout.fillWidth: true
    Layout.preferredHeight: root.colorMore ? 64 + 150 : 64
    visible: !root.viewerOnly && root.drawer === "color" && actions.color.enabled
    color: colors.dark_background
    function commit() {
        session.setClipColor(exposureControl.slider.value, contrastControl.slider.value,
                             saturationControl.slider.value, temperatureControl.slider.value)
    }
    function signed(v) { return (v > 0 ? "+" : "") + Math.round(v * 100) }
    // The three wheels as the session wants them, with one changed.
    function commitWheel(name, x, y, level) {
        const w = session.info.wheels || {}
        const all = { lift: w.lift || {}, gamma: w.gamma || {}, gain: w.gain || {} }
        all[name] = { x: x, y: y, level: level }
        session.setClipWheels(all)
    }
    RowLayout {
        id: colorBasics
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 64
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 18
        DrawerSlider {
            id: exposureControl
            label: "EXPOSURE"
            readout: (exposureControl.slider.value > 0 ? "+" : "") + exposureControl.slider.value.toFixed(2) + " EV"
            onCommitted: colorDrawer.commit()
        }
        DrawerSlider {
            id: contrastControl
            label: "CONTRAST"
            readout: colorDrawer.signed(contrastControl.slider.value)
            onCommitted: colorDrawer.commit()
        }
        DrawerSlider {
            id: saturationControl
            label: "SATURATION"
            readout: colorDrawer.signed(saturationControl.slider.value)
            onCommitted: colorDrawer.commit()
        }
        DrawerSlider {
            id: temperatureControl
            label: "TEMPERATURE"
            readout: temperatureControl.slider.value < 0 ? "Cooler " + Math.round(-temperatureControl.slider.value * 100)
                   : temperatureControl.slider.value > 0 ? "Warmer " + Math.round(temperatureControl.slider.value * 100) : "Neutral"
            onCommitted: colorDrawer.commit()
        }
        OmaButton {
            text: "Reset"
            enabled: !!session.info.colorAdjusted
            onClicked: { session.setClipColor(0, 0, 0, 0); session.resetClipGrade() }
        }
        OmaButton {
            text: "More"
            selected: root.colorMore
            activeDot: !!session.info.graded
            tip: "Color wheels, curves and LUTs"
            onClicked: root.colorMore = !root.colorMore
        }
    }
    // Grading: one tab at a time, so the drawer stays short.
    RowLayout {
        visible: root.colorMore
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: colorBasics.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.bottomMargin: 8
        spacing: 14
        ColumnLayout {
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: 84
            Layout.fillWidth: false
            spacing: 2
            Repeater {
                model: [{ name: "Wheels", tab: "wheels" }, { name: "Curves", tab: "curves" }, { name: "LUT", tab: "lut" }]
                delegate: OmaButton {
                    Layout.fillWidth: true
                    text: modelData.name
                    selected: root.gradeTab === modelData.tab
                    onClicked: root.gradeTab = modelData.tab
                }
            }
        }
        RowLayout { // lift, gamma, gain
            visible: root.gradeTab === "wheels"
            Layout.fillWidth: true
            spacing: 18
            Repeater {
                model: [{ name: "LIFT", key: "lift" }, { name: "GAMMA", key: "gamma" }, { name: "GAIN", key: "gain" }]
                delegate: ColorWheel {
                    required property var modelData
                    readonly property var value: (session.info.wheels || {})[modelData.key] || {}
                    label: modelData.name
                    tintX: value.x || 0
                    tintY: value.y || 0
                    level: value.level || 0
                    onCommitted: (x, y, level) => colorDrawer.commitWheel(modelData.key, x, y, level)
                }
            }
            Item { Layout.fillWidth: true }
        }
        RowLayout { // curves
            visible: root.gradeTab === "curves"
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10
            ColumnLayout {
                Layout.alignment: Qt.AlignTop
                spacing: 2
                Repeater {
                    model: ["Master", "Red", "Green", "Blue"]
                    delegate: OmaButton {
                        Layout.fillWidth: true
                        text: modelData
                        selected: root.curveChannel === index
                        activeDot: ((session.info.curves || {})[String(index)] || []).length > 0
                        onClicked: root.curveChannel = index
                    }
                }
            }
            CurveEditor {
                Layout.preferredWidth: 180
                Layout.fillHeight: true
                channel: root.curveChannel
                points: (session.info.curves || {})[String(root.curveChannel)] || []
                // Re-read whenever the selection or its properties change.
                samples: session.info && session.info.clip ? session.curveSamples(root.curveChannel, 96) : []
            }
            UiText {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                wrapMode: Text.WordWrap
                text: "Click to add a point, drag to move it, double-click to remove it."
                color: root.muted
                font.pixelSize: 11
            }
        }
        ColumnLayout { // LUT
            visible: root.gradeTab === "lut"
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: 8
            RowLayout {
                spacing: 8
                ComboBox {
                    id: lutChoice
                    Layout.preferredWidth: 200
                    font.pixelSize: 11
                    focusPolicy: Qt.TabFocus
                    Keys.onShortcutOverride: (event) => event.accepted = root.sliderKeys.includes(event.key) || event.key === Qt.Key_Space
                    textRole: "name"
                    model: [{ id: 0, name: "No LUT" }].concat(session.luts)
                    currentIndex: Math.max(0, model.findIndex(l => l.id === (session.info.lut || 0)))
                    onActivated: (index) => session.setClipLut(model[index].id, lutAmount.slider.value)
                }
                OmaButton { text: "Load LUT…"; tip: "A 3D LUT in the .cube format"; onClicked: lutDialog.open() }
            }
            DrawerSlider {
                id: lutAmount
                Layout.maximumWidth: 280
                enabled: (session.info.lut || 0) > 0
                label: "AMOUNT"
                readout: Math.round(lutAmount.slider.value * 100) + "%"
                onCommitted: session.setClipLut(session.info.lut || 0, lutAmount.slider.value)
            }
        }
    }
    Binding { target: lutAmount.slider; property: "value"; value: session.info.lutAmount === undefined ? 1 : session.info.lutAmount; when: !lutAmount.slider.pressed }
    Binding { target: exposureControl.slider; property: "from"; value: -2 }
    Binding { target: exposureControl.slider; property: "to"; value: 2 }
    Binding { target: exposureControl.slider; property: "value"; value: session.info.exposure || 0; when: !exposureControl.slider.pressed }
    Binding { target: contrastControl.slider; property: "from"; value: -1 }
    Binding { target: contrastControl.slider; property: "value"; value: session.info.contrast || 0; when: !contrastControl.slider.pressed }
    Binding { target: saturationControl.slider; property: "from"; value: -1 }
    Binding { target: saturationControl.slider; property: "value"; value: session.info.saturation || 0; when: !saturationControl.slider.pressed }
    Binding { target: temperatureControl.slider; property: "from"; value: -1 }
    Binding { target: temperatureControl.slider; property: "value"; value: session.info.temperature || 0; when: !temperatureControl.slider.pressed }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: root.line }
}

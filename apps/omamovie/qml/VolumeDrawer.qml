import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Shapes
import OmaMovie

// Volume drawer (ui-design §6): volume, fades and mute of the selected clip; More adds processing.
// Reads the window's ids (root, actions, session) from the document that places it.
Rectangle {
    id: volumeDrawer
    Layout.fillWidth: true
    Layout.preferredHeight: root.volumeMore ? 124 : 64
    visible: !root.viewerOnly && root.drawer === "volume" && actions.volume.enabled
    color: colors.dark_background
    function commit() {
        session.setClipAudio(root.gainOf(gainControl.slider.value), fadeInControl.slider.value,
                             fadeOutControl.slider.value, !!session.info.muted)
    }
    function commitEq() {
        session.setClipEq(lowControl.slider.value, midControl.slider.value, highControl.slider.value)
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 6
        anchors.bottomMargin: 6
        spacing: 6
        RowLayout {
            Layout.fillWidth: true
            spacing: 18
            DrawerSlider {
                id: gainControl
                label: "VOLUME"
                readout: root.decibels(root.gainOf(gainControl.slider.value))
                onCommitted: volumeDrawer.commit()
            }
            OmaButton {
                objectName: "volumeKey"
                iconName: "keyframe"
                iconFilled: !!session.motion.gainKeyHere
                selected: !!session.motion.gainKeyHere
                tip: session.motion.gainKeys > 0
                     ? session.motion.gainKeys + " volume keys: the slider sets the one at the playhead"
                     : "Animate volume: keep this level at the playhead"
                onClicked: session.toggleVolumeKey()
            }
            DrawerSlider {
                id: fadeInControl
                label: "FADE IN"
                readout: fadeInControl.slider.value.toFixed(2) + " s"
                onCommitted: volumeDrawer.commit()
            }
            DrawerSlider {
                id: fadeOutControl
                label: "FADE OUT"
                readout: fadeOutControl.slider.value.toFixed(2) + " s"
                onCommitted: volumeDrawer.commit()
            }
            OmaButton {
                text: session.info.muted ? "Unmute" : "Mute"
                selected: !!session.info.muted
                onClicked: session.setClipAudio(root.gainOf(gainControl.slider.value), fadeInControl.slider.value,
                                                fadeOutControl.slider.value, !session.info.muted)
            }
            OmaButton {
                text: "More"
                selected: root.volumeMore
                tip: "Equalizer, noise reduction, normalize"
                onClicked: root.volumeMore = !root.volumeMore
            }
        }
        RowLayout {
            Layout.fillWidth: true
            visible: root.volumeMore
            spacing: 18
            ColumnLayout {
                spacing: 2
                UiText { text: "EQUALIZER"; color: root.muted; font.pixelSize: 10; font.bold: true }
                ComboBox {
                    id: eqPreset
                    Layout.preferredWidth: 140
                    font.pixelSize: 11
                    focusPolicy: Qt.TabFocus
                    Keys.onShortcutOverride: (event) => event.accepted = root.sliderKeys.includes(event.key) || event.key === Qt.Key_Space
                    model: root.eqPresets.map(p => p.name).concat(["Custom"])
                    currentIndex: root.eqPresetIndex()
                    onActivated: (index) => {
                        if (index < root.eqPresets.length) {
                            const b = root.eqPresets[index].bands
                            session.setClipEq(b[0], b[1], b[2])
                        }
                    }
                }
            }
            DrawerSlider {
                id: lowControl
                label: "LOW"
                readout: lowControl.slider.value.toFixed(1) + " dB"
                onCommitted: volumeDrawer.commitEq()
            }
            DrawerSlider {
                id: midControl
                label: "MID"
                readout: midControl.slider.value.toFixed(1) + " dB"
                onCommitted: volumeDrawer.commitEq()
            }
            DrawerSlider {
                id: highControl
                label: "HIGH"
                readout: highControl.slider.value.toFixed(1) + " dB"
                onCommitted: volumeDrawer.commitEq()
            }
            DrawerSlider {
                id: noiseControl
                label: "NOISE REDUCTION"
                readout: noiseControl.slider.value > 0 ? Math.round(noiseControl.slider.value * 100) + "%" : "Off"
                onCommitted: session.setClipNoise(noiseControl.slider.value)
            }
            OmaButton {
                text: "Normalize"
                tip: "Raise or lower the volume so the loudest peak reaches −1 dB"
                onClicked: session.normalizeClip()
            }
        }
    }
    // The sliders follow the selected clip, except while one is being dragged.
    Binding { target: gainControl.slider; property: "from"; value: root.volumeFloorDb }
    Binding { target: gainControl.slider; property: "to"; value: 12 }
    Binding { target: gainControl.slider; property: "value"; value: root.dbOf(session.motion.gain === undefined ? (session.info.gain || 0) : session.motion.gain); when: !gainControl.slider.pressed }
    Binding { target: fadeInControl.slider; property: "to"; value: Math.max(0.01, session.info.clipDuration || 0) }
    Binding { target: fadeInControl.slider; property: "value"; value: session.info.fadeIn || 0; when: !fadeInControl.slider.pressed }
    Binding { target: fadeOutControl.slider; property: "to"; value: Math.max(0.01, session.info.clipDuration || 0) }
    Binding { target: fadeOutControl.slider; property: "value"; value: session.info.fadeOut || 0; when: !fadeOutControl.slider.pressed }
    Binding { target: lowControl.slider; property: "from"; value: -12 }
    Binding { target: lowControl.slider; property: "to"; value: 12 }
    Binding { target: lowControl.slider; property: "stepSize"; value: 0.5 }
    Binding { target: lowControl.slider; property: "value"; value: session.info.eqLow || 0; when: !lowControl.slider.pressed }
    Binding { target: midControl.slider; property: "from"; value: -12 }
    Binding { target: midControl.slider; property: "to"; value: 12 }
    Binding { target: midControl.slider; property: "stepSize"; value: 0.5 }
    Binding { target: midControl.slider; property: "value"; value: session.info.eqMid || 0; when: !midControl.slider.pressed }
    Binding { target: highControl.slider; property: "from"; value: -12 }
    Binding { target: highControl.slider; property: "to"; value: 12 }
    Binding { target: highControl.slider; property: "stepSize"; value: 0.5 }
    Binding { target: highControl.slider; property: "value"; value: session.info.eqHigh || 0; when: !highControl.slider.pressed }
    Binding { target: noiseControl.slider; property: "value"; value: session.info.noise || 0; when: !noiseControl.slider.pressed }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: root.line }
}

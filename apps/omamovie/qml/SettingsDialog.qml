import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// Settings (implementation plan, M6): user-facing defaults kept by AppSettings (C++), outside
// any project, plus what the app detected. Unavailable options say why instead of hiding.
Popup {
    id: settingsDialog
    objectName: "settingsDialog"
    modal: true
    focus: true
    padding: 0
    width: Math.min(640, parent.width - 32)
    height: Math.min(720, parent.height - 48)
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onOpened: { appSettings.refreshSystem(); closeButton.forceActiveFocus() }

    property var commandPalette // opened by "Shortcuts…"
    readonly property var system: appSettings.system
    readonly property bool scaleFromEnv: (system.overrides || "").indexOf("OMA_PREVIEW_SCALE") >= 0
    readonly property bool decodeFromEnv: (system.overrides || "").indexOf("OMA_PREVIEW_HARDWARE") >= 0

    background: Rectangle { color: colors.background; border.color: colors.lighter_background; radius: 8 }

    component Heading: UiText {
        Layout.topMargin: 14
        color: colors.dark_foreground
        font.pixelSize: 11
        font.bold: true
        font.letterSpacing: 1
    }
    component Note: UiText {
        Layout.fillWidth: true
        color: colors.dark_foreground
        font.pixelSize: 11
        wrapMode: Text.Wrap
    }
    // One choice of a setting; Tab reaches the group, the arrows move within it.
    component Choice: RadioButton {
        id: choice
        property string value
        property string current
        signal chosen(string value)
        checked: value === current
        onClicked: chosen(value)
        contentItem: UiText {
            leftPadding: choice.indicator.width + choice.spacing
            text: choice.text
            color: choice.enabled ? colors.foreground : colors.dark_foreground
            font.pixelSize: 12
            verticalAlignment: Text.AlignVCenter
        }
    }

    FolderDialog {
        id: recordingsPicker
        title: "Recordings folder"
        onAccepted: appSettings.recordingsFolder = selectedFolder.toString().replace(/^file:\/\//, "")
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 14
            UiText { text: "Settings"; color: colors.foreground; font.pixelSize: 16; font.bold: true }
            Item { Layout.fillWidth: true }
            OmaButton { objectName: "resetSettings"; text: "Reset all"; onClicked: appSettings.resetAll() }
            OmaButton { id: closeButton; text: "Done"; primary: true; onClicked: settingsDialog.close() }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: colors.lighter_background }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            ColumnLayout {
                x: 18
                width: parent.width - 36
                spacing: 4

                Heading { text: "PREVIEW QUALITY" }
                Note { text: "How sharp the viewer is while editing. Export is never affected." }
                ColumnLayout {
                    enabled: !settingsDialog.scaleFromEnv
                    spacing: 0
                    Choice { objectName: "qualityAuto"; text: "Automatic: lower it only when playback drops frames"; value: "auto"; current: appSettings.previewQuality; onChosen: v => appSettings.previewQuality = v }
                    Choice { text: "Full"; value: "full"; current: appSettings.previewQuality; onChosen: v => appSettings.previewQuality = v }
                    Choice { objectName: "qualityHalf"; text: "Half"; value: "half"; current: appSettings.previewQuality; onChosen: v => appSettings.previewQuality = v }
                    Choice { text: "Quarter"; value: "quarter"; current: appSettings.previewQuality; onChosen: v => appSettings.previewQuality = v }
                }
                Note { visible: settingsDialog.scaleFromEnv; text: "Set by OMA_PREVIEW_SCALE for this run."; color: colors.yellow }

                Heading { text: "VIDEO DECODING" }
                ColumnLayout {
                    enabled: !settingsDialog.decodeFromEnv
                    spacing: 0
                    Choice { text: "Automatic: hardware where OmaMovie has validated it"; value: "auto"; current: appSettings.decodePath; onChosen: v => appSettings.decodePath = v }
                    Choice { text: "Hardware"; value: "hardware"; current: appSettings.decodePath; onChosen: v => appSettings.decodePath = v }
                    Choice { objectName: "decodeSoftware"; text: "Software"; value: "software"; current: appSettings.decodePath; onChosen: v => appSettings.decodePath = v }
                }
                Note { objectName: "decodeStatus"; text: "Now: " + (settingsDialog.system.decode || "") }
                Note {
                    objectName: "restartNote"
                    visible: appSettings.decodePath !== appSettings.startupDecodePath
                    text: "Takes effect when OmaMovie restarts."
                    color: colors.yellow
                }

                Heading { text: "RECORDINGS" }
                Note { text: session.recordingsFolder + (appSettings.recordingsFolder === "" ? " (Omarchy's folder)" : "") }
                RowLayout {
                    OmaButton { text: "Choose folder…"; onClicked: recordingsPicker.open() }
                    OmaButton {
                        text: "Use Omarchy's folder"
                        enabled: appSettings.recordingsFolder !== ""
                        onClicked: appSettings.recordingsFolder = ""
                    }
                }

                Heading { text: "AUDIO OUTPUT" }
                Note { text: (settingsDialog.system.audio || "") + ". Choose the device in Omarchy's audio settings." }

                Heading { text: "APPEARANCE" }
                Note { text: "Follows the Omarchy theme and font" + (settingsDialog.system.font ? " (" + settingsDialog.system.font + ")" : "") + ". The viewer never takes theme colours." }

                Heading { text: "KEYBOARD" }
                RowLayout {
                    OmaButton {
                        text: "Shortcuts…"
                        onClicked: { settingsDialog.close(); if (settingsDialog.commandPalette) settingsDialog.commandPalette.open() }
                    }
                    Note { text: "Every action is listed in the command palette (Ctrl+K); Ctrl+Enter changes its shortcut." }
                }

                Heading { text: "CACHE" }
                Note { objectName: "cacheNote"; text: "Thumbnails and waveforms are kept in " + (settingsDialog.system.cache || "the cache folder") + ", oldest first out past the limit. Clearing it loses nothing but the time to rebuild them. The folder follows XDG_CACHE_HOME." }
                RowLayout {
                    spacing: 0
                    UiText { text: "Limit for each:"; color: colors.foreground; font.pixelSize: 12; rightPadding: 8 }
                    Choice { text: "256 MB"; value: "256"; current: appSettings.cacheLimit; onChosen: v => appSettings.cacheLimit = v }
                    Choice { text: "512 MB"; value: "512"; current: appSettings.cacheLimit; onChosen: v => appSettings.cacheLimit = v }
                    Choice { text: "1 GB"; value: "1024"; current: appSettings.cacheLimit; onChosen: v => appSettings.cacheLimit = v }
                    Choice { text: "4 GB"; value: "4096"; current: appSettings.cacheLimit; onChosen: v => appSettings.cacheLimit = v }
                }
                OmaButton { objectName: "clearCache"; text: "Clear cache"; onClicked: session.clearCache() }

                Heading { text: "EXPORT" }
                Note { text: "MP4: H.264 video at the project's size and frame rate, BT.709 colour, AAC stereo sound at 48 kHz. The encoder changes speed and file size, never these." }
                ColumnLayout {
                    spacing: 0
                    Choice { text: "Automatic: the GPU's encoder where OmaMovie has validated it (Intel), else software"; value: "auto"; current: appSettings.exportEncoder; onChosen: v => appSettings.exportEncoder = v }
                    Choice { text: "GPU (VA-API): faster, larger files; falls back to software if it cannot start"; value: "hardware"; current: appSettings.exportEncoder; onChosen: v => appSettings.exportEncoder = v }
                    Choice { objectName: "exportSoftware"; text: "Software (x264): slower, smaller files"; value: "software"; current: appSettings.exportEncoder; onChosen: v => appSettings.exportEncoder = v }
                }

                Heading { text: "THIS COMPUTER" }
                Note { text: "GPU: " + (settingsDialog.system.gpu || "unknown") + (settingsDialog.system.driver ? " · " + settingsDialog.system.driver : "") }
                Note { visible: !!settingsDialog.system.overrides; text: "Environment overrides: " + (settingsDialog.system.overrides || "") }
                Item { Layout.preferredHeight: 14 }
            }
        }
    }
}

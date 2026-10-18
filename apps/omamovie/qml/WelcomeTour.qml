import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Presentation only. The tour neither creates a project nor changes editor history.
Popup {
    id: tour
    objectName: "welcomeTour"
    property var actionSet
    property var settings
    property var hostWindow
    property Item previousFocus: null
    property int step: 0
    property int resumeStep: 0
    signal commandPaletteRequested()
    signal demoRequested()
    function label(action) { return action.text + (action.keys ? " (" + action.keys + ")" : "") }
    readonly property var pages: actionSet ? [
        {
            title: "Find any command",
            where: "Command Palette · available on Projects and in the editor",
            doThis: "Press " + (actionSet.commandPalette.keys || "your Command Palette shortcut") +
                    ", type a command such as Import, then press Enter. Try the button below now.",
            result: "The palette shows action names and current shortcuts. Press " +
                    (actionSet.welcomeTour.keys || "your Welcome tour shortcut") +
                    " to reopen this guide."
        },
        {
            title: "Create a project and import",
            where: "Projects → New project → top bar Import → Library",
            doThis: "Choose New project, then " + label(actionSet.importMedia) +
                    " and select a video, picture or sound file. Or open the bundled demo below and try its three clips.",
            result: "Imported files appear in the Library; the first one also starts your sequence. " +
                    "The demo opens as an unsaved copy, so your changes cannot replace it. " +
                    "Use " + label(actionSet.welcomeTour) + " after opening it to continue this guide."
        },
        {
            title: "Mark the part you want",
            where: "Library tile → Source viewer",
            doThis: "In the demo, select 03-finale in the Library; with your own media, select any tile. Choose " +
                    label(actionSet.openSource) + ". Play or scrub, then " +
                    label(actionSet.markIn) + " at the beginning and " + label(actionSet.markOut) +
                    " at the end. Choose Back to sequence when ready.",
            result: "The tile shows a yellow Range label. The Source viewer has its own playhead; " +
                    "the sequence playhead has not moved."
        },
        {
            title: "Put the range in your movie",
            where: "Library tile → Sequence timeline",
            doThis: "The demo already has three shots. To put another selected item at the end, use " +
                    label(actionSet.append) + ". " +
                    "Select a Library tile, move the sequence playhead and choose " + label(actionSet.insert) + ".",
            result: label(actionSet.insert) + " makes room and moves later storyline clips. " +
                    label(actionSet.overwrite) + " replaces at the playhead without moving later clips. " +
                    label(actionSet.connect) + " puts a cutaway on another lane; it follows its storyline anchor."
        },
        {
            title: "Cut and correct",
            where: "Sequence timeline → selected clip",
            doThis: "Click a clip, move the playhead, then choose " + label(actionSet.split) +
                    ". Drag a clip edge to trim it. Use " + label(actionSet.remove) + " to close a gap, or " +
                    label(actionSet.lift) + " to leave a gap.",
            result: "The storyline is the main row. Connected clips follow their anchor, and deleting " +
                    "that anchor removes them. Use " + label(actionSet.undo) + " if the result surprises you."
        },
        {
            title: "Check, save and export",
            where: "Viewer and top bar",
            doThis: "Return to the sequence if Source is open. Use " + label(actionSet.playPause) +
                    " to check the edit, then " + label(actionSet.save) + " to save the project and " +
                    label(actionSet.exportMovie) + " to make a movie file.",
            result: "The saved project stays editable; Export makes a separate movie. To try more: put the playhead " +
                    "where text should start and choose " + label(actionSet.addTitle) + "; select a clip with sound " +
                    "and choose " + label(actionSet.volume) + "; click the join marker between two touching " +
                    "storyline clips for a Cross dissolve. Find title and volume again with " +
                    label(actionSet.commandPalette) + "."
        }
    ] : []

    function restart() {
        previousFocus = hostWindow.activeFocusItem
        step = resumeStep
        resumeStep = 0
        open()
    }
    function finish() {
        settings.tourCompleted = true
        close()
    }

    modal: true
    focus: true
    padding: 0
    width: Math.min(600, parent.width - 32)
    height: Math.min(430, parent.height - 32)
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    closePolicy: Popup.CloseOnEscape
    onOpened: nextButton.forceActiveFocus()
    onClosed: {
        if (previousFocus && previousFocus.visible && previousFocus.enabled) previousFocus.forceActiveFocus()
        else parent.forceActiveFocus()
        previousFocus = null
    }
    background: Rectangle { color: colors.background; border.color: colors.lighter_background; radius: 8 }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12
        UiText {
            Layout.fillWidth: true
            text: "YOUR FIRST EDIT · " + (tour.step + 1) + " / " + tour.pages.length
            color: colors.dark_foreground
            font.pixelSize: Math.round(11 * appSettings.textScale)
            font.bold: true
        }
        UiText {
            Layout.fillWidth: true
            text: tour.pages.length ? tour.pages[tour.step].title : ""
            color: colors.foreground
            font.pixelSize: Math.round(18 * appSettings.textScale)
            font.bold: true
            wrapMode: Text.Wrap
        }
        ScrollView {
            id: bodyScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            ColumnLayout {
                width: bodyScroll.availableWidth
                spacing: 8
                UiText {
                    Layout.fillWidth: true
                    text: tour.pages.length ? tour.pages[tour.step].where : ""
                    color: colors.accent
                    font.pixelSize: Math.round(12 * appSettings.textScale)
                    font.bold: true
                    wrapMode: Text.Wrap
                }
                UiText {
                    Layout.fillWidth: true
                    text: "DO THIS"
                    color: colors.dark_foreground
                    font.pixelSize: Math.round(11 * appSettings.textScale)
                    font.bold: true
                }
                UiText {
                    Layout.fillWidth: true
                    text: tour.pages.length ? tour.pages[tour.step].doThis : ""
                    color: colors.foreground
                    font.pixelSize: Math.round(13 * appSettings.textScale)
                    wrapMode: Text.Wrap
                }
                UiText {
                    Layout.fillWidth: true
                    text: "WHAT YOU'LL SEE"
                    color: colors.dark_foreground
                    font.pixelSize: Math.round(11 * appSettings.textScale)
                    font.bold: true
                }
                UiText {
                    Layout.fillWidth: true
                    text: tour.pages.length ? tour.pages[tour.step].result : ""
                    color: colors.foreground
                    font.pixelSize: Math.round(13 * appSettings.textScale)
                    wrapMode: Text.Wrap
                }
                OmaButton {
                    objectName: "tourOpenPalette"
                    text: "Open Command Palette"
                    primary: true
                    visible: tour.step === 0
                    Layout.alignment: Qt.AlignLeft
                    onClicked: { tour.resumeStep = 1; tour.close(); tour.commandPaletteRequested() }
                }
                OmaButton {
                    objectName: "tourOpenDemo"
                    text: "Explore demo edit"
                    primary: true
                    visible: tour.step === 1 && actionSet.openDemo.enabled
                    Layout.alignment: Qt.AlignLeft
                    onClicked: { tour.resumeStep = 2; tour.close(); tour.demoRequested() }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 4
            OmaButton { objectName: "tourSkip"; text: "Skip for now"; onClicked: tour.close() }
            OmaButton { objectName: "tourDismiss"; text: "Don't show again"; onClicked: tour.finish() }
            Item { Layout.fillWidth: true }
            OmaButton { objectName: "tourBack"; text: "Back"; enabled: tour.step > 0; onClicked: --tour.step }
            OmaButton {
                id: nextButton
                objectName: "tourNext"
                text: tour.step === tour.pages.length - 1 ? "Start editing" : "Next"
                primary: true
                onClicked: tour.step === tour.pages.length - 1 ? tour.finish() : ++tour.step
            }
        }
    }
}

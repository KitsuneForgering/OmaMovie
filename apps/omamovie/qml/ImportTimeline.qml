import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import OmaMovie

// Importing a timeline (M9: OpenTimelineIO, MLT, Kdenlive, FCPXML, EDL): choose the file, then,
// when the import has notes (CLAUDE.md §16.4: what was approximated, replaced or not carried
// over), list them.
Item {
    id: importer
    function choose() { fileDialog.open() }
    FileDialog {
        id: fileDialog
        title: "Import a timeline"
        nameFilters: ["Timelines (*.otio *.mlt *.kdenlive *.fcpxml *.edl)", "OpenTimelineIO (*.otio)",
                      "MLT and Kdenlive projects (*.mlt *.kdenlive)", "Final Cut Pro XML (*.fcpxml)",
                      "CMX 3600 EDL (*.edl)"]
        // An EDL names no frame rate: ask for the one its timecodes count in.
        onAccepted: selectedFile.toString().toLowerCase().endsWith(".edl")
                    ? edlRate.open() : session.importTimeline(selectedFile)
    }
    Dialog {
        id: edlRate
        objectName: "edlRate"
        title: "EDL frame rate"
        modal: true
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: session.importTimeline(fileDialog.selectedFile, rates.currentValue)
        ComboBox {
            id: rates
            textRole: "text"
            valueRole: "value"
            currentIndex: 4
            model: [{ text: "23.976", value: "24000/1001" }, { text: "24", value: "24" },
                    { text: "25", value: "25" }, { text: "29.97", value: "30000/1001" },
                    { text: "30", value: "30" }, { text: "50", value: "50" },
                    { text: "59.94", value: "60000/1001" }, { text: "60", value: "60" }]
        }
    }
    Connections {
        target: session
        function onImportReportChanged() { if (session.importReport.length > 0) report.open() }
    }
    Dialog {
        id: report
        objectName: "importReport"
        title: "Import report"
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(640, root.width - 48)
        height: Math.min(420, root.height - 48)
        standardButtons: Dialog.Close
        ListView {
            anchors.fill: parent
            clip: true
            model: session.importReport
            delegate: UiText {
                required property string modelData
                width: ListView.view.width
                text: "• " + modelData
                color: root.fg
                wrapMode: Text.Wrap
                font.pixelSize: Math.round(12 * appSettings.textScale)
            }
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import OmaMovie

// Importing a timeline (M9: OpenTimelineIO, MLT, Kdenlive): choose the file, then, when the import has notes
// (CLAUDE.md §16.4: what was approximated, replaced or not carried over), list them.
Item {
    id: importer
    function choose() { fileDialog.open() }
    FileDialog {
        id: fileDialog
        title: "Import a timeline"
        // EDL is left out here: it names no frame rate, which this dialog does not ask for yet.
        nameFilters: ["Timelines (*.otio *.mlt *.kdenlive)", "OpenTimelineIO (*.otio)",
                      "MLT and Kdenlive projects (*.mlt *.kdenlive)"]
        onAccepted: session.importTimeline(selectedFile)
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

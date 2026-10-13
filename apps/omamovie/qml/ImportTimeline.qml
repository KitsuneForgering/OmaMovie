import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import OmaMovie

// Importing an OpenTimelineIO file (M9): choose the file, then, when the import has notes
// (CLAUDE.md §16.4: what was approximated, replaced or not carried over), list them.
Item {
    id: importer
    function choose() { fileDialog.open() }
    FileDialog {
        id: fileDialog
        title: "Import OpenTimelineIO"
        nameFilters: ["OpenTimelineIO (*.otio)"]
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
                font.pixelSize: 12
            }
        }
    }
}

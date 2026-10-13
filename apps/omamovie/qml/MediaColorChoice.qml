import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import OmaMovie

// One of the media's color interpretation choices (ADR-0019) in the Info drawer: the matrix or
// the range its YUV is read with. A change is one undoable edit of the media, for every clip of it.
ColumnLayout {
    id: choice
    required property bool range // false: the matrix
    visible: !!session.info.hasVideo && session.info.colorMatrix !== undefined
    spacing: 2
    UiText {
        text: choice.range ? "RANGE" : "READ COLOR AS"
        color: root.muted
        font.pixelSize: 11
        font.bold: true
    }
    ComboBox {
        objectName: choice.range ? "mediaColorRange" : "mediaColorMatrix"
        Layout.preferredWidth: 150
        font.pixelSize: 11
        focusPolicy: Qt.TabFocus
        model: choice.range ? ["As the file says", "Limited (TV)", "Full (PC)"]
                            : ["As the file says", "BT.601 (SD)", "BT.709 (HD)", "BT.2020"]
        currentIndex: (choice.range ? session.info.colorRange : session.info.colorMatrix) || 0
        onActivated: (index) => choice.range ? session.setMediaColor(session.info.colorMatrix || 0, index)
                                             : session.setMediaColor(index, session.info.colorRange || 0)
    }
}

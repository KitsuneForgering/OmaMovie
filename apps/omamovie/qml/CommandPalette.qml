import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The command palette (ui-design §8.2): every action, searchable by name, run with Enter.
// Ctrl+Enter (or the keys button) records a new shortcut for the highlighted action;
// search, conflicts and persistence live in the C++ ActionRegistry.
Popup {
    id: palette
    objectName: "commandPalette"
    modal: true
    focus: true
    padding: 8
    closePolicy: Popup.CloseOnPressOutside
    width: Math.min(560, parent.width - 32)
    height: Math.min(440, parent.height - 64)
    x: (parent.width - width) / 2
    y: 48

    property var rows: []
    property string recording: "" // the action whose shortcut is being recorded
    property string message: ""

    function refresh() {
        rows = actionRegistry.search(query.text)
        list.currentIndex = rows.length > 0 ? 0 : -1
    }
    function run(index) {
        if (index < 0 || index >= rows.length || !rows[index].enabled) return
        const id = rows[index].id
        close()
        actionRegistry.trigger(id)
    }
    function record(index) {
        if (index < 0 || index >= rows.length) return
        list.currentIndex = index
        recording = rows[index].id
        message = "Press the new shortcut · Backspace removes it · Escape cancels"
        recorder.forceActiveFocus()
    }
    function finish(keys) {
        const refused = actionRegistry.remap(recording, keys)
        message = refused
        if (refused !== "") return // stay in recording mode so another try is one key press away
        recording = ""
        refresh()
        query.forceActiveFocus()
    }

    onOpened: { recording = ""; message = ""; query.text = ""; refresh(); query.forceActiveFocus() }

    background: Rectangle { color: colors.background; border.color: colors.lighter_background; radius: 8 }

    // Captures the next key combination. Accepting ShortcutOverride keeps the editor's own
    // shortcuts from firing while a new one is being typed.
    Item {
        id: recorder
        Keys.onShortcutOverride: event => event.accepted = true
        Keys.onPressed: event => {
            event.accepted = true
            if (event.key === Qt.Key_Escape && event.modifiers === Qt.NoModifier) {
                palette.recording = ""; palette.message = ""; query.forceActiveFocus(); return
            }
            if (event.key === Qt.Key_Backspace && event.modifiers === Qt.NoModifier) { palette.finish(""); return }
            const keys = actionRegistry.sequenceOf(event.key, event.modifiers)
            if (keys !== "") palette.finish(keys)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 6
        TextField {
            id: query
            objectName: "paletteQuery"
            Layout.fillWidth: true
            placeholderText: "Search actions"
            color: colors.foreground
            onTextChanged: palette.refresh()
            // Navigation keys belong to the list, not to the editor's shortcuts.
            Keys.onShortcutOverride: event => {
                event.accepted = [Qt.Key_Up, Qt.Key_Down, Qt.Key_Return, Qt.Key_Enter, Qt.Key_Escape,
                                  Qt.Key_PageUp, Qt.Key_PageDown].includes(event.key)
            }
            Keys.onUpPressed: list.decrementCurrentIndex()
            Keys.onDownPressed: list.incrementCurrentIndex()
            Keys.onEscapePressed: palette.close()
            Keys.onReturnPressed: event => event.modifiers & Qt.ControlModifier ? palette.record(list.currentIndex)
                                                                                : palette.run(list.currentIndex)
            Keys.onEnterPressed: event => event.modifiers & Qt.ControlModifier ? palette.record(list.currentIndex)
                                                                               : palette.run(list.currentIndex)
        }
        UiText {
            Layout.fillWidth: true
            visible: palette.message !== ""
            text: palette.message
            color: palette.recording !== "" && palette.message.startsWith("Press") ? colors.dark_foreground : colors.yellow
            font.pixelSize: 11
            wrapMode: Text.Wrap
        }
        ListView {
            id: list
            objectName: "paletteList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: palette.rows
            highlightMoveDuration: 0
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                id: row
                required property var modelData
                required property int index
                width: ListView.view.width
                height: 32
                highlighted: ListView.isCurrentItem
                onClicked: palette.run(index)
                contentItem: RowLayout {
                    spacing: 8
                    UiText {
                        Layout.fillWidth: true
                        text: row.modelData.text
                        color: row.modelData.enabled ? colors.foreground : colors.dark_foreground
                        elide: Text.ElideRight
                        font.pixelSize: 13
                    }
                    Button {
                        flat: true
                        Layout.preferredHeight: 24
                        text: palette.recording === row.modelData.id ? "…"
                              : row.modelData.keys !== "" ? row.modelData.keys : "Add shortcut"
                        font.pixelSize: 11
                        font.bold: row.modelData.keys !== row.modelData.defaultKeys
                        focusPolicy: Qt.NoFocus
                        onClicked: palette.record(row.index)
                        ToolTip.visible: hovered
                        ToolTip.text: "Change the shortcut (Ctrl+Enter)" +
                                      (row.modelData.keys !== row.modelData.defaultKeys
                                       ? " · default: " + (row.modelData.defaultKeys || "none") : "")
                    }
                    Button {
                        flat: true
                        visible: row.modelData.keys !== row.modelData.defaultKeys
                        Layout.preferredHeight: 24
                        text: "Reset"
                        font.pixelSize: 11
                        focusPolicy: Qt.NoFocus
                        onClicked: { actionRegistry.reset(row.modelData.id); palette.refresh() }
                    }
                }
            }
        }
    }
}

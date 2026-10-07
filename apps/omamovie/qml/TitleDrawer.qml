import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Title drawer (ui-design §6, ADR-0015): the selected title's text, size, colour and placement.
// Text commits when typing pauses or the field loses focus, the rest on release or click; each
// change is one undoable edit. Reads the window's ids (root, actions, session) from the document
// that places it.
Rectangle {
    id: titleDrawer
    objectName: "titleDrawer"
    Layout.fillWidth: true
    Layout.preferredHeight: 104
    visible: !root.viewerOnly && root.drawer === "title" && !!session.info.isTitle
    color: colors.dark_background

    // Picture colours, not theme colours: a title belongs to the movie (ui-design §5).
    readonly property var swatches: ["#ffffff", "#000000", "#ffd400", "#ff4d4d", "#4da6ff", "#4dff88"]
    readonly property var placements: ["Lower third", "Centre", "Top"]

    function commit(text) {
        session.setClipTitle(text, sizeSlider.value, session.info.titleColor || "#ffffff", session.info.titlePlacement || 0)
    }

    Timer { id: typing; interval: 600; onTriggered: titleDrawer.commit(textField.text) }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 16
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            TextArea {
                id: textField
                objectName: "titleText"
                placeholderText: "Title text"
                wrapMode: TextEdit.Wrap
                color: colors.foreground
                font.pixelSize: 13
                // Follows the selected title unless it is being typed in.
                Binding on text { value: session.info.titleText || ""; when: !textField.activeFocus }
                onTextChanged: if (activeFocus) typing.restart()
                onActiveFocusChanged: if (!activeFocus && typing.running) { typing.stop(); titleDrawer.commit(text) }
                // Typing belongs to the field, not to the editor's single-key shortcuts.
                Keys.onShortcutOverride: event => event.accepted = event.key !== Qt.Key_Escape
            }
        }
        ColumnLayout {
            spacing: 6
            Layout.preferredWidth: 260
            RowLayout {
                UiText { text: "SIZE"; color: root.muted; font.pixelSize: 10; font.bold: true }
                Slider {
                    id: sizeSlider
                    objectName: "titleSize"
                    Layout.fillWidth: true
                    from: 0.03
                    to: 0.25
                    focusPolicy: Qt.TabFocus
                    Binding on value { value: session.info.titleSize || 0.08; when: !sizeSlider.pressed }
                    onPressedChanged: if (!pressed) titleDrawer.commit(session.info.titleText || "")
                }
            }
            RowLayout {
                spacing: 6
                Repeater {
                    model: titleDrawer.swatches
                    delegate: Rectangle {
                        required property string modelData
                        width: 20; height: 20; radius: 10
                        color: modelData
                        border.width: (session.info.titleColor || "") === modelData ? 2 : 1
                        border.color: (session.info.titleColor || "") === modelData ? root.accent : root.line
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: session.setClipTitle(session.info.titleText || "", session.info.titleSize || 0.08,
                                                            parent.modelData, session.info.titlePlacement || 0)
                        }
                    }
                }
            }
            RowLayout {
                spacing: 4
                Repeater {
                    model: titleDrawer.placements
                    delegate: OmaButton {
                        required property string modelData
                        required property int index
                        text: modelData
                        selected: (session.info.titlePlacement || 0) === index
                        onClicked: session.setClipTitle(session.info.titleText || "", session.info.titleSize || 0.08,
                                                        session.info.titleColor || "#ffffff", index)
                    }
                }
            }
        }
    }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: root.line }
}

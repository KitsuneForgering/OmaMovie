import QtQuick
import QtQuick.Layouts

ColumnLayout {
    property string label
    property string value
    spacing: 1
    UiText { text: parent.label; color: colors.dark_foreground; font.pixelSize: 9; font.bold: true }
    UiText { text: parent.value; color: colors.foreground; font.pixelSize: 11; elide: Text.ElideMiddle; Layout.maximumWidth: 360 }
}

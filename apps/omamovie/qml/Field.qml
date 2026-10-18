import QtQuick
import QtQuick.Layouts

ColumnLayout {
    property string label
    property string value
    spacing: 1
    UiText { text: parent.label; color: colors.dark_foreground; font.pixelSize: Math.round(9 * appSettings.textScale); font.bold: true }
    UiText { text: parent.value; color: colors.foreground; font.pixelSize: Math.round(11 * appSettings.textScale); elide: Text.ElideMiddle; Layout.maximumWidth: 360 }
}

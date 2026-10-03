import QtQuick
import QtQuick.Controls

// A flat button showing an icon, a label or both; tooltips carry the name and shortcut.
AbstractButton {
    id: control
    property string iconName
    property bool showLabel: true
    property bool primary: false
    property bool selected: false
    property bool activeDot: false
    property string tip: action ? action.text : text
    focusPolicy: Qt.NoFocus // Space and letters belong to the editor's actions
    hoverEnabled: true
    implicitHeight: 32
    implicitWidth: content.implicitWidth + (showLabel ? 22 : 14)
    ToolTip.visible: hovered && (!showLabel || iconName === "") && tip !== ""
    ToolTip.text: tip + (action && action.keys ? " (" + action.keys + ")" : "")
    ToolTip.delay: 500
    background: Rectangle {
        radius: 6
        color: control.primary && control.enabled ? colors.accent
            : control.selected ? colors.selection_background
            : control.down || (control.hovered && control.enabled) ? colors.lighter_background
            : "transparent"
    }
    contentItem: Item {
        implicitWidth: content.implicitWidth
        implicitHeight: content.implicitHeight
        Row {
            id: content
            anchors.centerIn: parent
            spacing: 6
            readonly property color tint: !control.enabled ? colors.dark_foreground
                : control.primary ? colors.background
                : control.selected ? colors.accent : colors.foreground
            Icon {
                visible: control.iconName !== ""
                name: control.iconName
                color: content.tint
                opacity: control.enabled ? 1 : 0.55
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                visible: control.showLabel && control.text !== ""
                text: control.text
                color: content.tint
                opacity: control.enabled ? 1 : 0.55
                font.pixelSize: 12
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        Rectangle { // marks an adjustment already active on the clip (ui-design §6)
            visible: control.activeDot
            width: 5; height: 5; radius: 2.5
            color: colors.accent
            anchors.right: parent.right
            anchors.top: parent.top
        }
    }
}

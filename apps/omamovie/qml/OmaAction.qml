import QtQuick.Controls

// An editor command (ui-design §8.1). `keys` is its default shortcut; Main.qml registers every
// action's keys once. A button that uses the action does not register them again, because two
// identical sequences are ambiguous in Qt and neither fires.
Action {
    property string keys
}

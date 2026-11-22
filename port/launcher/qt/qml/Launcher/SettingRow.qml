import QtQuick
import QtQuick.Layouts
import Launcher

// One row of a Group: the label (and an optional secondary line) on the left, the control on the right.
// Long texts and taller controls (native ones carry their focus-ring insets) grow the row from 40 pt.
Item {
    id: row
    property string label
    property string detail
    default property alias trailing: slot.data

    width: parent ? parent.width : implicitWidth
    implicitHeight: Math.max(40, texts.implicitHeight + 24, slot.implicitHeight)
    // VoiceOver reads a switch or pop-up by the row's label; a button keeps its own title.
    Component.onCompleted: {
        for (const control of slot.children)
            if (control.focusPolicy !== undefined && !control.text)
                control.Accessible.name = Qt.binding(() => row.label)
    }

    Rectangle {    // hairline above every row but the first, inset like the text
        visible: !row.Positioner.isFirstItem
        x: Theme.s2
        width: parent.width - 2 * Theme.s2
        height: 1
        color: Theme.separator
    }
    Column {
        id: texts
        anchors.left: parent.left
        anchors.leftMargin: Theme.s2
        anchors.right: slot.left
        anchors.rightMargin: Theme.s2
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2
        Text {
            width: parent.width
            text: row.label
            color: Theme.textPrimary
            font.pixelSize: Theme.body
            wrapMode: Text.Wrap
        }
        Text {
            width: parent.width
            visible: text !== ""
            text: row.detail
            color: Theme.textSecondary
            font.pixelSize: Theme.caption
            wrapMode: Text.Wrap
        }
    }
    RowLayout {    // centers a value and a button on one line
        id: slot
        anchors.right: parent.right
        anchors.rightMargin: Theme.s2
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.s1
    }
}

import QtQuick
import Launcher

// One row of a Group: the label (and an optional secondary line) on the left, the control on the right.
// Long texts wrap; the row grows from 40 pt to fit them.
Item {
    id: row
    property string label
    property string detail
    default property alias trailing: slot.data

    width: parent ? parent.width : implicitWidth
    implicitHeight: Math.max(40, texts.implicitHeight + 24)

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
    Row {
        id: slot
        anchors.right: parent.right
        anchors.rightMargin: Theme.s2
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.s1
    }
}

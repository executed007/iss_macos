import QtQuick
import Launcher

// A collapsed group of rows, as System Settings' disclosure: one header row (title, chevron) that opens
// the rows under it. The rows are built when it first opens. Space or Return toggles it from the keyboard.
Rectangle {
    id: box
    property string title
    property string detail
    property bool open: false
    default property Component content

    width: parent ? parent.width : implicitWidth
    implicitHeight: header.height + (open ? body.height : 0)
    radius: Theme.radius
    color: Theme.groupBg
    clip: true

    Item {
        id: header
        width: parent.width
        height: Math.max(40, label.implicitHeight + 24)
        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: box.title
        Accessible.description: box.open ? "Expanded" : "Collapsed"
        Accessible.onPressAction: box.open = !box.open
        Keys.onSpacePressed: box.open = !box.open
        Keys.onReturnPressed: box.open = !box.open
        Column {
            id: label
            anchors { left: parent.left; leftMargin: Theme.s2; right: chevron.left; rightMargin: Theme.s1
                      verticalCenter: parent.verticalCenter }
            spacing: 2
            Text {
                width: parent.width
                text: box.title
                color: Theme.textPrimary
                font.pixelSize: Theme.body
                wrapMode: Text.Wrap
            }
            Text {
                width: parent.width
                visible: text !== ""
                text: box.detail
                color: Theme.textSecondary
                font.pixelSize: Theme.caption
                wrapMode: Text.Wrap
            }
        }
        Image {
            id: chevron
            anchors { right: parent.right; rightMargin: Theme.s2; verticalCenter: parent.verticalCenter }
            width: 12; height: 12
            sourceSize: Qt.size(12, 12)
            source: "image://icon/" + (box.open ? "chevron-down" : "chevron-right") + "/" + String(Theme.textSecondary).slice(1)
        }
        Rectangle {    // the keyboard focus ring
            anchors.fill: parent
            anchors.margins: 1
            radius: Theme.radius - 1
            visible: header.activeFocus
            color: "transparent"
            border.width: 2
            border.color: Theme.accent
        }
        TapHandler { onTapped: { header.forceActiveFocus(Qt.MouseFocusReason); box.open = !box.open } }
    }
    Column {
        id: body
        y: header.height
        width: parent.width
        visible: box.open
        Rectangle { width: parent.width - 2 * Theme.s2; x: Theme.s2; height: 1; color: Theme.separator }
        Loader {
            width: parent.width
            active: box.open || status === Loader.Ready      // built on first open, then kept
            sourceComponent: box.content
        }
    }
}

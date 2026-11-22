import QtQuick
import Launcher

// The source list, as in System Settings: one Tab stop, Up and Down choose the pane, and the selected row
// turns accent-colored while the list has the keyboard focus. model: [{key, label, icon}], icon names a
// file in icons/.
Rectangle {
    id: root
    property var model: []
    property string currentKey
    signal activated(string key)

    implicitWidth: 216
    color: Theme.sidebarBg
    activeFocusOnTab: true
    Keys.onUpPressed: step(-1)
    Keys.onDownPressed: step(1)
    function step(by) {
        const i = model.findIndex(entry => entry.key === currentKey) + by
        if (i >= 0 && i < model.length)
            activated(model[i].key)
    }
    Accessible.role: Accessible.List
    Accessible.name: "Sidebar"

    Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.separator }

    Column {
        x: Theme.s1
        y: root.SafeArea.margins.top + Theme.s1    // below the traffic lights
        width: parent.width - 2 * Theme.s1
        Repeater {
            model: root.model
            Rectangle {
                id: row
                required property var modelData
                readonly property bool selected: modelData.key === root.currentKey
                readonly property bool emphasized: selected && root.activeFocus     // false in an inactive window too
                readonly property color ink: emphasized ? Theme.textOnAccent : Theme.textPrimary
                width: parent.width
                height: 28
                radius: 6
                color: emphasized ? Theme.accent : selected ? Theme.selection : "transparent"
                Image {
                    x: Theme.s1
                    anchors.verticalCenter: parent.verticalCenter
                    width: 16; height: 16
                    sourceSize: Qt.size(16, 16)     // Qt asks the provider for 16 x the screen's pixel ratio
                    source: "image://icon/" + row.modelData.icon + "/" + String(row.ink).slice(1)
                }
                Text {
                    x: 32
                    width: parent.width - x - Theme.s1
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.modelData.label
                    color: row.ink
                    font.pixelSize: Theme.body
                    elide: Text.ElideRight
                }
                TapHandler {
                    onTapped: {
                        root.forceActiveFocus(Qt.MouseFocusReason)
                        root.activated(row.modelData.key)
                    }
                }
                Accessible.role: Accessible.ListItem
                Accessible.name: modelData.label
                Accessible.selected: selected
            }
        }
    }
}

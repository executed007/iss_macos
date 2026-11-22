import QtQuick
import Launcher

// The shell: the sidebar and one pane per destination. A pane is built the first time it is shown and
// then kept (its scroll and state stay).
Rectangle {
    id: root
    color: Theme.windowBg
    implicitWidth: 1080
    implicitHeight: 720
    property int current: 0
    readonly property var panes: [
        { key: "play", label: "Play", icon: "play" },
        { key: "graphics", label: "Graphics", icon: "monitor" },
        { key: "controls", label: "Controls", icon: "gamepad-2" },
        { key: "cheats", label: "Cheats", icon: "wand-sparkles" },
        { key: "saves", label: "Saves", icon: "hard-drive" },
        { key: "advanced", label: "Advanced", icon: "settings" },
        { key: "about", label: "About", icon: "info" } ]
    readonly property var pages: panes.map(pane => pane.key)

    function open(key) { current = pages.indexOf(key) }

    Sidebar {
        id: sidebar
        objectName: "sidebar"
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
        model: root.panes
        currentKey: root.pages[root.current] || ""
        onActivated: (key) => root.current = root.pages.indexOf(key)
    }
    Item {
        anchors { left: sidebar.right; right: parent.right; top: parent.top; bottom: parent.bottom }
        Repeater {
            model: root.panes
            Loader {
                id: slot
                required property var modelData
                required property int index
                readonly property bool shown: root.current === index
                property bool visited: false
                onShownChanged: if (shown) visited = true
                Component.onCompleted: if (shown) visited = true
                anchors.fill: parent
                visible: shown
                active: visited
                sourceComponent: ({ play: playPane, graphics: graphicsPane, controls: controlsPane, cheats: cheatsPane,
                                    saves: savesPane, advanced: advancedPane, about: aboutPane })[modelData.key]
                Component { id: playPane; PlayPane { onOpenSaves: root.open("saves") } }
                Component { id: graphicsPane; GraphicsPane { } }
                Component { id: controlsPane; ControlsPane { } }
                Component { id: cheatsPane; CheatsPane { } }
                Component { id: savesPane; SavesPane { } }
                Component { id: advancedPane; AdvancedPane { } }
                Component { id: aboutPane; AboutPane { } }
            }
        }
    }
}

import QtQuick
import QtQuick.Controls as Controls
import Launcher

// The shell: the sidebar and one pane per destination. A pane is built the first time it is shown and
// then kept (its scroll and state stay).
Rectangle {
    id: root
    color: Theme.windowBg
    implicitWidth: 1080
    implicitHeight: 720
    property int current: 0
    property bool animate: true          // the old Play screen's pulse
    readonly property var panes: [
        { key: "play", label: "Play", icon: "play" },
        { key: "graphics", label: "Graphics", icon: "monitor" },
        { key: "controls", label: "Controls", icon: "gamepad-2" },
        { key: "cheats", label: "Cheats", icon: "wand-sparkles" },
        { key: "saves", label: "Saves", icon: "hard-drive" },
        { key: "advanced", label: "Advanced", icon: "settings" },
        { key: "about", label: "About", icon: "info" } ]
    readonly property var pages: panes.map(pane => pane.key)

    // Until Tasks 2-5 replace them, the old screens load under their new keys; where a key holds several,
    // buttons above the screen switch between them.
    readonly property var screens: ({
        play: [[home, "Play"]], graphics: [[graphics, "Graphics"]],
        controls: [[controls, "Controls"], [guides, "Controller guides"]], cheats: [[mods, "Cheats"]],
        saves: [[saves, "Saves"]], advanced: [[advanced, "Advanced"], [fixes, "Fixes"], [support, "Support"]],
        about: [[updates, "Updates"]] })
    property var shownScreen: ({})
    function open(key, screen) {
        shownScreen = Object.assign({}, shownScreen, { [key]: screen })
        current = pages.indexOf(key)
    }

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
                sourceComponent: Pane {
                    id: pane
                    readonly property var list: root.screens[slot.modelData.key]
                    readonly property int screen: root.shownScreen[slot.modelData.key] || 0
                    title: slot.modelData.label
                    Row {
                        id: switcher
                        visible: pane.list.length > 1
                        spacing: Theme.s1
                        Repeater {
                            model: pane.list
                            Controls.Button {
                                required property var modelData
                                required property int index
                                text: modelData[1]
                                highlighted: index === pane.screen
                                onClicked: root.open(slot.modelData.key, index)
                            }
                        }
                    }
                    Rectangle {    // the old screens keep their dark look until they are redesigned
                        width: parent.width
                        height: pane.bodyHeight - Theme.s3 - (switcher.visible ? switcher.height + Theme.s1 : 0)
                        radius: Theme.radius
                        color: Theme.bg
                        clip: true
                        Loader { anchors.fill: parent; sourceComponent: pane.list[pane.screen][0] }
                    }
                }
            }
        }
    }
    Component { id: home; HomePage { animate: root.animate; onOpenFixes: root.open("advanced", 1); onOpenSaves: root.open("saves", 0) } }
    Component { id: graphics; GraphicsPage { } }
    Component { id: controls; ControlsPage { guidesShown: true; onOpenGuides: root.open("controls", 1) } }
    Component { id: guides; GuidesPage { } }
    Component { id: mods; ModsPage { } }
    Component { id: saves; SavesPage { } }
    Component { id: advanced; AdvancedPage { } }
    Component { id: fixes; FixesPage { } }
    Component { id: support; SupportPage { } }
    Component { id: updates; UpdatesPage { } }
}

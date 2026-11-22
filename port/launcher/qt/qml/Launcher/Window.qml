import QtQuick
import QtQuick.Window
import Launcher

Window {
    id: win
    property alias current: main.current
    property alias animate: main.animate
    readonly property alias pages: main.pages
    width: 1080
    height: 720
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    // Unified title bar, as in System Settings: the sidebar runs under the traffic lights.
    flags: Qt.Window | Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint
    color: Theme.windowBg
    title: "inFAMOUS Second Son"
    Main { id: main; anchors.fill: parent }
    Item {    // the title bar strip: Qt gets its mouse events, so it drags the window and zooms on double-click
        width: parent.width
        height: main.SafeArea.margins.top
        DragHandler { target: null; onActiveChanged: if (active) win.startSystemMove() }
        TapHandler { onDoubleTapped: win.visibility === Window.Maximized ? win.showNormal() : win.showMaximized() }
    }
}

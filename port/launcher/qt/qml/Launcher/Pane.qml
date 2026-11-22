import QtQuick
import QtQuick.Controls
import Launcher

// One sidebar destination: a 20 pt title over a scrolling column of groups (Launcher's Pane wins over
// the QtQuick.Controls type of the same name in either import order). Without a title the column starts
// there (Play puts the game's art first).
Item {
    id: pane
    property string title
    default property alias content: column.data
    readonly property real bodyHeight: scroll.height     // Play sizes its key art from it

    Text {
        id: heading
        visible: pane.title !== ""
        x: Theme.s3
        y: pane.SafeArea.margins.top + Theme.s1         // below the title bar
        width: parent.width - 2 * Theme.s3
        text: pane.title
        color: Theme.textPrimary
        font.pixelSize: Theme.title
        font.weight: Font.DemiBold
        elide: Text.ElideRight
        Accessible.role: Accessible.Heading
    }
    ScrollView {
        id: scroll
        anchors { top: heading.visible ? heading.bottom : heading.top; topMargin: heading.visible ? Theme.s2 : 0
                  left: parent.left; right: parent.right; bottom: parent.bottom }
        contentWidth: availableWidth
        Column {
            id: column
            x: Theme.s3
            width: scroll.availableWidth - 2 * Theme.s3
            spacing: Theme.s1
            bottomPadding: Theme.s3
        }
    }
}

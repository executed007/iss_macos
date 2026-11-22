import QtQuick
import Launcher

// The small sentence-case caption over a Group, aligned with the rows' text.
Text {
    width: parent ? parent.width : implicitWidth
    topPadding: Positioner.isFirstItem ? 0 : Theme.s2
    leftPadding: Theme.s2
    rightPadding: Theme.s2
    color: Theme.textSecondary
    font.pixelSize: Theme.caption
    wrapMode: Text.Wrap
    Accessible.role: Accessible.Heading
}

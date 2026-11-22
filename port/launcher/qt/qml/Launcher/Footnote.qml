import QtQuick
import Launcher

// Secondary text under a Group (what a setting changes, where a number comes from), aligned with the rows.
Text {
    width: parent ? parent.width : implicitWidth
    leftPadding: Theme.s2
    rightPadding: Theme.s2
    color: Theme.textSecondary
    font.pixelSize: Theme.caption
    wrapMode: Text.Wrap
    lineHeight: 1.15
}

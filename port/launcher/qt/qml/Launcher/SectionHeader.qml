import QtQuick
import Launcher

// The sentence-case heading over a Group, 13 pt semibold as in macOS 26 System Settings, aligned with the rows' text.
Text {
    width: parent ? parent.width : implicitWidth
    topPadding: Positioner.isFirstItem ? 0 : Theme.s2
    leftPadding: Theme.s2
    rightPadding: Theme.s2
    color: Theme.textPrimary
    font.pixelSize: Theme.headline
    font.weight: Font.DemiBold
    wrapMode: Text.Wrap
    Accessible.role: Accessible.Heading
}

import QtQuick
import Launcher

// An inset grouped list: SettingRows on a rounded fill; the rows draw the hairlines between them.
Rectangle {
    default property alias rows: column.data
    width: parent ? parent.width : implicitWidth
    implicitHeight: column.implicitHeight
    radius: Theme.radius
    color: Theme.groupBg
    Column {
        id: column
        width: parent.width
    }
}

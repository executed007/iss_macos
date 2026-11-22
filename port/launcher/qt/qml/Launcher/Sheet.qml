import QtQuick
import QtQuick.Controls
import Launcher

// The native confirmation sheet for a backend that asks one (Saves, Graphics): `source` has `dialog`
// ({open, title, body, lines, buttons}) and `answer(id)`. Return or a button answers it; Escape cancels.
Dialog {
    id: sheet
    objectName: "sheet"
    property var source
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(480, parent.width - 2 * Theme.s3)
    padding: Theme.s2
    topPadding: 0
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.3) }
    background: Rectangle { color: Theme.windowBg; radius: Theme.radius; border.width: 1; border.color: Theme.separator }
    header: Text {
        text: sheet.source.dialog.title || ""
        color: Theme.textPrimary
        font.pixelSize: Theme.callout
        font.weight: Font.DemiBold
        wrapMode: Text.Wrap
        topPadding: Theme.s2; leftPadding: Theme.s2; rightPadding: Theme.s2; bottomPadding: Theme.s1
        Accessible.role: Accessible.Heading
    }
    onClosed: if (sheet.source.dialog.open) sheet.source.answer("cancel")
    contentItem: Column {
        spacing: Theme.s1
        Text {
            width: parent.width
            text: sheet.source.dialog.body || ""
            color: Theme.textPrimary
            font.pixelSize: Theme.body
            wrapMode: Text.Wrap
        }
        Repeater {
            model: (sheet.source.dialog.lines || []).slice(0, 8)
            Text {
                required property string modelData
                width: parent.width
                text: modelData
                color: Theme.textSecondary
                font.pixelSize: Theme.caption
                wrapMode: Text.Wrap
            }
        }
    }
    footer: Row {
        spacing: Theme.s1
        layoutDirection: Qt.RightToLeft
        padding: Theme.s2
        Repeater {
            model: [...(sheet.source.dialog.buttons || [])].reverse()
            Button {
                required property var modelData
                text: modelData.label
                highlighted: modelData.tone === "primary"
                onClicked: sheet.source.answer(modelData.id)
            }
        }
    }
    Connections {
        target: sheet.source
        function onDialogChanged() { sheet.source.dialog.open ? sheet.open() : sheet.close() }
    }
}

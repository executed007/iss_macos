import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Launcher

// Controls: the mouse camera, the stick, the keyboard map and the controller guides.
Pane {
    id: page
    title: "Controls"

    SectionHeader { text: "Mouse camera" }
    Group {
        SettingRow {
            label: "Mouse camera"
            detail: "Look around with the mouse."
            Switch {
                checked: PadControls.mouseEnabled
                onToggled: {
                    PadControls.toggleMouse()
                    checked = Qt.binding(() => PadControls.mouseEnabled)
                }
            }
        }
        ModelRow { source: PadControls; rowId: "m:mouse_sensitivity"; label: "Sensitivity" }
        ModelRow { source: PadControls; rowId: "m:mouse_invert_y"; label: "Invert vertical look" }
    }

    SectionHeader { text: "Controller" }
    Group {
        ModelRow { source: PadControls; rowId: "c:stick_deadzone"; label: "Stick dead zone"; detail: "Raise it if the camera drifts on its own." }
        SettingRow {
            label: "Controller guides"
            detail: "Which button does what on Xbox, Switch Pro and 8BitDo controllers."
            Button { text: "Show…"; onClicked: Backend.openGuide() }
        }
    }

    SectionHeader { text: "Keyboard" }
    Group {
        SettingRow {
            label: "Custom key map"
            detail: "Empty keeps the default keys."
            TextField {
                id: field
                objectName: "keyMap"
                Layout.preferredWidth: 200
                placeholderText: "Default"
                selectByMouse: true
                Component.onCompleted: text = PadControls.draftText
                onTextChanged: PadControls.setDraft(text)
                onAccepted: if (PadControls.draftValid && PadControls.draftDirty) PadControls.applyKeyMap()
                Connections {
                    target: PadControls
                    function onDraftChanged() { if (field.text !== PadControls.draftText) field.text = PadControls.draftText }
                }
            }
            Button { text: "Apply"; enabled: PadControls.draftValid && PadControls.draftDirty; onClicked: PadControls.applyKeyMap() }
            Button { text: "Reset"; enabled: PadControls.savedKeyMap !== "" || field.text !== ""; onClicked: PadControls.clearKeyMap() }
        }
    }
    Footnote {
        text: PadControls.draftMessages.length ? PadControls.draftMessages.map(m => m.text).join(" ") : Backend.t("keymap_intro")
        color: PadControls.draftValid ? Theme.textSecondary : Theme.textPrimary
    }
    Group {
        Repeater {
            model: PadControls.keyRows
            SettingRow {
                required property var modelData
                label: modelData.name
                detail: modelData.changed ? "Remapped" : ""
                Text { text: modelData.keys; color: Theme.textSecondary; font.pixelSize: Theme.body }
            }
        }
    }
}

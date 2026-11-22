import QtQuick
import QtQuick.Controls
import Launcher

// Cheats: the GoldHEN community cheats, then the built-in options, one switch each. The list is the backend's
// (read off the UI thread at start); a switch writes mods.json through the same slot as before.
Pane {
    id: page
    title: "Cheats"

    // Only the list of keys rebuilds the rows, so a toggle keeps the keyboard focus on its switch.
    property var keys: []
    function sync() {
        const now = Backend.modsModel.map(cheat => cheat.key)
        if (now.join("\n") !== keys.join("\n"))
            keys = now
    }
    Component.onCompleted: sync()
    Connections {
        target: Backend
        function onModsChanged() { page.sync() }
    }

    Group {
        SettingRow {
            visible: Backend.modsLoading || page.keys.length === 0
            label: Backend.modsLoading ? "Loading…" : "No cheats found for this game version."
        }
        Repeater {
            model: page.keys
            SettingRow {
                id: row
                required property string modelData
                readonly property var cheat: Backend.modsModel.find(c => c.key === modelData) || ({})
                label: cheat.name || ""
                detail: cheat.note || ""
                Switch {
                    checked: row.cheat.on === true
                    onToggled: {
                        Backend.setModEnabled(row.modelData, checked)
                        checked = Qt.binding(() => row.cheat.on === true)
                    }
                }
            }
        }
    }
    Footnote { text: "From the GoldHEN community cheat list. Changes apply the next time you press Play." }

    Group {
        SettingRow {
            label: "Cheat files"
            detail: "Put more GoldHEN cheat files in this folder, then press Refresh."
            Button { text: "Refresh"; onClicked: Backend.refreshMods() }
            Button { text: "Show cheats folder"; onClicked: Backend.openModsFolder() }
        }
    }
}

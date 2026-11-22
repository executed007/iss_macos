import QtQuick
import QtQuick.Controls
import Launcher

// About: the version, where this port lives, how to update, and the credits.
Pane {
    id: page
    title: "About"

    Group {
        SettingRow { label: "inFAMOUS Second Son"; detail: "macOS port for Apple silicon" }
        SettingRow {
            label: "Version"
            Text { text: Backend.buildVersion || "Not tagged"; color: Theme.textSecondary; font.pixelSize: Theme.body }
        }
        SettingRow {
            label: "macOS port"
            detail: "github.com/executed007/iss_macos"
            Button { text: "Open"; onClicked: Backend.openUrl("fork") }
        }
        SettingRow {
            label: "Updates"
            detail: "Pull the latest version and rebuild."
        }
    }

    SectionHeader { text: "Credits" }
    Group {
        SettingRow {
            label: "Based on iss_pc by HayatoG"
            detail: "The PC port this one builds on."
            Button { text: "Project"; onClicked: Backend.openUrl("upstream") }
            Button { text: "Buy HayatoG a coffee"; onClicked: Backend.openUrl("coffee") }
        }
        SettingRow {
            label: "GoldHEN cheat repository"
            detail: "The community cheats on the Cheats screen."
            Button { text: "Open"; onClicked: Backend.openUrl("goldhen") }
        }
        SettingRow {
            label: "MoltenVK patches by satperm"
            detail: "github.com/satperm/iss_mac_port"
            Button { text: "Open"; onClicked: Backend.openUrl("moltenvk") }
        }
        SettingRow {
            label: "Icons: Lucide"
            detail: "ISC licence. The licence text is bundled with the icons."
            Button { text: "Licence"; onClicked: Backend.openUrl("lucide") }
        }
    }
}

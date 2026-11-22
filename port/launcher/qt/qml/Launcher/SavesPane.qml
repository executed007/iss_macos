import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Launcher

// Saves: the data folder, the saves in it, their backups, export and import. Every button calls the same
// Saves slot as before; the confirmation (restore, delete, import) is a native sheet driven by Saves.dialog.
Pane {
    id: page
    title: "Saves"
    property string exportTarget: ""      // "" = all saves; else a slot id
    property string folderPurpose: "data"

    Group {    // what the last action did (or why it failed), and why the buttons are off while the game runs
        visible: Saves.message !== "" || Saves.running
        SettingRow {
            visible: Saves.running
            label: Theme.tr("saves_running")
        }
        SettingRow {
            visible: Saves.message !== ""
            label: Saves.message
            Button { text: "Dismiss"; onClicked: Saves.dismissMessage() }
        }
    }

    SectionHeader { text: "Data folder" }
    Group {
        SettingRow {
            label: Saves.sourceText
            detail: Saves.userDir
            Button { text: "Show in Finder"; onClicked: Saves.openFolder() }
        }
        SettingRow {
            label: "Change folder"
            detail: Theme.tr("saves_suggest").replace("{path}", Saves.suggestion)
            Button { text: "Use suggested"; visible: Saves.source === "default"; enabled: Saves.canChange; onClicked: Saves.useSuggested() }
            Button { text: "Reset"; visible: Saves.source === "pointer"; enabled: Saves.canChange; onClicked: Saves.resetDir() }
            Button { text: "Choose…"; enabled: Saves.canChange; onClicked: { page.folderPurpose = "data"; folderDialog.open() } }
        }
        SettingRow {
            label: "Saves in other folders"
            detail: Saves.scanning ? "Looking…" : Saves.scanned && Saves.found.length === 0 ? Theme.tr("saves_scan_none") : ""
            Button { text: "Look again"; enabled: !Saves.scanning; onClicked: Saves.scan() }
        }
        Repeater {
            model: Saves.found
            SettingRow {
                required property var modelData
                label: modelData.text
                Button { text: "Import…"; enabled: !Saves.running; onClicked: Saves.importFound(index) }
            }
        }
    }
    Footnote { visible: Saves.source === "default"; text: Theme.tr("saves_portable_warn") }
    Footnote { visible: !Saves.canChange; text: Theme.tr("saves_env_locked") }

    SectionHeader { text: "Saves" }
    Group {
        SettingRow { visible: !Saves.hasSaves; label: Theme.tr("saves_none") }
        Repeater {
            model: Saves.slots
            SettingRow {
                required property var modelData
                label: modelData.title
                detail: modelData.kindText + " · " + modelData.slot + " · " + modelData.when + " · " + modelData.size
                Button { text: "Export…"; onClicked: { page.exportTarget = modelData.id; saveZip.open() } }
                Button { text: "Duplicate"; enabled: !Saves.running; onClicked: Saves.duplicate(modelData.id) }
                Button { text: "Delete…"; enabled: !Saves.running; onClicked: Saves.askDelete(modelData.id) }
            }
        }
    }
    Group {
        SettingRow {
            label: "Back up now"
            Button { text: "Back up"; onClicked: Saves.backupNow() }
        }
        SettingRow {
            label: "Import saves from a ZIP"
            Button { text: "Import…"; enabled: !Saves.running; onClicked: openZip.open() }
        }
        SettingRow {
            label: "Export all saves"
            Button { text: "Export…"; enabled: Saves.hasSaves; onClicked: { page.exportTarget = ""; saveZip.open() } }
        }
    }

    SectionHeader { text: "Backups" }
    Group {
        SettingRow {
            label: Theme.tr("backups_keep").replace("{n}", Saves.keep)
            Button { text: "Fewer"; onClicked: Saves.setKeep(Saves.keep - 1) }
            Button { text: "More"; onClicked: Saves.setKeep(Saves.keep + 1) }
        }
        SettingRow {
            label: Theme.tr("backups_extra")
            detail: Saves.extraDir || Theme.tr("backups_extra_none")
            Button { text: "Clear"; visible: Saves.extraDir !== ""; onClicked: Saves.clearExtra() }
            Button { text: "Choose…"; onClicked: { page.folderPurpose = "extra"; folderDialog.open() } }
        }
        SettingRow { visible: Saves.backups.length === 0; label: Theme.tr("backups_none") }
        Repeater {
            model: Saves.backups
            SettingRow {
                required property var modelData
                label: modelData.when
                detail: modelData.info
                Button { text: "Restore…"; enabled: !Saves.running; onClicked: Saves.askRestore(modelData.name) }
            }
        }
    }
    Footnote { text: Theme.tr("backups_intro") }

    Dialog {    // the confirmation Saves asks for; Return or a button answers it, Escape cancels
        id: sheet
        objectName: "sheet"
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(480, parent.width - 2 * Theme.s3)
        padding: Theme.s2
        topPadding: 0
        Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.3) }
        background: Rectangle { color: Theme.windowBg; radius: Theme.radius; border.width: 1; border.color: Theme.separator }
        header: Text {
            text: Saves.dialog.title || ""
            color: Theme.textPrimary
            font.pixelSize: Theme.callout
            font.weight: Font.DemiBold
            wrapMode: Text.Wrap
            topPadding: Theme.s2; leftPadding: Theme.s2; rightPadding: Theme.s2; bottomPadding: Theme.s1
            Accessible.role: Accessible.Heading
        }
        onClosed: if (Saves.dialog.open) Saves.answer("cancel")
        contentItem: Column {
            spacing: Theme.s1
            Text {
                width: parent.width
                text: Saves.dialog.body || ""
                color: Theme.textPrimary
                font.pixelSize: Theme.body
                wrapMode: Text.Wrap
            }
            Repeater {
                model: (Saves.dialog.lines || []).slice(0, 8)
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
                model: [...(Saves.dialog.buttons || [])].reverse()
                Button {
                    required property var modelData
                    text: modelData.label
                    highlighted: modelData.tone === "primary"
                    onClicked: Saves.answer(modelData.id)
                }
            }
        }
    }
    Connections {
        target: Saves
        function onDialogChanged() { Saves.dialog.open ? sheet.open() : sheet.close() }
    }

    FolderDialog {
        id: folderDialog
        title: Theme.tr(page.folderPurpose === "data" ? "saves_choose_title" : "backups_extra_title")
        currentFolder: "file://" + (page.folderPurpose === "data" ? Saves.userDir : Saves.extraDir)
        onAccepted: page.folderPurpose === "data" ? Saves.chooseDir(selectedFolder.toString()) : Saves.chooseExtra(selectedFolder.toString())
    }
    FileDialog {
        id: openZip
        title: Theme.tr("saves_zip_open_title")
        nameFilters: ["ZIP (*.zip)"]
        onAccepted: Saves.importZip(selectedFile.toString())
    }
    FileDialog {
        id: saveZip
        title: Theme.tr("saves_zip_save_title")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "zip"
        nameFilters: ["ZIP (*.zip)"]
        onAccepted: page.exportTarget === "" ? Saves.exportAll(selectedFile.toString()) : Saves.exportSlot(page.exportTarget, selectedFile.toString())
    }
}

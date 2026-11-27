import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Launcher

// Advanced: everything an expert tunes, in collapsed groups. The rows are the backends' own (the same keys
// and values as before); nothing here is needed to play.
Pane {
    id: page
    title: "Advanced"

    Footnote { text: "These are test switches. A wrong value can lower the frame rate, break an effect or stop the game from starting. Changes apply the next time you play." }

    Disclosure {
        title: "In-game menu options"
        detail: "The options of the game's menu (Insert) that Graphics does not show."
        Column {
            ModelRow { source: Graphics; rowId: "menu:region_lod"; describe: true }
            ModelRow { source: Graphics; rowId: "menu:region_fade"; describe: true }
            ModelRow { source: Graphics; rowId: "menu:near_clip"; describe: true }
            ModelRow { source: Graphics; rowId: "menu:far_clip"; describe: true }
            ModelRow { source: Graphics; rowId: "menu:mip_lod_bias"; describe: true }
            ModelRow { source: Graphics; rowId: "menu:iss_temporal"; describe: true }
            ModelRow { source: Graphics; rowId: "menu:iss_temporal_mode"; describe: true }
            ModelRow { source: PadControls; rowId: "i:rumble"; describe: true }
            ModelRow { source: PadControls; rowId: "menu:sensor_aim"; describe: true }
            ModelRow { source: Graphics; rowId: "menu:overlay_fps"; describe: true }
            ModelRow { source: Graphics; rowId: "menu:overlay_frametime"; describe: true }
            ModelRow { source: Graphics; rowId: "menu:overlay_upscaler"; describe: true }
            ModelRow { source: Graphics; rowId: "menu:overlay_gpu"; describe: true }
            ModelRow { source: Graphics; rowId: "menu:overlay_cpu"; describe: true }
            ModelRow { source: Graphics; rowId: "menu:overlay_ram"; describe: true }
            ModelRow { source: Graphics; rowId: "menu:overlay_vram"; describe: true }
            ModelRow { source: Graphics; rowId: "menu:overlay_scale"; step: 10; describe: true }
            ModelRow { source: Graphics; rowId: "menu:overlay_corner"; describe: true }
            ModelRow { source: Graphics; rowId: "menu:overlay_opacity"; describe: true }
            ModelRow { source: Graphics; rowId: "menu:overlay_layout"; describe: true }
        }
    }

    Disclosure {
        title: "Touchpad mouse mode"
        detail: "Use the controller's touchpad as a mouse."
        Column {
            ModelRow { source: PadControls; rowId: "p:mouse_pad"; describe: true }
            ModelRow { source: PadControls; rowId: "p:mouse_pad_key"; describe: true }
            ModelRow { source: PadControls; rowId: "p:mouse_pad_mode"; describe: true }
            ModelRow { source: PadControls; rowId: "p:mouse_pad_sens"; describe: true }
            ModelRow { source: PadControls; rowId: "p:mouse_pad_drag"; step: 10; describe: true }
        }
    }

    Disclosure {
        title: "Fixes"
        detail: "The port's fixes BP01 to BP12 and a few more. " + Backend.profileText + "."
        Column {
            SettingRow {
                label: "Profile"
                detail: Theme.tr("fixes_recompile")
                ComboBox {
                    id: profile
                    model: ["Recommended", "Like the previous release"]
                    currentIndex: Backend.profileIsRecommended ? 0 : -1
                    displayText: Backend.profileText
                    implicitContentWidthPolicy: ComboBox.WidestText
                    onActivated: (i) => {
                        Backend.resetFixes(i === 0 ? "defaults" : "previous")
                        currentIndex = Qt.binding(() => Backend.profileIsRecommended ? 0 : -1)
                    }
                }
            }
            Repeater { model: Backend.fixesModel; FixRow { required property var model; d: model.d } }
        }
    }

    Disclosure {
        title: "A/B switches"
        detail: "Every tuning switch of the GPU backend. \"Automatic\" lets the game decide. " + Backend.advancedText + "."
        Column {
            SettingRow {
                label: "Filter"
                TextField {
                    Layout.preferredWidth: 200
                    placeholderText: "Search switches"
                    selectByMouse: true
                    onTextChanged: Backend.setAdvancedFilter(text)
                }
                Button { text: "Restore defaults"; onClicked: Backend.resetAdvanced() }
                Button { text: "Copy my combination"; onClicked: Backend.copyCombination() }
            }
            Repeater { model: Backend.advancedModel; FixRow { required property var model; d: model.d } }
        }
    }

    Disclosure {
        title: "Diagnostics"
        detail: "For reports: what the log says and how light glare is drawn."
        Column {
            SettingRow {
                label: "Detailed log"
                detail: "Adds more lines to the game's log, which makes it larger."
                Switch {
                    checked: Backend.verboseLog
                    onToggled: {
                        Backend.setVerboseLog(checked)
                        checked = Qt.binding(() => Backend.verboseLog)
                    }
                }
            }
            SettingRow {
                label: "Light glare (occlusion)"
                detail: "How the glare around bright lights is tested."
                ComboBox {
                    model: Backend.occlusionOptions
                    textRole: "label"
                    currentIndex: Backend.occlusionIndex
                    implicitContentWidthPolicy: ComboBox.WidestText
                    onActivated: (i) => {
                        Backend.setOcclusion(Backend.occlusionOptions[i].value)
                        currentIndex = Qt.binding(() => Backend.occlusionIndex)
                    }
                }
            }
        }
    }

    Disclosure {
        title: "Shader cache"
        detail: Graphics.cacheText
        Column {
            SettingRow {
                label: Graphics.cacheText
                detail: Theme.tr("gfx_cache_hint")
                Button { text: "Clear…"; enabled: Graphics.canClear && !Graphics.confirming; onClicked: Graphics.askClear() }
            }
            SettingRow {
                visible: Graphics.confirming
                label: Graphics.confirmText
                Button { text: "Cancel"; onClicked: Graphics.cancelClear() }
                Button { text: "Clear"; onClicked: Graphics.clearCache() }
            }
            SettingRow {
                visible: Graphics.message !== ""
                label: Graphics.message
                labelColor: Theme.toneColor(Graphics.messageTone)
            }
        }
    }

    Disclosure {
        title: "Texture memory"
        detail: "How much video memory the texture cache may use."
        Column {
            ModelRow { source: Graphics; rowId: "s:vram_budget_mb"; describe: true }
            SettingRow {
                visible: Graphics.canSuggest
                label: "Suggested for this Mac"
                Button { text: "Use " + Graphics.vramSuggested + " MB"; onClicked: Graphics.useSuggestedBudget() }
            }
        }
    }

    Disclosure {
        title: "Share settings"
        detail: "Save your graphics settings to a file, or load someone else's."
        Column {
            SettingRow {
                label: "Settings file"
                Button { text: "Export…"; onClicked: saveJson.open() }
                Button { text: "Import…"; onClicked: openJson.open() }
            }
            SettingRow {
                visible: Graphics.shareMessage !== ""
                label: Graphics.shareMessage
                labelColor: Theme.toneColor(Graphics.shareTone)
            }
        }
    }

    Disclosure {
        title: "Report a problem"
        detail: "A zip with your hardware, the settings and the last log, with your user name removed."
        Column {
            SettingRow {
                label: "Problem report"
                detail: "Nothing is sent: you attach the file to a bug report yourself."
                Button { text: "Show logs"; onClicked: Support.openLogsFolder() }
                Button { text: "Save report…"; onClicked: reportZip.open() }
            }
            SettingRow {
                visible: Support.message !== ""
                label: Support.message
                labelColor: Theme.toneColor(Support.tone)
                Button { text: "Dismiss"; onClicked: Support.dismissMessage() }
            }
        }
    }

    Sheet { source: Graphics }
    FileDialog {
        id: openJson
        title: "Import a settings file"
        nameFilters: ["Settings (*.json)", "All files (*)"]
        onAccepted: Graphics.importPreset(selectedFile.toString())
    }
    FileDialog {
        id: saveJson
        title: "Export your settings"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        currentFile: currentFolder + "/" + Graphics.exportFile
        nameFilters: ["Settings (*.json)"]
        onAccepted: Graphics.exportPreset(Graphics.exportName || "My settings", "", selectedFile.toString())
    }
    FileDialog {
        id: reportZip
        title: "Save the problem report"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "zip"
        currentFile: Support.defaultZipUrl
        nameFilters: ["ZIP (*.zip)"]
        onAccepted: Support.saveZip(selectedFile.toString())
    }
}

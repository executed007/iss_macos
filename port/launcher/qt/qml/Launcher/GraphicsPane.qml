import QtQuick
import QtQuick.Controls
import Launcher

// Graphics: what the picture looks like, and how fast it runs on this Mac. Every row is one of the Graphics
// backend's rows (the same keys as before); Advanced keeps the rest.
Pane {
    id: page
    title: "Graphics"

    SectionHeader { text: "Display" }
    Group {
        ModelRow { source: Graphics; rowId: "s:display_mode"; label: "Window mode" }
        ModelRow { source: Graphics; rowId: "s:vsync"; label: "VSync"; detail: "No tearing, a little more input delay." }
        ModelRow {
            source: Graphics; rowId: "s:show_fps"; label: "Performance overlay"
            detail: "Shown when the game starts; F10 toggles it in the game."
        }
    }

    SectionHeader { text: "Performance" }
    Group {
        SettingRow {
            id: preset
            label: "Preset"
            readonly property int index: Math.max(0, Graphics.presetChoices.findIndex(p => p.id === Graphics.presetId))
            detail: Graphics.presetChoices[index].what
            ComboBox {
                objectName: "preset"
                model: Graphics.presetChoices
                textRole: "name"
                currentIndex: preset.index
                implicitContentWidthPolicy: ComboBox.WidestText
                onActivated: (i) => {
                    Graphics.choosePreset(Graphics.presetChoices[i].id)
                    currentIndex = Qt.binding(() => preset.index)     // Custom is a result, not a choice
                }
            }
        }
        ModelRow { source: Graphics; rowId: "n:output"; label: "Output size" }
        ModelRow { source: Graphics; rowId: "n:internal"; label: "Internal resolution" }
        ModelRow { source: Graphics; rowId: "menu:fsr1"; label: "FSR 1 upscaling"; detail: "Enlarges the image when it is smaller than the window." }
        ModelRow { source: Graphics; rowId: "menu:rcas"; label: "Sharpening"; detail: "Sharpens the final image." }
        ModelRow { source: Graphics; rowId: "menu:fps_limit"; label: "Frame rate limit" }
        ModelRow {
            source: Graphics; rowId: "menu:quality_floor"; label: "Detail level"; step: 1
            detail: "Higher runs faster with less detail (0 is the best, 14 the lowest)."
        }
    }
    Footnote { text: "About 30 FPS in most scenes at the Balanced settings on this Mac. Changes apply the next time you play." }
}

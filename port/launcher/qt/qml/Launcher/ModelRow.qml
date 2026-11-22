import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Launcher

// A SettingRow driven by one row of a backend's rowMap (Graphics or PadControls): a switch, a pop-up or a slider,
// written back through the backend's setValue, so it writes the same key and value as before. A pane may set
// its own label and detail; the row is dimmed and inert while the backend marks it disabled.
SettingRow {
    id: row
    property var source
    property string rowId
    property real step: d.step || 1
    readonly property var d: (source && source.rowMap[rowId]) || ({})
    label: d.label || ""
    detail: d.note || d.info || ""
    opacity: d.disabled ? 0.4 : 1

    Switch {
        visible: row.d.control === "switch"
        enabled: !row.d.disabled
        checked: row.d.on === true
        onToggled: {
            row.source.setValue(row.rowId, checked ? "1" : "0")
            checked = Qt.binding(() => row.d.on === true)     // the backend's value, whatever the write did
        }
    }
    ComboBox {
        visible: row.d.control === "combo"
        enabled: !row.d.disabled
        model: row.d.options || []
        textRole: "label"
        currentIndex: row.d.index || 0
        implicitContentWidthPolicy: ComboBox.WidestText
        onActivated: (index) => {
            row.source.setValue(row.rowId, row.d.options[index].value)
            currentIndex = Qt.binding(() => row.d.index || 0)
        }
    }
    Slider {   // written when the knob is let go, or on each key press: not on every pixel of a drag
        id: slider
        visible: row.d.control === "slider"
        enabled: !row.d.disabled
        Layout.preferredWidth: 160
        from: row.d.lo || 0
        to: row.d.hi || 1
        stepSize: row.step
        value: row.d.value || 0
        function write() {
            row.source.setValue(row.rowId, String(value))
            value = Qt.binding(() => row.d.value || 0)
        }
        onMoved: if (!pressed) write()
        onPressedChanged: if (!pressed) write()
    }
    Text {
        visible: row.d.control === "slider"
        Layout.preferredWidth: 48
        horizontalAlignment: Text.AlignRight
        text: slider.pressed ? slider.value.toFixed(row.d.decimals || 0) + (row.d.unit || "") : (row.d.valueText || "")
        color: Theme.textSecondary
        font.pixelSize: Theme.body
    }
}

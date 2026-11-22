import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Launcher

// One row of a FixesModel (a BPnn fix or an A/B switch): the name, what it does, and a switch, a number or a
// pop-up; written through Backend.setFix, as before. A group header row of the model shows as a small caption.
Item {
    id: row
    property var d: ({})
    width: parent ? parent.width : implicitWidth
    implicitHeight: d.kind === "header" ? caption.implicitHeight + Theme.s2 : item.implicitHeight

    Text {
        id: caption
        visible: row.d.kind === "header"
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: Theme.s2; rightMargin: Theme.s2 }
        text: row.d.title || ""
        color: Theme.textSecondary
        font.pixelSize: Theme.caption
        wrapMode: Text.Wrap
        Accessible.role: Accessible.Heading
    }
    SettingRow {
        id: item
        visible: row.d.kind === "item"
        width: parent.width
        label: (row.d.code ? row.d.code + "  " : "") + (row.d.label || "")
        detail: [row.d.what || "", (row.d.tags || []).map(t => t.text).concat(row.d.shaderWarn ? [row.d.shaderWarn] : []).join(" · ")]
                .filter(t => t !== "").join("\n")
        Switch {
            visible: row.d.control === "switch"
            checked: row.d.on === true
            onToggled: {
                Backend.setFix(row.d.id, checked ? "1" : "0")
                checked = Qt.binding(() => row.d.on === true)
            }
        }
        TextField {
            visible: row.d.control === "number"
            Layout.preferredWidth: 88
            horizontalAlignment: Text.AlignRight
            text: row.d.value === undefined ? "" : row.d.value
            placeholderText: row.d.placeholder === undefined ? "" : String(row.d.placeholder)
            inputMethodHints: Qt.ImhDigitsOnly
            validator: IntValidator { bottom: row.d.lo || 0; top: row.d.hi || 0 }
            onEditingFinished: Backend.setFix(row.d.id, text === "" ? "auto" : text)
        }
        Text {
            visible: row.d.control === "number" && !!row.d.unit
            text: row.d.unit || ""
            color: Theme.textSecondary
            font.pixelSize: Theme.body
        }
        ComboBox {
            visible: row.d.control === "combo"
            model: row.d.options || []
            textRole: "label"
            currentIndex: row.d.index || 0
            implicitContentWidthPolicy: ComboBox.WidestText
            onActivated: (i) => {
                Backend.setFix(row.d.id, row.d.options[i].value)
                currentIndex = Qt.binding(() => row.d.index || 0)
            }
        }
    }
}

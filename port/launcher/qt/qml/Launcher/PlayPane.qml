import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Templates as T
import Launcher

// Play: the game's own key art (read from the player's game folder at run time, never shipped), its title,
// one Play button and a short status list. Without art the area stays plain, with the title in it.
Pane {
    id: page
    signal openSaves()
    readonly property string gameTitle: "inFAMOUS Second Son"
    // logic.compute_chips: the GPU chip, then the game folder's ('ok' once it holds the supported game), then
    // the others; a 'bad' one after the folder's is the port not being built.
    readonly property var gpuChip: Backend.chips[0] || { text: "" }
    readonly property var gameChip: Backend.chips[1] || { text: "", level: "bad" }
    readonly property var portChip: Backend.chips.slice(2).find(chip => chip.level === "bad")
    readonly property bool folderOk: gameChip.level === "ok"
    readonly property string cacheLead: Backend.tf("chip_cache", { size: "" })      // "Shader cache: "
    readonly property var cacheChip: Backend.chips.find(chip => chip.text.startsWith(cacheLead))
    readonly property string cacheText: cacheChip ? cacheChip.text.slice(cacheLead.length) : ""
    function primary() { folderOk ? Backend.play() : folderDialog.open() }

    component Value: Text { color: Theme.textSecondary; font.pixelSize: Theme.body }

    Item {   // the key art, 40% of the pane's body; plain (the window background) with the title when there is none
        id: art
        readonly property bool plain: image.status === Image.Null || image.status === Image.Error
        width: parent.width
        height: Math.round(page.bodyHeight * 0.4)
        Image {
            id: image
            readonly property bool wide: implicitWidth > 1.5 * implicitHeight     // pic1/pic0 are 16:9, icon0 square
            anchors.fill: parent
            source: Backend.keyArtUrl
            asynchronous: true
            fillMode: wide ? Image.PreserveAspectCrop : Image.PreserveAspectFit
            verticalAlignment: wide ? Image.AlignTop : Image.AlignVCenter     // keeps pic1's crows and pic0's face
        }
        Rectangle {   // rounds the picture's corners: a frame in the window's color, as wide as the radius
            visible: image.status === Image.Ready
            anchors.centerIn: image
            width: Math.min(image.width, image.paintedWidth) + 2 * Theme.radius
            height: Math.min(image.height, image.paintedHeight) + 2 * Theme.radius
            radius: 2 * Theme.radius
            color: "transparent"
            border.width: Theme.radius
            border.color: Theme.windowBg
        }
        Text {
            visible: art.plain
            anchors.centerIn: parent
            text: page.gameTitle
            color: Theme.textPrimary
            font.pixelSize: Theme.title
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
        }
    }

    Item {   // the title, the save and the one Play button: 16 pt under the art, 24 pt over the status list
        width: parent.width
        implicitHeight: Theme.s1 + bar.height + Theme.s2
        Item {
            id: bar
            y: Theme.s1
            width: parent.width
            height: Math.max(heading.implicitHeight, playButton.implicitHeight)
            Column {
                id: heading
                anchors { left: parent.left; right: playButton.left; rightMargin: Theme.s2; verticalCenter: parent.verticalCenter }
                spacing: 2
                Text {
                    visible: !art.plain
                    width: parent.width
                    text: page.gameTitle
                    color: Theme.textPrimary
                    font.pixelSize: Theme.title
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    Accessible.role: Accessible.Heading
                }
                Text {   // "Continue · <save>", "New game", or what is happening (starting, shader list progress)
                    width: parent.width
                    text: Backend.playSubtitle
                    color: Theme.textSecondary
                    font.pixelSize: Theme.body
                    elide: Text.ElideRight
                }
            }
            T.Button {
                id: playButton
                anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                text: page.folderOk ? "Play" : "Choose game folder…"
                enabled: !page.folderOk || Backend.canPlay
                opacity: enabled ? 1 : 0.4
                implicitWidth: Math.max(96, contentItem.implicitWidth + 2 * Theme.s3)
                implicitHeight: 32
                contentItem: Text {
                    text: playButton.text
                    color: Theme.textOnAccent
                    font.pixelSize: Theme.body
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {   // the accent capsule
                    radius: height / 2
                    color: playButton.down ? Qt.darker(Theme.accent, 1.15) : Theme.accent
                    Rectangle {   // the focus ring, as macOS draws it
                        visible: playButton.visualFocus
                        anchors { fill: parent; margins: -3 }
                        radius: height / 2
                        color: "transparent"
                        border.width: 3
                        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.5)
                    }
                }
                onClicked: page.primary()
            }
        }
    }

    Group {
        SettingRow { label: "Mac"; Value { text: page.gpuChip.text } }
        SettingRow {   // a problem with the folder (none, wrong version) is this row, with its fix
            label: page.folderOk ? "Game" : page.gameChip.text
            Value {   // the title ID and version: hero_kicker is "Seattle · <title ID> · v<version>"
                visible: page.folderOk
                text: Backend.heroKicker.split(" · ").slice(1).join(" · ")
            }
            Button { text: page.folderOk ? "Change…" : "Choose…"; onClicked: folderDialog.open() }
        }
        SettingRow { visible: page.portChip !== undefined; label: page.portChip ? page.portChip.text : "" }
        SettingRow {   // the data folder has no saves but another folder does (typically after an update)
            visible: Saves.homeHint !== ""
            label: Saves.homeHint
            Button { text: "Show Saves"; onClicked: page.openSaves() }
        }
        SettingRow { label: "Shader cache"; Value { text: page.cacheText.charAt(0).toUpperCase() + page.cacheText.slice(1) } }
        SettingRow { label: "Cheats"; Value { text: Backend.modsModel.filter(mod => mod.on).length + " on" } }
        SettingRow {
            label: "Language"
            ComboBox {   // the game's voice and text, from the next start
                objectName: "language"
                model: Backend.gameLanguageOptions
                textRole: "label"
                valueRole: "value"
                currentIndex: Backend.gameLanguageIndex
                displayText: currentIndex === 0 && Backend.gameLanguageNow !== ""     // "Automatic: English" -> "Automatic (English)"
                             ? Backend.gameLanguageNow.replace(/: (.*)/, " ($1)") : currentText
                implicitContentWidthPolicy: ComboBox.WidestText
                onActivated: (index) => Backend.setGameLanguage(valueAt(index))
            }
        }
    }
    Footnote {   // what Choose… did, or what the first start after an update does
        visible: text !== ""
        text: Backend.notice || (Backend.showFirstRun ? Backend.t("first_run_warning") : "")
    }

    FolderDialog {
        id: folderDialog
        title: Backend.t("choose_folder_title")
        currentFolder: Backend.gameDir ? "file://" + Backend.gameDir : ""
        onAccepted: Backend.chooseFolder(selectedFolder.toString())
    }
    Shortcut {   // Return presses the primary button, as on a Mac's default button
        sequences: ["Return", "Enter"]
        enabled: page.visible && playButton.enabled
        onActivated: page.primary()
    }
}

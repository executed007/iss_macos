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
    function primary() { Backend.folderValid ? Backend.play() : folderDialog.open() }

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
                text: Backend.folderValid ? "Play" : "Choose game folder…"
                enabled: !Backend.folderValid || Backend.canPlay
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
        SettingRow { label: "Mac"; Value { text: Backend.gpuSummary } }
        SettingRow {   // a problem with the folder (none, wrong version) shows here, with its fix
            label: "Game"
            Value { text: Backend.gameSummary }
            Button { text: Backend.folderValid ? "Change…" : "Choose…"; onClicked: folderDialog.open() }
        }
        SettingRow { visible: !Backend.portBuilt; label: Backend.t("chip_port_missing") }
        SettingRow {   // the data folder has no saves but another folder does (typically after an update)
            visible: Saves.homeHint !== ""
            label: Saves.homeHint
            Button { text: "Show Saves"; onClicked: page.openSaves() }
        }
        SettingRow { label: "Shader cache"; Value { text: Backend.cacheState } }
        SettingRow { label: "Cheats"; Value { text: Backend.cheatsOnCount + " on" } }
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
        objectName: "folderDialog"
        title: Backend.t("choose_folder_title")
        // a file URL, each folder name encoded: a space or a '#' in the path stays part of it
        currentFolder: Backend.gameDir ? "file://" + Backend.gameDir.split("/").map(encodeURIComponent).join("/") : ""
        onAccepted: Backend.chooseFolder(selectedFolder.toString())
    }
    Shortcut {   // Return presses the primary button, as on a Mac's default button
        sequences: ["Return", "Enter"]
        enabled: page.visible && playButton.enabled
        onActivated: page.primary()
    }
}

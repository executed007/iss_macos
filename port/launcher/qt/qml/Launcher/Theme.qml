pragma Singleton
import QtQuick

// The launcher's look in one place: semantic colors with light and dark values (following the Mac's
// appearance), Apple's type scale and the 8 pt spacing grid. Screens use only these tokens; every Text
// already uses the system font. Surfaces are the macOS 26 values; text keeps at least 4.5:1 on all of them.
QtObject {
    // Follows macOS; a test, or a forced appearance, may set it.
    property int colorScheme: Qt.styleHints.colorScheme
    readonly property bool dark: colorScheme === Qt.ColorScheme.Dark

    readonly property color windowBg: dark ? "#1e1e1e" : "#ffffff"
    readonly property color sidebarBg: dark ? "#292929" : "#f7f7f7"
    readonly property color groupBg: dark ? "#262626" : "#f7f7f7"
    readonly property color separator: dark ? "#333333" : "#e6e6e6"
    readonly property color selection: dark ? "#464646" : "#dcdcdc"     // the selected sidebar row
    readonly property color textPrimary: dark ? "#dddddd" : "#262626"
    readonly property color textSecondary: dark ? "#9d9d9d" : "#6e6e6e"
    // The one accent: the Mac's accent color (Apple blue off screen); white text reads on it.
    readonly property SystemPalette system: SystemPalette { }
    readonly property color accent: Qt.platform.pluginName === "cocoa" ? system.accent : "#0a84ff"
    readonly property color textOnAccent: "#ffffff"

    readonly property int body: 13
    readonly property int caption: 11
    readonly property int title: 20
    readonly property int s1: 8
    readonly property int s2: 16
    readonly property int s3: 24
    readonly property int radius: 10

    // ---- Old screens only: their names and values until Tasks 2-5 replace them (they draw on `bg`). ----
    readonly property color bg: "#101216"
    readonly property color navHover: "#1a1d23"
    readonly property color navOn: "#1d2027"
    readonly property color card: "#15181d"
    readonly property color cardLine: "#262b33"
    readonly property color chip: "#171a20"
    readonly property color chipLine: "#2a2f38"
    readonly property color ghost: "#1a1d23"
    readonly property color ghostHover: "#22262e"
    readonly property color ghostLine: "#303641"
    readonly property color ghostLineHover: "#444b59"
    readonly property color text: "#e9e6df"
    readonly property color textSoft: "#c9c5bd"
    readonly property color textChip: "#d8d5ce"
    readonly property color textMuted: "#a3a9b3"
    readonly property color textDim: "#8e949f"
    readonly property color textFaint: "#7d838e"
    readonly property color ok: "#35d07f"
    readonly property color warn: "#f5c400"
    readonly property color bad: "#ff4d4d"
    readonly property color off: "#5a606b"
    readonly property color smoke: "#ff6a2b"
    readonly property color video: "#4d8bff"
    readonly property color neon: "#ff3d9a"
    readonly property color cyan: "#19e3ff"
    readonly property var accents: ({ advanced: "#8e7bff", mods: "#2bd9c4" })
    readonly property string fontDisplay: Qt.application.font.family
    readonly property string fontText: Qt.application.font.family
    readonly property int heroHeight: 320
    readonly property var icons: ({ saves: "M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z M12 10.5v5M9.5 13.5 12 16l2.5-2.5" })
    // Reading Backend.lang makes every binding that calls tr() follow a language switch.
    function tr(key) { var l = Backend.lang; return Backend.t(key) }
}

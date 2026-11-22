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
    // The one accent: the Mac's accent color (Apple blue off screen).
    readonly property SystemPalette system: SystemPalette { }
    readonly property color accent: Qt.platform.pluginName === "cocoa" ? system.accent : "#0a84ff"
    readonly property color textOnAccent: textOn(accent)
    // For 14 pt semibold labels, which are WCAG large text.
    readonly property color textOnAccentLarge: textOnLarge(accent)
    function luminance(fill) {
        const c = Qt.color(fill)
        const linear = v => v <= 0.04045 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4)
        return 0.2126 * linear(c.r) + 0.7152 * linear(c.g) + 0.0722 * linear(c.b)
    }
    // Black or white, whichever contrasts more with the fill (WCAG luminance): black on yellow, orange, green
    // or graphite, white on deep blue (this Mac's #0a60ff) or purple. At least 4.58:1 whatever the accent.
    function textOn(fill) {
        const l = luminance(fill)
        return (l + 0.05) / 0.05 > 1.05 / (l + 0.05) ? "#000000" : "#ffffff"
    }
    // Large text needs only 3:1, so white wherever it reaches that (white on #0a84ff is 3.65:1); else as textOn.
    function textOnLarge(fill) { return 1.05 / (luminance(fill) + 0.05) >= 3 ? "#ffffff" : textOn(fill) }

    readonly property int body: 13
    readonly property int caption: 11
    readonly property int headline: 13      // section headers, semibold, as in macOS 26 System Settings
    readonly property int callout: 14       // semibold: the Play capsule's label, large text for contrast
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
    readonly property var icons: ({ saves: "M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z M12 10.5v5M9.5 13.5 12 16l2.5-2.5" })
    // Reading Backend.lang makes every binding that calls tr() follow a language switch.
    function tr(key) { var l = Backend.lang; return Backend.t(key) }
}

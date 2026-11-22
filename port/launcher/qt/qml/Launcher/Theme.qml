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

    // Status text: the result of an action. At least 4.5:1 on the window and the grouped background.
    readonly property color success: dark ? "#30d158" : "#177a31"
    readonly property color danger: dark ? "#ff6961" : "#c4001b"
    function toneColor(tone) { return tone === "bad" ? danger : tone === "good" ? success : textPrimary }

    function tr(key) { return Backend.t(key) }
}

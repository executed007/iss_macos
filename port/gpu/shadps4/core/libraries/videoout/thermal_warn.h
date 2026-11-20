// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// macOS thermal state (0 nominal, 1 fair, 2 serious, 3 critical): true when `state` is serious or
/// critical and higher than the previous one, so each upward transition warns once. Updates `last`.
inline bool ThermalWarn(long state, long& last) {
    const bool warn = state >= 2 && state > last;
    last = state;
    return warn;
}

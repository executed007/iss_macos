# SPDX-License-Identifier: GPL-2.0-or-later
"""Presets for a fanless MacBook Air M5 (16 GB unified memory) running the x86_64 build under Rosetta 2.

Values from the M5 Air measurements (8l to 8o). The internal resolution (tlg.ini) is not a preset item:
each text names the percentage to pick. The texture budget is
the launcher's rule for 8 GB (half of the unified memory)."""
from presets_current import CURRENT

_BUDGET_MB = 5000      # gfxlogic.suggested_budget_mb(8)


def _profile(profile_id, name, what, vsync, items):
    return {
        'id': profile_id,
        'exported': '2026-11-20',
        'names': {'en': name, 'pt': name},
        'what': {'en': what, 'pt': what},
        'credit': 'macOS port',
        'settings': dict(CURRENT['settings'], vsync=vsync, gpu_readbacks='2',
                         vram_budget_mb=_BUDGET_MB, vram_auto=False),
        'items': dict(CURRENT['items'], **items),
    }


_BAL = {'menu:fps_limit': '30', 'menu:fsr1': '1', 'menu:rcas': '1'}

M5AIR_BATTERY = _profile(
    'm5air_battery', 'M5 AIR · BATTERY',
    'MacBook Air M5: the Balanced settings with quality level 12. Pick 35% in the resolution control. '
    'Level 12 kept every object at 50% (8m); at 35% and its power use are unmeasured.',
    True, dict(_BAL, **{'menu:quality_floor': '12'}))

M5AIR_BALANCED = _profile(
    'm5air_balanced', 'M5 AIR · BALANCED',
    'MacBook Air M5, recommended: 30 FPS cap, FSR 1 and sharpening, quality level 8. Pick 35% in the '
    'resolution control: a steady 30 FPS (lowest scene 29.8, 8o), every object kept, softer picture.',
    True, dict(_BAL, **{'menu:quality_floor': '8'}))

M5AIR_PERFORMANCE = _profile(
    'm5air_performance', 'M5 AIR · PERFORMANCE',
    'MacBook Air M5: no FPS cap, FSR 1 and sharpening, quality level 8. Pick 50% in the resolution '
    'control: 27.4 FPS (8m), sharper. Fanless Mac: it may throttle in long sessions.',
    True, {'menu:fsr1': '1', 'menu:rcas': '1', 'menu:quality_floor': '8'})

M5AIR_PRESETS = (M5AIR_BATTERY, M5AIR_BALANCED, M5AIR_PERFORMANCE)

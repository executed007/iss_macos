# SPDX-License-Identifier: GPL-2.0-or-later
"""Presets for a fanless MacBook Air M5 (16 GB unified memory) running the x86_64 build under Rosetta 2.

PROVISIONAL starting values: Task 10 measures them on the machine and adjusts. The texture budget is
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


M5AIR_BATTERY = _profile(
    'm5air_battery', 'M5 AIR · BATTERY',
    'MacBook Air M5: 30 FPS cap, FSR 1 and sharpening, reduced detail. Coolest and quietest.',
    True, {'menu:fps_limit': '30', 'menu:fsr1': '1', 'menu:rcas': '1', 'menu:lod_scale': '0.45',
           'menu:far_clip': '400000'})

M5AIR_BALANCED = _profile(
    'm5air_balanced', 'M5 AIR · BALANCED',
    'MacBook Air M5: the default. 30 FPS cap, FSR 1, medium detail (the 60 FPS cap waits for measurements).',
    True, {'menu:fps_limit': '30', 'menu:fsr1': '1', 'menu:lod_scale': '0.65'})

M5AIR_PERFORMANCE = _profile(
    'm5air_performance', 'M5 AIR · PERFORMANCE',
    'MacBook Air M5: no FPS cap, no upscaler, native detail, vsync off. '
    'Fanless Mac: expect throttling after a few minutes.',
    False, {'menu:fsr1': '0'})

M5AIR_PRESETS = (M5AIR_BATTERY, M5AIR_BALANCED, M5AIR_PERFORMANCE)

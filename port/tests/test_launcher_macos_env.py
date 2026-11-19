# SPDX-License-Identifier: GPL-2.0-or-later
"""probe_env() on macOS: Rosetta advertises AVX, the Vulkan loader is pinned to MoltenVK (never
KosmicKrisp) unless the player chose a driver, and no LD_LIBRARY_PATH."""
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))  # also runs as tests.test_launcher_macos_env
from paths import ROOT  # noqa: E402
sys.path.insert(0, str(ROOT / 'launcher'))
import infamous_run  # noqa: E402


class MacEnvTests(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.sdk = Path(tmp.name)
        vulkan = self.sdk / 'share' / 'vulkan'
        (vulkan / 'icd.d').mkdir(parents=True)
        (vulkan / 'explicit_layer.d').mkdir()
        (vulkan / 'icd.d' / 'MoltenVK_icd.json').write_text('{}')

    def env(self, **environ):
        with mock.patch.object(sys, 'platform', 'darwin'), \
                mock.patch.dict('os.environ', {'VULKAN_SDK': str(self.sdk), **environ}, clear=True):
            return infamous_run.probe_env()

    def test_darwin_pins_moltenvk(self):
        env = self.env()
        self.assertEqual('1', env['ROSETTA_ADVERTISE_AVX'])
        self.assertTrue(env['VK_ICD_FILENAMES'].endswith('MoltenVK_icd.json'))
        self.assertEqual(str(self.sdk / 'share' / 'vulkan' / 'explicit_layer.d'), env['VK_LAYER_PATH'])
        self.assertNotIn('LD_LIBRARY_PATH', env)

    def test_player_driver_is_kept(self):
        self.assertEqual('/x/icd.json', self.env(VK_ICD_FILENAMES='/x/icd.json')['VK_ICD_FILENAMES'])
        env = self.env(VK_DRIVER_FILES='/y/icd.json')
        self.assertEqual('/y/icd.json', env['VK_DRIVER_FILES'])
        self.assertNotIn('VK_ICD_FILENAMES', env)
        self.assertEqual('/l', self.env(VK_LAYER_PATH='/l')['VK_LAYER_PATH'])

    def test_game_environment_has_them(self):
        # build_env() is what the game is started with; it once dropped everything but LD_LIBRARY_PATH.
        with mock.patch.object(sys, 'platform', 'darwin'), \
                mock.patch.dict('os.environ', {'VULKAN_SDK': str(self.sdk)}, clear=True), \
                mock.patch.object(infamous_run, 'pad_glyphs', return_value=''):
            env = infamous_run.build_env(dict(infamous_run.DEFAULT_SETTINGS))
        self.assertEqual('1', env['ROSETTA_ADVERTISE_AVX'])
        self.assertTrue(env['VK_ICD_FILENAMES'].endswith('MoltenVK_icd.json'))
        self.assertNotIn('LD_LIBRARY_PATH', env)


if __name__ == '__main__':
    unittest.main()

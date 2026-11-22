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

    def test_patched_moltenvk_preferred(self):
        # port/out/moltenvk/MoltenVK_icd.json (scripts/macos/build_moltenvk.sh) wins over the SDK's, not over the player's.
        patched = Path(self.sdk) / 'patched_icd.json'
        patched.write_text('{}')
        with mock.patch.object(infamous_run, 'PATCHED_ICD', patched):
            self.assertEqual(str(patched), self.env()['VK_ICD_FILENAMES'])
            self.assertEqual('/x/icd.json', self.env(VK_ICD_FILENAMES='/x/icd.json')['VK_ICD_FILENAMES'])
            self.assertNotIn('VK_ICD_FILENAMES', self.env(VK_DRIVER_FILES='/y/icd.json'))
        with mock.patch.object(infamous_run, 'PATCHED_ICD', self.sdk / 'missing.json'):
            self.assertTrue(self.env()['VK_ICD_FILENAMES'].startswith(str(self.sdk)))

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


class Bp08DefaultTests(unittest.TestCase):
    def test_launcher_env_turns_bp08_off_by_default(self):
        with mock.patch.dict('os.environ', {}, clear=True):
            env = infamous_run.build_env(dict(infamous_run.DEFAULT_SETTINGS))
        self.assertEqual('0', env['TLG_BP08'])

    def test_player_choice_in_the_environment_wins(self):
        with mock.patch.dict('os.environ', {'TLG_BP08': '1'}, clear=True):
            env = infamous_run.build_env(dict(infamous_run.DEFAULT_SETTINGS))
        self.assertEqual('1', env['TLG_BP08'])

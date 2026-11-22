"""GoldHEN cheat JSON reading, original-byte guard, and the 1.07 mods version."""
import hashlib
import io
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path[:0] = [str(ROOT / 'scripts'), str(ROOT / 'launcher')]
import mod_patches
import infamous_patches

IDS = {'CUSA00004', 'CUSA00223'}
BUILTIN = (ROOT / 'patches' / 'iss_builtin_intro.xml',)
VERIFIED = 'b3afd173a379f83e1e408e71fdef22b78f93092d332b79decd48a18a08bfc605'


def fake_elf(code):
    elf = bytearray(0x1000) + code
    struct.pack_into('<Q', elf, 0x20, 0x40)
    struct.pack_into('<HH', elf, 0x36, 0x38, 1)
    struct.pack_into('<IIQQQQQQ', elf, 0x40, 1, 5, 0x1000, 0, 0, len(code), len(code), 0)
    return bytes(elf)


def cheat_file(directory, **over):
    data = {'name': 'inFAMOUS', 'id': 'CUSA00004', 'version': '01.07', 'process': 'eboot.bin',
            'mods': [{'name': 'Infinite Shards', 'type': 'checkbox',
                      'memory': [{'offset': '10', 'on': '9090', 'off': 'C4C1'}]},
                     {'name': 'Max Level', 'type': 'button',
                      'memory': [{'offset': 'EFADB8', 'on': '00', 'off': '00'}]}]}
    data.update(over)
    (Path(directory) / 'CUSA00004_01.07.json').write_text(json.dumps(data))


class GoldhenTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.d = self.temp.name
        cheat_file(self.d)

    def test_checkbox_becomes_patch_and_button_is_skipped(self):
        found = mod_patches.external_patches(self.d, '01.07', IDS)
        self.assertEqual(len(found), 1)
        key, _, meta = found[0]
        self.assertEqual(key, 'CUSA00004_01.07.json/Infinite Shards')
        lines = list(meta.iter('Line'))
        self.assertEqual(len(lines), 1)
        self.assertEqual((lines[0].get('Address'), lines[0].get('Value'), lines[0].get('Original')),
                         ('0x400010', '9090', 'C4C1'))

    def test_other_version_or_title_not_offered(self):
        cheat_file(self.d, version='01.06')
        self.assertEqual(mod_patches.external_patches(self.d, '01.07', IDS), [])
        cheat_file(self.d, id='CUSA99999')
        self.assertEqual(mod_patches.external_patches(self.d, '01.07', IDS), [])

    def test_malformed_and_foreign_json_skipped(self):
        (Path(self.d) / 'broken.json').write_text('{oops')
        (Path(self.d) / 'mods.json').write_text('{"enabled": []}')
        (Path(self.d) / 'foreign.json').write_text('{"mods": ["oops"]}')
        with mock.patch('sys.stderr', new_callable=io.StringIO) as err:
            self.assertEqual(len(mod_patches.external_patches(self.d, '01.07', IDS)), 1)
        self.assertIn('Mods: broken.json:', err.getvalue())

    def test_bad_entries_dropped_others_listed(self):
        cheat_file(self.d, mods=[
            {'name': 'BadHex', 'type': 'checkbox', 'memory': [{'offset': '10', 'on': '90', 'off': 'C4C'}]},
            'oops',
            {'name': 'Empty', 'type': 'checkbox', 'memory': []},
            {'name': 'Good', 'type': 'checkbox', 'memory': [{'offset': '10', 'on': '9090', 'off': 'C4C1'}]}])
        elf = fake_elf(b'\0' * 0x10 + bytes.fromhex('C4C1'))
        with mock.patch('sys.stderr', new_callable=io.StringIO) as err:
            found = mod_patches.external_patches(self.d, '01.07', IDS, elf=elf)
        self.assertEqual([k for k, _, _ in found], ['CUSA00004_01.07.json/Good'])
        self.assertIn('Mods: CUSA00004_01.07.json/BadHex:', err.getvalue())
        self.assertIn('Empty', err.getvalue())

    def test_original_mismatch_not_offered(self):
        ok = fake_elf(b'\0' * 0x10 + bytes.fromhex('C4C1'))
        bad = fake_elf(b'\0' * 0x10 + bytes.fromhex('FFFF'))
        self.assertEqual(len(mod_patches.external_patches(self.d, '01.07', IDS, elf=ok)), 1)
        self.assertEqual(mod_patches.external_patches(self.d, '01.07', IDS, elf=bad), [])

    def test_compile_checks_original(self):
        found = mod_patches.external_patches(self.d, '01.07', IDS)
        ok = fake_elf(b'\0' * 0x10 + bytes.fromhex('C4C1'))
        bad = fake_elf(b'\0' * 0x10 + bytes.fromhex('FFFF'))
        got = mod_patches.compile_external(found, [(0, 0x12)], eboot=ok)
        self.assertEqual([(o, d) for o, d, _ in got], [(0x10, b'\x90\x90')])
        self.assertEqual(mod_patches.compile_external(found, [(0, 0x12)], eboot=bad), [])

    def test_untouched_folder_changes_nothing(self):
        with tempfile.TemporaryDirectory() as empty:
            for v in ('01.00', '01.07'):
                found = mod_patches.external_patches(empty, v, IDS, builtin_files=BUILTIN)
                self.assertEqual(mod_patches.external_selection(found, None), [])

    def test_mods_app_version_for_verified_eu_dump(self):
        import prepare
        game = Path(self.d) / 'game'
        (game / 'sce_sys').mkdir(parents=True)
        (game / 'sce_sys/param.sfo').write_bytes(b'fixture')
        cases = (({'TITLE_ID': 'CUSA00004', 'APP_VER': '01.00'}, VERIFIED, '01.07'),
                 ({'TITLE_ID': 'CUSA00004', 'APP_VER': '01.00'}, '0' * 64, '01.00'),
                 ({'TITLE_ID': 'CUSA00223', 'APP_VER': '01.07'}, '0' * 64, '01.07'))
        for sfo, digest_value, expected in cases:
            with mock.patch.object(prepare, 'sfo', return_value=sfo), \
                 mock.patch.object(infamous_patches, 'read_eboot_elf', return_value=b'fixture'), \
                 mock.patch.object(hashlib, 'sha256') as digest:
                digest.return_value.hexdigest.return_value = digest_value
                self.assertEqual(infamous_patches.mods_app_version(game), expected)


if __name__ == '__main__':
    unittest.main()

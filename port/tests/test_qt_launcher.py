"""The Qt launcher's logic (launcher/qt/logic.py, backend.py): translations, chips, news, saves,
fix groups. The pure-Python parts need no PySide6; the Backend tests skip without it."""
import os
import json
import re
import struct
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock
from pathlib import Path

from paths import ROOT
sys.path.insert(0, str(ROOT / 'launcher'))
from qt import logic  # noqa: E402

QT = ROOT / 'launcher' / 'qt'
STRINGS = json.loads((QT / 'i18n' / 'strings.json').read_text(encoding='utf-8'))
PT = {'ui_language': 'pt_BR'}
EN = {'ui_language': 'en_US'}


def placeholders(text):
    return set(re.findall(r'{(\w+)}', text))


def make_sfo(entries):
    """A minimal PSF (param.sfo / param.bin) with UTF-8 string entries."""
    keys, values, index = b'', b'', b''
    for key, value in entries.items():
        data = value.encode() + b'\0'
        index += struct.pack('<HHIII', len(keys), 0x0204, len(data), len(data), len(values))
        keys += key.encode() + b'\0'
        values += data
    start = 20 + len(index)
    return struct.pack('<4sIIII', b'\0PSF', 0x101, start, start + len(keys), len(entries)) + index + keys + values


class TranslationTests(unittest.TestCase):
    def test_every_key_has_both_languages(self):
        for key, entry in STRINGS.items():
            self.assertEqual({'en', 'pt'}, set(entry), key)
            self.assertTrue(entry['en'].strip() and entry['pt'].strip(), key)

    def test_placeholders_match_between_languages(self):
        for key, entry in STRINGS.items():
            self.assertEqual(placeholders(entry['en']), placeholders(entry['pt']), key)

    def test_no_key_hides_a_tk_key_with_other_placeholders(self):
        tk = logic.tk_text()
        for key in set(STRINGS) & set(tk):
            self.assertEqual(placeholders(tk[key][0]), placeholders(STRINGS[key]['en']), key)

    def test_keys_used_by_the_interface_exist(self):
        tk = logic.tk_text()
        used = set()
        for path in list(QT.rglob('*.qml')) + [QT / 'backend.py', QT / 'logic.py']:
            text = path.read_text(encoding='utf-8')
            used |= set(re.findall(r'(?:tr\(|t_\(|\bt\()["\'](\w+)["\']', text))
        used |= {'nav_' + n for n in ('play', 'graphics', 'controls', 'fixes', 'saves', 'guides', 'support', 'updates', 'advanced')}
        for key in (k for k in used if not k.endswith('_')):
            self.assertTrue(key in STRINGS or key in tk, key)

    def test_translator_picks_language_and_falls_back_to_tk_text(self):
        pt, en = logic.Translator(PT), logic.Translator(EN)
        self.assertEqual('Jogar', pt('nav_play'))
        self.assertEqual('Play', en('nav_play'))
        self.assertEqual(logic.tk_text()['exp_live'][1], pt('exp_live'))   # reused from the Tk launcher
        self.assertEqual('Cache de shaders: 1 MB', pt('chip_cache', size='1 MB'))
        self.assertEqual('no_such_key', en('no_such_key'))


class ChipTests(unittest.TestCase):
    def chips(self, settings=PT, **kw):
        kw.setdefault('states', {})
        kw.setdefault('problems', [])
        kw.setdefault('cache_bytes', 0)
        settings = dict(settings, game_dir=kw.pop('game_dir', '/some/game'))
        return logic.compute_chips(settings, logic.Translator(settings), **kw)

    def test_everything_ready(self):
        chips, ready = self.chips(gpu=('NVIDIA GeForce RTX 2060', '1.3'), pad='DualSense',
                                  cache_bytes=218 * 2 ** 20)
        self.assertTrue(ready)
        texts = [c['text'] for c in chips]
        self.assertIn('NVIDIA GeForce RTX 2060 · Vulkan 1.3', texts)
        self.assertIn('Pasta do jogo OK · update 1.07', texts)
        self.assertIn('Perfil: Recomendado', texts)
        self.assertIn('Controle: DualSense', texts)
        self.assertTrue(any(t.startswith('Cache de shaders: 218') for t in texts))
        self.assertTrue(all(c['level'] == 'ok' for c in chips))

    def test_missing_game_folder_blocks(self):
        chips, ready = self.chips(game_dir='', gpu=None)
        self.assertFalse(ready)
        self.assertEqual('bad', chips[1]['level'])
        self.assertEqual('warn', chips[0]['level'])

    def test_blocking_problem_names_the_file(self):
        chips, ready = self.chips(problems=[('missing', 'eboot.bin')], gpu='checking')
        self.assertFalse(ready)
        self.assertIn('eboot.bin', chips[1]['text'])

    def test_unbuilt_port_is_its_own_chip_not_a_game_problem(self):
        chips, ready = self.chips(problems=[('not_built', '/x/tlg-probe')])
        self.assertFalse(ready)
        texts = [c['text'] for c in chips]
        self.assertIn('Pasta do jogo OK · update 1.07', texts)
        self.assertIn('Port não compilado (rode o build.sh)', texts)

    def test_profile_counts_changes_like_the_tk_window(self):
        chips, _ = self.chips(states={'BP06': '1', 'BP01': '0'})
        self.assertIn('Perfil: personalizado (2 mudanças)', [c['text'] for c in chips])

    def test_english(self):
        chips, _ = self.chips(EN, gpu=('GPU', ''), pad=None)
        texts = [c['text'] for c in chips]
        self.assertIn('Game folder OK · update 1.07', texts)
        self.assertIn('No controller detected', texts)


class ProbeParsingTests(unittest.TestCase):
    SUMMARY = """Devices:
========
GPU0:
\tapiVersion         = 1.4.318
\tdriverVersion      = 595.1.0.0
\tdeviceType         = PHYSICAL_DEVICE_TYPE_CPU
\tdeviceName         = llvmpipe (LLVM 19.1.7, 256 bits)
GPU1:
\tapiVersion         = 1.3.289
\tdeviceType         = PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
\tdeviceName         = NVIDIA GeForce RTX 2060
"""

    def test_prefers_the_discrete_gpu(self):
        self.assertEqual(('NVIDIA GeForce RTX 2060', '1.3'), logic.parse_vulkaninfo(self.SUMMARY))

    def test_no_devices(self):
        self.assertIsNone(logic.parse_vulkaninfo('nothing useful'))

    def test_controller_names(self):
        text = 'N: Name="Sony Interactive Entertainment DualSense Wireless Controller"\nN: Name="DualSense Touchpad"\n'
        self.assertEqual('DualSense', logic.detect_pad(text))
        self.assertEqual('Xbox', logic.detect_pad('N: Name="Microsoft Xbox Series pad"\n'))
        for name, label in (('XInput Controller #1', 'Xbox'), ('Controller (Xbox One For Windows)', 'Xbox'),
                            ('8BitDo Ultimate 2 Wireless Controller', '8BitDo'),
                            ('Nintendo Switch Pro Controller', 'Switch Pro'), ('Wireless Controller', 'DualShock 4')):
            self.assertEqual(label, logic.detect_pad(f'N: Name="{name}"\n'), name)
        self.assertIsNone(logic.detect_pad('N: Name="Logitech USB Keyboard"\nN: Name="Power Button"\n'))

    def test_guide_for_detected_pads(self):
        for name, kind in (('Xbox', 'xbox'), ('8BitDo', '8bitdo'), ('Switch Pro', 'switch'), ('DualSense', 'xbox')):
            self.assertEqual(kind, logic.guide_kind(name))

    def test_controller_from_the_probe(self):
        from unittest import mock
        class R:
            stdout = 'Gamepad: type=standard south=a east=b west=x north=y name=XInput Controller #1\n'
        with mock.patch.object(logic.subprocess, 'run', return_value=R()):
            self.assertEqual('Xbox', logic.detect_pad())


class NewsTests(unittest.TestCase):
    README = ('# x\n\n## Novidades\n\n- **Primeira coisa (#51):** detalhe longo.\n- Segunda sem negrito.\n'
              '- **Terceira [com link](http://x)** mais\n- **Quarta:** não entra\n\n## Status\n- **fora:** x\n'
              "\n## What's new\n\n- **First thing:** detail\n")

    def test_takes_three_bold_leads(self):
        self.assertEqual(['Primeira coisa', 'Segunda sem negrito.'.rstrip('.'), 'Terceira com link'],
                         logic.parse_news(self.README, True))

    def test_english_section(self):
        self.assertEqual(['First thing'], logic.parse_news(self.README, False))

    def test_fallback_when_readme_is_missing(self):
        t = logic.Translator(PT)
        self.assertEqual(3, len(logic.news(t, True, readme='/no/such/README.md')))

    def test_the_real_readme_has_news(self):
        upstream = ROOT.parent / 'README-UPSTREAM.md'
        self.assertEqual(upstream if upstream.is_file() else ROOT.parent / 'README.md', logic.news_readme())
        self.assertEqual(3, len(logic.parse_news(logic.news_readme().read_text(encoding='utf-8'), True)))


class SaveTests(unittest.TestCase):
    def test_title_from_param_bin(self):
        with tempfile.TemporaryDirectory() as folder:
            slot = Path(folder) / '1' / 'CUSA00223' / 'sce_sys'
            slot.mkdir(parents=True)
            (slot / 'param.bin').write_bytes(make_sfo({'SUBTITLE': 'Welcome to Seattle', 'DETAIL': '8%'}))
            self.assertEqual('Welcome to Seattle — 8%', logic.last_save_title(folder))

    def test_hidden_without_a_save_or_with_a_damaged_one(self):
        with tempfile.TemporaryDirectory() as folder:
            self.assertIsNone(logic.last_save_title(folder))
            self.assertIsNone(logic.last_save_title(Path(folder) / 'nope'))
            (Path(folder) / 'param.bin').write_bytes(b'garbage')
            self.assertIsNone(logic.last_save_title(folder))


class FixGroupTests(unittest.TestCase):
    def test_every_fix_is_in_a_known_group(self):
        for item in logic.fix_items():
            self.assertIn(logic.group_of(item), logic.GROUP_ORDER, item.id)
            self.assertIn(item.id, logic._GROUP_OF, item.id)   # a new switch must be placed on purpose

    def test_all_twelve_backports_and_the_ab_switches_are_listed(self):
        ids = {i.id for i in logic.fix_items()}
        self.assertTrue({f'BP{n:02d}' for n in range(1, 13)} <= ids)
        self.assertTrue({i.id for i in logic.backports_table.DIAGNOSTICS} <= ids)

    def test_risky_and_broken_items_are_experimental(self):
        for item in logic.fix_items():
            if item.risky or item.broken:
                self.assertEqual('exp', logic.group_of(item), item.id)


try:
    from qt.backend import Backend
    HAVE_QT = True
except ImportError:
    HAVE_QT = False


@unittest.skipUnless(HAVE_QT, 'PySide6 not installed')
class BackendTests(unittest.TestCase):
    def make(self, **settings):
        saved = []
        probes = {'gpu': lambda: ('GPU', '1.3'), 'pad': lambda: None, 'problems': lambda: [],
                  'cache': lambda: 0, 'save': lambda: 'Welcome to Seattle — 8%', 'news': lambda: ['a', 'b', 'c'],
                  'version': lambda: 'v1', 'save_fixes': lambda s, st: saved.append(dict(st))}
        base = dict(logic.tlg_run.DEFAULT_SETTINGS, game_dir='/g', ui_language='pt_BR', **settings)
        return Backend(settings=base, probes=probes), saved

    def test_play_subtitle_continues_the_save(self):
        backend, _ = self.make()
        self.assertTrue(backend.canPlay)
        self.assertEqual('Continue · Welcome to Seattle — 8%', backend.playSubtitle)

    def test_key_art_fallback(self):
        from PySide6.QtCore import QUrl
        backend, _ = self.make()
        with tempfile.TemporaryDirectory() as folder:
            art = Path(folder) / 'Extracted Game' / 'sce_sys'      # a space, as in the real dump's folder
            art.mkdir(parents=True)
            for name in ('pic0.png', 'icon0.png'):
                (art / name).write_bytes(b'png')
            backend.settings['game_dir'] = str(art.parent)
            self.assertTrue(backend.keyArtUrl.startswith('file://'), backend.keyArtUrl)
            self.assertTrue(backend.keyArtUrl.endswith('/sce_sys/pic0.png'), backend.keyArtUrl)
            self.assertEqual(str(art / 'pic0.png'), QUrl(backend.keyArtUrl).toLocalFile())
            for name in ('pic0.png', 'icon0.png'):
                (art / name).unlink()
            self.assertEqual('', backend.keyArtUrl)
        backend.settings['game_dir'] = ''
        self.assertEqual('', backend.keyArtUrl)

    def test_folder_valid_and_port_built(self):
        backend, _ = self.make()
        self.assertEqual((True, True), (backend.folderValid, backend.portBuilt))
        backend.probes['problems'] = lambda: [('missing', 'eboot.bin')]
        backend.refresh()
        self.assertEqual((False, True), (backend.folderValid, backend.portBuilt))
        backend.probes['problems'] = lambda: [('not_built', '/x/tlg-probe')]
        backend.refresh()
        self.assertEqual((True, False), (backend.folderValid, backend.portBuilt))
        backend.probes['problems'] = lambda: []
        backend.settings['game_dir'] = ''
        backend.refresh()
        self.assertFalse(backend.folderValid)

    def test_gpu_summary_names_the_chip_and_metal(self):
        backend, _ = self.make()
        self.assertEqual(backend.t_('chip_gpu_checking'), backend.gpuSummary)
        backend._on_gpu(('Apple M5', '1.4'))
        self.assertEqual('Apple M5 · Metal', backend.gpuSummary)
        backend._on_gpu(None)
        self.assertEqual(backend.t_('chip_gpu_unknown'), backend.gpuSummary)

    def test_game_summary_is_title_id_and_version_or_the_problem(self):
        backend, _ = self.make()
        with tempfile.TemporaryDirectory() as folder:
            (Path(folder) / 'sce_sys').mkdir()
            (Path(folder) / 'sce_sys' / 'param.sfo').write_bytes(make_sfo({'TITLE_ID': 'CUSA00004'}))
            backend.settings['game_dir'] = folder
            backend.refresh()
            self.assertEqual('CUSA00004 · v1.07', backend.gameSummary)
        backend.probes['problems'] = lambda: [('missing', 'eboot.bin')]
        backend.refresh()
        self.assertEqual(logic.game_problem_text(backend.t_, [('missing', 'eboot.bin')]), backend.gameSummary)
        backend.settings['game_dir'] = ''
        backend.refresh()
        self.assertEqual(backend.t_('play_game_none'), backend.gameSummary)

    def test_cache_state(self):
        backend, _ = self.make()                          # the probe measures 0 bytes
        self.assertEqual(backend.t_('play_cache_empty'), backend.cacheState)
        backend._on_cache_measured(218 * 2 ** 20)
        self.assertEqual('218.0 MB', backend.cacheState)
        backend._cache_bytes = None                       # before the first measure ends
        self.assertEqual(backend.t_('play_cache_measuring'), backend.cacheState)

    def wait_for_mods(self, backend):
        """Runs the event loop until the worker that fills the first listing has delivered it."""
        import time
        from PySide6.QtCore import QCoreApplication
        app = QCoreApplication.instance() or QCoreApplication([])
        end = time.time() + 5
        while backend.modsLoading and time.time() < end:
            app.processEvents()
            time.sleep(0.01)
        self.assertFalse(backend.modsLoading)

    def test_mods_listing_is_read_once_until_it_changes(self):
        backend, _ = self.make()
        rows = [('a.xml/One', 'One', '', True), ('a.xml/Two', 'Two', '', False)]
        with tempfile.TemporaryDirectory() as mods, mock.patch.dict('os.environ', {'ISS_MODS_DIR': mods}), \
                mock.patch.object(logic.tlg_run, 'list_mods', return_value=rows) as list_mods, \
                mock.patch.object(logic.tlg_run, 'set_mod_enabled'):
            backend.modsModel                                         # the first read starts the worker
            self.wait_for_mods(backend)
            for _ in range(3):
                self.assertEqual(['One', 'Two'], [m['name'] for m in backend.modsModel])
                self.assertEqual(1, backend.cheatsOnCount)
            self.assertEqual(1, list_mods.call_count)
            backend.setModEnabled('a.xml/Two', True)                  # a toggle
            self.assertEqual(1, backend.cheatsOnCount)
            self.assertEqual(2, list_mods.call_count)
            backend.settings['game_dir'] = '/another/game'            # another game folder
            self.assertEqual(2, len(backend.modsModel))
            self.assertEqual(3, list_mods.call_count)
            stat = os.stat(mods)                                      # a cheat file added to the mods folder
            (Path(mods) / 'more.xml').write_text('<Patch/>', encoding='utf-8')
            os.utime(mods, ns=(stat.st_atime_ns, stat.st_mtime_ns + 10 ** 9))
            self.assertEqual(1, backend.cheatsOnCount)
            self.assertEqual(2, len(backend.modsModel))
            self.assertEqual(4, list_mods.call_count)

    def test_first_mods_listing_is_read_off_the_gui_thread(self):
        import threading
        import time
        backend, _ = self.make()
        rows = [('a.xml/One', 'One', '', True)]
        seen = []

        def slow(settings):
            seen.append(threading.get_ident())
            time.sleep(0.3)                                           # the 133 MB eboot read
            return rows
        changed = []
        backend.modsChanged.connect(lambda: changed.append(1))
        with tempfile.TemporaryDirectory() as mods, mock.patch.dict('os.environ', {'ISS_MODS_DIR': mods}), \
                mock.patch.object(logic.tlg_run, 'list_mods', side_effect=slow):
            started = time.time()
            self.assertEqual([], backend.modsModel)                   # not ready: no rows, no waiting
            self.assertLess(time.time() - started, 0.2)
            self.assertTrue(backend.modsLoading)
            self.assertEqual(0, backend.cheatsOnCount)
            self.assertNotIn(threading.get_ident(), seen)             # never on the calling thread, not even started
            self.assertEqual([], changed)
            self.wait_for_mods(backend)
            self.assertEqual(1, len(seen))
            self.assertNotEqual(threading.get_ident(), seen[0])
            self.assertEqual(1, len(changed))                         # modsChanged fired when it finished
            self.assertEqual(['One'], [m['name'] for m in backend.modsModel])
            self.assertEqual(1, backend.cheatsOnCount)
            self.assertEqual(1, len(seen))                            # kept: not read again

    def test_a_failed_first_listing_is_empty_not_loading(self):
        backend, _ = self.make()
        with tempfile.TemporaryDirectory() as mods, mock.patch.dict('os.environ', {'ISS_MODS_DIR': mods}), \
                mock.patch.object(logic.tlg_run, 'list_mods', side_effect=OSError('no eboot')):
            backend.modsModel
            self.wait_for_mods(backend)
            self.assertEqual([], backend.modsModel)

    def test_cheats_list_the_community_cheats_before_the_built_in_options(self):
        backend, _ = self.make()
        rows = [('iss_builtin_intro.xml/Skip intro movie', 'Skip intro movie', '', False),
                ('CUSA00004_01.07.json/Godmode', 'Godmode', 'A note', False),
                ('CUSA00004_01.07.json/Infinite Energy', 'Infinite Energy', '', True),
                ('native/camera-speed-1.5x', 'Camera improvements', '', False)]
        with tempfile.TemporaryDirectory() as mods, mock.patch.dict('os.environ', {'ISS_MODS_DIR': mods}), \
                mock.patch.object(logic.tlg_run, 'list_mods', return_value=rows):
            backend.modsModel
            self.wait_for_mods(backend)
            self.assertEqual(['Godmode', 'Infinite Energy', 'Skip intro movie', 'Camera improvements'],
                             [m['name'] for m in backend.modsModel])
            self.assertEqual('A note', backend.modsModel[0]['note'])

    def test_set_fix_saves_and_profile_follows(self):
        backend, saved = self.make()
        self.assertTrue(backend.profileIsRecommended)
        backend.setFix('BP06', '0')
        self.assertEqual('0', saved[-1]['BP06'])
        self.assertFalse(backend.profileIsRecommended)
        backend.setFix('BP06', '1')            # back to its default: nothing stored
        self.assertIsNone(saved[-1]['BP06'])
        self.assertTrue(backend.profileIsRecommended)

    def test_previous_release_turns_every_default_on_backport_off(self):
        backend, saved = self.make()
        backend.resetFixes('previous')
        for item in logic.backports_table.BACKPORTS:
            self.assertEqual('0' if item.default == '1' else None, saved[-1][item.id], item.id)
        backend.resetFixes('defaults')
        self.assertTrue(backend.profileIsRecommended)

    def test_occlusion_selector_defaults_to_4_and_saves_through_the_fix_path(self):
        backend, saved = self.make()
        self.assertEqual(['0', '1', '2', '3', '4'], [o['value'] for o in backend.occlusionOptions])
        self.assertEqual(4, backend.occlusionIndex)
        backend.setOcclusion('1')
        self.assertEqual(1, backend.occlusionIndex)
        self.assertEqual('1', saved[-1]['TLG_OCCLUSION_MODE'])
        backend.setOcclusion('3')
        self.assertEqual(3, backend.occlusionIndex)
        self.assertEqual('3', saved[-1]['TLG_OCCLUSION_MODE'])
        backend.setOcclusion('4')
        self.assertEqual(4, backend.occlusionIndex)

    def test_game_language_list_is_auto_then_game_languages_in_order(self):
        backend, _ = self.make()
        self.assertEqual(['auto'] + list(logic.tlg_run._DEFAULT_LANGUAGES), [o['value'] for o in backend.gameLanguageOptions])
        self.assertEqual(7, len(backend.gameLanguageOptions))
        self.assertEqual('Automatic (system language)', backend.gameLanguageOptions[0]['label'])
        self.assertEqual(['English', 'Português (Brasil)', 'Español (Latinoamérica)', 'Français (Canada)',
                          'Türkçe', 'Русский'], [o['label'] for o in backend.gameLanguageOptions[1:]])
        tk = (ROOT / 'launcher' / 'infamous_launcher.py').read_text(encoding='utf-8')
        for key, name in logic.GAME_LANGUAGE_NAMES.items():
            self.assertIn(f"'{key}': '{name}'", tk)

    def test_game_language_default_is_auto_and_shows_what_applies(self):
        self.assertEqual('auto', logic.tlg_run.DEFAULT_SETTINGS['game_language'])
        backend, _ = self.make(game_language='auto')
        self.assertEqual(0, backend.gameLanguageIndex)
        with mock.patch.dict('os.environ', {'LC_ALL': 'fr_FR.UTF-8'}):
            self.assertEqual('Automatic: Français (Canada)', backend.gameLanguageNow)
        backend, _ = self.make(game_language='ru')
        self.assertEqual('', backend.gameLanguageNow)

    def test_game_language_saves_the_key_and_auto(self):
        backend, _ = self.make()
        with mock.patch.object(logic.tlg_run, 'save_settings') as save:
            backend.setGameLanguage('ru')
            self.assertEqual('ru', save.call_args[0][0]['game_language'])
            self.assertEqual(6, backend.gameLanguageIndex)
            backend.setGameLanguage('xx')                        # unknown keys are ignored
            self.assertEqual(1, save.call_count)
            backend.setGameLanguage('auto')
            self.assertEqual('auto', save.call_args[0][0]['game_language'])
            self.assertEqual(0, backend.gameLanguageIndex)

    def test_build_env_passes_the_chosen_game_language(self):
        for key, number in (('en', '1'), ('pt_BR', '17'), ('es_LA', '20'), ('fr_CA', '22'), ('tr', '19'), ('ru', '8')):
            backend, _ = self.make()
            with mock.patch.object(logic.tlg_run, 'save_settings'), mock.patch.dict('os.environ'):
                os.environ.pop('ISS_LANGUAGE', None)
                backend.setGameLanguage(key)
                self.assertEqual(number, logic.tlg_run.build_env(backend.settings)['ISS_LANGUAGE'], key)

    def test_controller_guides_are_english_and_tolerate_missing_files(self):
        self.assertEqual('8bitdo', logic.guide_kind('8BitDo Pro 2'))
        self.assertEqual('switch', logic.guide_kind('Switch Pro'))
        self.assertEqual('xbox', logic.guide_kind(None))
        with tempfile.TemporaryDirectory() as folder:
            backend, _ = self.make()
            backend.probes['guide_dir'] = Path(folder)
            self.assertEqual('', backend.guideSource)                      # no images: no crash
            (Path(folder) / 'inFAMOUS-controle-xbox-pt-BR.png').write_bytes(b'png')
            backend.setGuideKind('xbox')
            self.assertEqual('', backend.guideSource)                      # only pt-BR exists in the temp folder
            (Path(folder) / 'inFAMOUS-controle-xbox-en-US.png').write_bytes(b'png')
            self.assertTrue(backend.guideSource.endswith('xbox-en-US.png'))
            out = Path(folder) / 'copy'
            backend.saveGuide(str(out))
            self.assertEqual(b'png', (Path(folder) / 'copy.png').read_bytes())
        shipped = [logic.guide_path(k, pt) for k, _ in logic.GUIDE_KINDS for pt in (True, False)]
        self.assertTrue(all(shipped))

    def test_fix_rows_have_groups_headers_and_controls(self):
        backend, _ = self.make()
        rows = backend.fixes.rows
        headers = [r['title'] for r in rows if r['kind'] == 'header']
        self.assertEqual(['Lights and effects', 'Performance', 'Textures', 'Experimental'], headers)
        bp06 = next(r for r in rows if r.get('id') == 'BP06')
        self.assertEqual('switch', bp06['control'])
        self.assertTrue(bp06['on'])
        auto = next(r for r in rows if r.get('id') == 'TLG_BLOCK1D')
        self.assertEqual('combo', auto['control'])



@unittest.skipUnless(HAVE_QT, 'PySide6 not installed')
class NoUpdateCheckTests(unittest.TestCase):
    """Spec: no update check runs, at start or ever; the language is English and cannot be switched."""

    def make(self, **settings):
        self.fetched = []
        probes = {'gpu': lambda: ('GPU', '1.3'), 'pad': lambda: None, 'problems': lambda: [], 'cache': lambda: 0,
                  'save': lambda: None, 'version': lambda: 'v1', 'save_fixes': lambda s, st: None,
                  'saves': {'running': lambda: False, 'scan': lambda: []},
                  'support': {'info': lambda: {}, 'logs_dir': lambda: Path(tempfile.mkdtemp())},
                  'updates': {'macos': lambda: False, 'version': lambda: 'v1', 'save': lambda s: None,
                              'fetch': lambda tag: self.fetched.append(tag)}}
        base = dict(logic.tlg_run.DEFAULT_SETTINGS, game_dir='/g', **settings)
        return Backend(settings=base, probes=probes)

    def test_no_update_check(self):
        from PySide6.QtCore import QCoreApplication
        app = QCoreApplication.instance() or QCoreApplication([])
        backend = self.make(check_updates_on_open=True)       # even with the old option stored as on
        with mock.patch.object(backend.saves, 'start_timer'):
            backend.start_probes()
            for _ in range(20):
                app.processEvents()
                time.sleep(0.01)
        self.assertEqual([], self.fetched)

    def test_set_lang_is_not_visible_to_qml_and_the_language_is_english(self):
        backend = self.make(ui_language='pt_BR')
        meta = backend.metaObject()
        names = {str(meta.method(i).name(), 'utf-8') for i in range(meta.methodCount())}
        self.assertNotIn('setLang', names)
        self.assertNotIn('lang', {meta.property(i).name() for i in range(meta.propertyCount())})
        self.assertEqual('Play', backend.t_('nav_play'))


class RemovedScreensTests(unittest.TestCase):
    """The redesign removed the old screens, the bundled fonts and the language switch; About credits the sources."""
    LAUNCHER = QT / 'qml' / 'Launcher'

    def test_old_screens_and_fonts_are_gone(self):
        for name in ('HomePage', 'GraphicsPage', 'ControlsPage', 'FixesPage', 'SavesPage', 'GuidesPage', 'SupportPage',
                     'UpdatesPage', 'ModsPage', 'AdvancedPage', 'Rail', 'GhostButton', 'Toggle', 'Select', 'ConfirmCard'):
            self.assertFalse((self.LAUNCHER / f'{name}.qml').exists(), name)
        self.assertFalse((QT / 'fonts').exists())
        self.assertFalse((QT / 'screenshots.py').exists())

    def test_qml_never_reaches_the_language_or_updates(self):
        text = ' '.join(p.read_text(encoding='utf-8') for p in self.LAUNCHER.glob('*.qml'))
        for word in ('setLang', 'Backend.lang', 'Updates.', 'DLSS', 'dlss'):
            self.assertNotIn(word, text, word)

    def test_every_qmldir_entry_has_its_file_and_every_file_an_entry(self):
        entries = {line.split()[-1] for line in (self.LAUNCHER / 'qmldir').read_text().splitlines()[1:] if line.strip()}
        files = {p.name for p in self.LAUNCHER.glob('*.qml')}
        self.assertEqual(files, entries)

    def test_about_credits_and_their_links(self):
        text = (self.LAUNCHER / 'AboutPane.qml').read_text(encoding='utf-8')
        for credit in ('Based on iss_pc by HayatoG', 'GoldHEN cheat repository', 'satperm', 'Lucide',
                       'github.com/executed007/iss_macos', 'Pull the latest version and rebuild.'):
            self.assertIn(credit, text)
        urls = {'fork': 'https://github.com/executed007/iss_macos', 'coffee': 'https://buymeacoffee.com/hayatog',
                'goldhen': 'https://github.com/GoldHEN/GoldHEN_Cheat_Repository',
                'moltenvk': 'https://github.com/satperm/iss_mac_port'}
        backend_text = (QT / 'backend.py').read_text(encoding='utf-8')
        for name, url in urls.items():
            self.assertIn(f"'{name}': '{url}'", backend_text)
            self.assertIn(f'Backend.openUrl("{name}")', text)
        self.assertTrue((self.LAUNCHER / 'icons' / 'LICENSE').is_file())

    def test_advanced_row_ids_exist_in_their_backends(self):
        text = (self.LAUNCHER / 'AdvancedPane.qml').read_text(encoding='utf-8')
        ids = re.findall(r'source:\s*(Graphics|PadControls);\s*rowId:\s*"([^"]+)"', text)
        self.assertGreaterEqual(len(ids), 30)
        for source in ('Graphics', 'PadControls'):
            self.assertTrue(any(s == source for s, _ in ids))


# Opens the window off screen as --selftest does, in its own process (one QGuiApplication per
# process), and prints what the shell tests check: the sidebar keys and the theme in both appearances.
SHELL_FACTS = r'''
import json, os, sys, tempfile
sandbox = tempfile.mkdtemp(prefix='iss-shell-test-')
os.environ.update(QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software', XDG_CONFIG_HOME=sandbox,
                  ISS_USER_DIR=os.path.join(sandbox, 'user'), ISS_QT_REDUCED_MOTION='1')
sys.path.insert(0, sys.argv[1])
from PySide6.QtCore import QObject, QUrl
from PySide6.QtQml import QQmlComponent
import infamous_run as tlg_run
from qt.app import App
from qt.backend import Backend
probes = {'gpu': lambda: ('test', '0'), 'pad': lambda: None, 'problems': lambda: [], 'cache': lambda: 0,
          'save': lambda: None, 'version': lambda: 'test', 'save_fixes': lambda s, st: None,
          'saves': {'running': lambda: False, 'scan': lambda: []}}
settings = dict(tlg_run.DEFAULT_SETTINGS, ui_language='en_US', game_dir=os.path.join(sandbox, 'game'))
app = App(backend=Backend(settings=settings, probes=probes))
sidebar = app.window.findChild(QObject, 'sidebar')
model = sidebar.property('model') if sidebar else None
model = model.toVariant() if hasattr(model, 'toVariant') else model
theme = QQmlComponent(app.engine)
theme.setData(b"""
import QtQuick
import Launcher
QtObject {
    property string out
    function snap() {
        var o = {}
        for (const k of ["windowBg", "sidebarBg", "groupBg", "separator", "selection", "textPrimary", "textSecondary",
                         "accent", "body", "caption", "headline", "title", "s1", "s2", "s3", "radius"])
            o[k] = typeof Theme[k] === "number" ? Theme[k] : String(Theme[k])
        return o
    }
    Component.onCompleted: {
        var light = {}, dark = {}, onAccent = {}, onAccentLarge = {}
        try { Theme.colorScheme = Qt.ColorScheme.Light; light = snap(); Theme.colorScheme = Qt.ColorScheme.Dark; dark = snap() }
        catch (e) { console.log(e) }
        try { for (const fill of ["#ffc600", "#62ba46", "#8c8c8c", "#0a60ff", "#953d96"]) onAccent[fill] = String(Theme.textOn(fill)) }
        catch (e) { console.log(e) }
        try { for (const fill of ["#007aff", "#0a84ff", "#ffd60a", "#ffc600", "#8c8c8c"]) onAccentLarge[fill] = String(Theme.textOnLarge(fill)) }
        catch (e) { console.log(e) }
        out = JSON.stringify({ light: light, dark: dark, onAccent: onAccent, onAccentLarge: onAccentLarge })
    }
}""", QUrl())
probe = theme.create()
print(json.dumps({'sidebar': [entry['key'] for entry in model] if model else None,
                  'theme': json.loads(probe.property('out')) if probe else None}))
'''


@unittest.skipUnless(HAVE_QT, 'PySide6 not installed')
class ShellTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        run = subprocess.run([sys.executable, '-c', SHELL_FACTS, str(ROOT / 'launcher')],
                             capture_output=True, text=True, timeout=120)
        lines = run.stdout.strip().splitlines()
        cls.facts = json.loads(lines[-1]) if run.returncode == 0 and lines else {}
        cls.log = run.stderr[-3000:]

    def test_sidebar_order(self):
        self.assertEqual(['play', 'graphics', 'controls', 'cheats', 'saves', 'advanced', 'about'],
                         self.facts.get('sidebar'), self.log)

    def test_theme_has_light_and_dark(self):
        theme = self.facts.get('theme') or {}
        light, dark = theme.get('light', {}), theme.get('dark', {})
        for token in ('windowBg', 'sidebarBg', 'groupBg', 'separator', 'selection', 'textPrimary', 'textSecondary'):
            self.assertIn(token, light, self.log)
            self.assertNotEqual(light[token], dark[token], token)
        for token, size in (('body', 13), ('caption', 11), ('headline', 13), ('title', 20), ('s1', 8), ('s2', 16),
                            ('s3', 24), ('radius', 10)):
            self.assertEqual((size, size), (light.get(token), dark.get(token)), token)

    def test_text_on_accent_follows_its_luminance(self):
        # yellow, green and graphite take black text; blue and purple white (4.5:1 or more either way)
        self.assertEqual({'#ffc600': '#000000', '#62ba46': '#000000', '#8c8c8c': '#000000', '#0a60ff': '#ffffff',
                          '#953d96': '#ffffff'}, (self.facts.get('theme') or {}).get('onAccent'), self.log)

    def test_large_text_on_accent_prefers_white_at_3_to_1(self):
        # 14 pt semibold is WCAG large text (3:1): white wherever it reaches that (the two Apple blues, graphite),
        # dark on yellow
        self.assertEqual({'#007aff': '#ffffff', '#0a84ff': '#ffffff', '#ffd60a': '#000000', '#ffc600': '#000000',
                          '#8c8c8c': '#ffffff'},
                         (self.facts.get('theme') or {}).get('onAccentLarge'), self.log)


# The Play pane in its own process: Return presses Play only while the pane is shown (the slot is mocked: no
# game starts), and a row's label names its pop-up for VoiceOver.
PLAY_FACTS = r'''
import json, os, sys, tempfile
sandbox = tempfile.mkdtemp(prefix='iss-play-test-')
os.environ.update(QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software', XDG_CONFIG_HOME=sandbox,
                  ISS_USER_DIR=os.path.join(sandbox, 'user'), ISS_QT_REDUCED_MOTION='1')
sys.path.insert(0, sys.argv[1])
from PySide6.QtCore import QObject, Qt, Slot
from PySide6.QtQml import QQmlEngine, QQmlExpression
from PySide6.QtTest import QTest
import infamous_run as tlg_run
from qt.app import App
from qt.backend import Backend


class Recorder(Backend):
    plays = 0

    @Slot()
    def play(self):
        Recorder.plays += 1


probes = {'gpu': lambda: ('test', '0'), 'pad': lambda: None, 'problems': lambda: [], 'cache': lambda: 0,
          'save': lambda: None, 'version': lambda: 'test', 'save_fixes': lambda s, st: None,
          'saves': {'running': lambda: False, 'scan': lambda: []}}
settings = dict(tlg_run.DEFAULT_SETTINGS, ui_language='en_US', game_dir=os.path.join(sandbox, 'Extracted Game #1'))
app = App(backend=Recorder(settings=settings, probes=probes))
window = app.window
window.requestActivate()
QTest.qWaitForWindowActive(window)
facts = {}
QTest.keyClick(window, Qt.Key_Return)
facts['plays'] = Recorder.plays
app.go(1)
QTest.keyClick(window, Qt.Key_Return)
facts['plays_on_graphics'] = Recorder.plays - facts['plays']
def find(item, test):            # panes come from a Repeater, outside the QObject tree findChild walks
    if test(item):
        return item
    return next((found for found in (find(child, test) for child in item.childItems()) if found), None)


combo = find(window.contentItem(), lambda item: item.objectName() == 'language')
if combo is not None:
    value = QQmlExpression(QQmlEngine.contextForObject(combo), combo, 'Accessible.name').evaluate()
    facts['language_name'] = value[0] if isinstance(value, tuple) else value
pane = find(window.contentItem(), lambda item: item.metaObject().className().startswith('PlayPane'))
dialog = pane.findChild(QObject, 'folderDialog') if pane else None
if dialog is not None:
    facts['dialog_folder'] = dialog.property('currentFolder').toLocalFile()
facts['game_dir'] = settings['game_dir']
print(json.dumps(facts))
'''


@unittest.skipUnless(HAVE_QT, 'PySide6 not installed')
class PlayPaneTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        run = subprocess.run([sys.executable, '-c', PLAY_FACTS, str(ROOT / 'launcher')],
                             capture_output=True, text=True, timeout=120)
        lines = run.stdout.strip().splitlines()
        cls.facts = json.loads(lines[-1]) if run.returncode == 0 and lines else {}
        cls.log = run.stderr[-3000:]

    def test_return_presses_play_only_on_the_play_pane(self):
        self.assertEqual(1, self.facts.get('plays'), self.log)
        self.assertEqual(0, self.facts.get('plays_on_graphics'), self.log)

    def test_row_label_names_its_control(self):
        self.assertEqual('Language', self.facts.get('language_name'), self.log)

    def test_folder_dialog_opens_at_the_game_folder(self):
        # a space and a '#': only an encoded file URL keeps the whole path ('#1' would become a fragment)
        self.assertEqual(self.facts.get('game_dir'), self.facts.get('dialog_folder'), self.log)


if __name__ == '__main__':
    unittest.main()

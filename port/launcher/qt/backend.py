# SPDX-License-Identifier: GPL-2.0-or-later
"""The QObject the QML screens talk to. All real work is delegated to the modules the Tk
launcher uses (infamous_run: prepare/launch/build_env/save_experimental; backports_table)."""
import os
import sys
import subprocess
import threading
from pathlib import Path

from PySide6.QtCore import (Property, QAbstractListModel, QCoreApplication, QModelIndex, QObject, Qt, QTimer, QUrl,
                            Signal, Slot)

from . import logic
from .saves import Saves
from .support import Support
from .updates import Updates
from .logic import backports_table, tlg_run
import advanced_table  # noqa: E402
import logtail  # noqa: E402
import precompile  # noqa: E402

LOG_LINES = 500           # lines the log panel keeps
LOG_POLL_MS = 1000        # how often the game's log file is read for the panel
CLOSE_DELAY_MS = 2000     # 'Close the launcher when the game starts': wait after the start

URLS = {'channel': 'https://www.youtube.com/@hayatog-dev', 'discord': 'https://discord.gg/UMkdMYZh9g',
        'sponsor': 'https://github.com/sponsors/HayatoG',
        'coffee': 'https://buymeacoffee.com/hayatog',
        'fork': 'https://github.com/executed007/iss_macos',
        'upstream': 'https://github.com/HayatoG/iss_pc',
        'goldhen': 'https://github.com/GoldHEN/GoldHEN_Cheat_Repository',
        'moltenvk': 'https://github.com/satperm/iss_mac_port',
        'lucide': 'https://github.com/lucide-icons/lucide/blob/main/LICENSE',
        'apolar': 'https://github.com/sapoguruoficial-cmd'}


class FixesModel(QAbstractListModel):
    """Rows of the Fixes screen: group headers and one row per BPnn / A-B switch."""
    DATA = Qt.UserRole + 1

    def __init__(self, backend, scope='fixes'):
        super().__init__(backend)
        self.backend = backend
        self.scope = scope          # 'fixes' (BPnn + A/B switches) or 'adv' (the Advanced screen)
        self.filter = ''
        self.rows = []
        self.rebuild()

    def roleNames(self):
        return {self.DATA: b'd'}

    def rowCount(self, parent=QModelIndex()):
        return 0 if parent.isValid() else len(self.rows)

    def data(self, index, role=DATA):
        if role == self.DATA and 0 <= index.row() < len(self.rows):
            return self.rows[index.row()]
        return None

    def rebuild(self):
        t = self.backend.t_
        states = self.backend.fix_states
        rows = []
        if self.scope == 'adv':
            self._rebuild_advanced(t, states)
            return
        titles = {'light': 'fixes_group_light', 'perf': 'fixes_group_perf', 'tex': 'fixes_group_tex',
                  'exp': 'fixes_group_exp'}
        for group in logic.GROUP_ORDER:
            items = [i for i in logic.fix_items() if logic.group_of(i) == group]
            if not items:
                continue
            rows.append({'kind': 'header', 'id': 'hdr-' + group, 'title': t(titles[group]),
                         'note': t('fixes_group_exp_note') if group == 'exp' else ''})
            rows += [self.item_row(i, states) for i in items]
        self.beginResetModel()
        self.rows = rows
        self.endResetModel()

    def _rebuild_advanced(self, t, states):
        words = self.filter.lower().split()
        rows = []
        for area, title_en, title_pt in advanced_table.AREAS:
            items = [i for i in backports_table.ADVANCED if i.tab == area and self._matches(i, words, t)]
            if not items:
                continue
            rows.append({'kind': 'header', 'id': 'hdr-' + area, 'title': title_pt if t.pt() else title_en, 'note': ''})
            rows += [self.item_row(i, states) for i in items]
        self.beginResetModel()
        self.rows = rows
        self.endResetModel()

    @staticmethod
    def _matches(item, words, t):
        text = (item.id + ' ' + t.pair(item.name) + ' ' + t.pair(item.what)).lower()
        return all(w in text for w in words)

    def set_filter(self, text):
        self.filter = text
        self.rebuild()

    def item_row(self, item, states):
        t = self.backend.t_
        value = backports_table.effective(item, states)
        name = t.pair(item.name)
        what = t.pair(item.what)
        if item.source:
            what += ' (' + t('exp_source', source=item.source) + ')'
        tags = [{'text': t('exp_live') if item.live else t('exp_restart'), 'tone': 'info'}]
        if item.group == 'adv':
            tags.append({'text': item.env, 'tone': 'info'})
        if backports_table.is_recommended(item):
            tags.append({'text': t('exp_recommended'), 'tone': 'good'})
        if item.risky:
            tags.append({'text': t('exp_experimental'), 'tone': 'warn'})
        if item.broken:
            tags.append({'text': t('exp_broken'), 'tone': 'bad'})
        row = {'kind': 'item', 'id': item.id, 'code': item.id if item.group == 'bp' else '', 'label': name,
               'what': what, 'tags': tags, 'shaderWarn': t('fixes_shader_warn') if item.shader else '',
               'changed': states.get(item.id) is not None, 'control': 'switch', 'on': value == '1',
               'value': value if value is not None else 'auto', 'options': [], 'index': 0,
               'lo': 0, 'hi': 0, 'unit': ''}
        if item.vtype == 'int':
            row['control'] = 'number'
            row['value'] = '' if states.get(item.id) is None else states[item.id]
            row['lo'], row['hi'], row['unit'] = item.lo, item.hi, item.unit
            row['placeholder'] = t('adv_auto') if item.default is None else item.default
        elif not (item.vtype == 'bool' and item.default is not None):
            if item.vtype == 'bool':  # automatic: the GPU code decides
                options = [('auto', t('exp_auto')), ('1', t('exp_on')), ('0', t('exp_off'))]
            else:
                options = [(v, (pt if t.pt() else en)) for v, en, pt in item.choices]
            row['control'] = 'combo'
            row['options'] = [{'value': v, 'label': label} for v, label in options]
            row['index'] = next((n for n, (v, _) in enumerate(options) if v == row['value']), 0)
        return row

    def refresh_item(self, item_id):
        for n, row in enumerate(self.rows):
            if row['id'] == item_id:
                item = next(i for i in backports_table.ITEMS if i.id == item_id)
                self.rows[n] = self.item_row(item, self.backend.fix_states)
                self.dataChanged.emit(self.index(n), self.index(n))
                return

    def refresh_all(self):
        self.rebuild()


class Backend(QObject):
    chipsChanged = Signal()
    stateChanged = Signal()
    gameDirChanged = Signal()
    logChanged = Signal()
    profileChanged = Signal()
    gameLanguageChanged = Signal()
    gpuFound = Signal(object)
    padFound = Signal(object)              # controller name or None (from the probing thread)
    lineReceived = Signal(str)
    jobsProgress = Signal(str, int, int)   # shader list: phase, done, total (from the play thread)
    cacheMeasured = Signal(object)         # bytes of the shader cache (from the measuring thread)
    finished = Signal(int)
    gameStarted = Signal(str)   # the log path; from the play thread right after the game was launched
    message = Signal(str)
    guideChanged = Signal()
    scrollRequested = Signal(int)   # used by the screenshot tool
    modsChanged = Signal()
    modsLoaded = Signal(int, object, object)   # generation, key, rows (from the worker that fills the first listing)

    def __init__(self, settings=None, translator=None, probes=None):
        """probes: optional dict of callables replacing the slow/hardware ones (tests, screenshots)."""
        super().__init__()
        self.gameDirChanged.connect(self.gameLanguageChanged)   # the languages offered follow the game files
        self.settings = tlg_run.load_settings() if settings is None else settings
        self.t_ = translator or logic.Translator({'ui_language': 'en_US'})   # English only; the stored setting is left as it is
        self.probes = probes or {}
        self.fonts = ('sans-serif', 'sans-serif')  # app.py puts the loaded families here
        self.fix_states = tlg_run.experimental_states(self.settings)
        if tlg_run.fold_temporal_mode(self.fix_states):   # macOS: a temporal mode becomes the internal resolution
            self._save_fixes()
        self._gpu = 'checking'
        self._pad = None
        self._pad_scanning = False
        self._chips, self._ready = [], False
        self._play_state = 'idle'
        self._log = []
        self._news = []
        self._save = None
        self._message = ''
        self._guide_kind, self._guide_message = None, ''
        self.process = None
        self._cache_bytes = None   # None until the first measure ends; QML properties never walk the folder
        self._measuring = self._measure_again = False
        self._mods, self._mods_key = None, None   # list_mods, kept (it reads the whole eboot.bin): _mod_rows()
        self._mods_loading, self._mods_filled, self._mods_generation = False, False, 0
        self.modsLoaded.connect(self._on_mods_loaded)
        self.cacheMeasured.connect(self._on_cache_measured)
        self.fixes = FixesModel(self)
        self.advanced = FixesModel(self, 'adv')
        self.gpuFound.connect(self._on_gpu)
        self.padFound.connect(self._on_pad)
        self.lineReceived.connect(self._on_line)
        self.jobsProgress.connect(self._on_jobs_progress)
        self.precompiler = precompile.Precompiler()
        self._jobs = None        # (phase, done, total) while the shader list is being made
        self.finished.connect(self._on_finished)
        self.gameStarted.connect(self._on_game_started)
        self._tail = None        # logtail.LogTail of the running game's log
        self._tail_timer = QTimer(self)
        self._tail_timer.setInterval(LOG_POLL_MS)
        self._tail_timer.timeout.connect(self._poll_log)
        self.refresh()
        self.saves = Saves(self, self.probes.get('saves'))
        self.support = Support(self, self.probes.get('support'))
        self.updates = Updates(self, self.probes.get('updates'))

    # -- fonts and motion (set by app.py) ------------------------------------------------------
    @Property(str, constant=True)
    def fontDisplay(self):
        return self.fonts[0]

    @Property(str, constant=True)
    def fontText(self):
        return self.fonts[1]

    @Property(bool, constant=True)
    def reducedMotion(self):
        return bool(os.environ.get('ISS_QT_REDUCED_MOTION'))

    @Property(bool, constant=True)
    def logShown(self):
        return False

    # -- helpers -------------------------------------------------------------------------------
    def probe(self, name, default):
        return self.probes[name]() if name in self.probes else default()

    def start_probes(self):
        """GPU/Vulkan detection and the search for saves run off the UI thread."""
        self._mod_rows()   # the cheat list, read off the UI thread
        self.saves.scan()
        self.saves.start_timer()
        self.support.collect()
        def work():
            self.gpuFound.emit(self.probe('gpu', logic.detect_gpu))
        threading.Thread(target=work, daemon=True).start()
        self.rescan_pad()

    def rescan_pad(self):
        """Looks for a controller off the UI thread: `tlg-probe --gamepad` is a process that waits at
        least 300 ms when none is connected, and every click used to wait for it."""
        if 'pad' in self.probes or self._pad_scanning:
            return
        self._pad_scanning = True

        def work():
            try:
                self.padFound.emit(logic.detect_pad())
            finally:
                self._pad_scanning = False
        threading.Thread(target=work, daemon=True).start()

    def _on_pad(self, pad):
        if pad != self._pad:
            self._pad = pad
            self._compute_chips()
            self.chipsChanged.emit()
            self.stateChanged.emit()

    def _on_gpu(self, gpu):
        self._gpu = gpu
        self.refresh()

    def refresh(self):
        if 'pad' in self.probes:   # a test or the selftest injects the answer: no thread
            self._pad = self.probes['pad']()
        self.measure_cache()
        self._compute_chips()
        self._news = self.probe('news', lambda: logic.news(self.t_, self.t_.pt()))
        savedata = tlg_run.user_dir() / 'savedata'
        self._save = self.probe('save', lambda: logic.last_save_title(savedata))
        self.chipsChanged.emit()
        self.stateChanged.emit()
        self.profileChanged.emit()
        self.gameDirChanged.emit()

    def _compute_chips(self):
        problems = self.probes['problems']() if 'problems' in self.probes else \
            tlg_run.check_game(self.settings.get('game_dir') or '')
        self._game_state, self._game_problems, self._port_built = logic.game_state(self.settings, problems)
        self._chips, self._ready = logic.compute_chips(
            self.settings, self.t_, gpu=self._gpu, pad=self._pad, problems=problems,
            states=self.fix_states, cache_bytes=self._cache_bytes, measure_cache=False)

    # -- shader cache size ---------------------------------------------------------------------
    def measure_cache(self):
        """Sizes the shader cache off the UI thread. After the first start it holds tens of thousands of
        files: walking them on the UI thread at every state change (the Home chip, showFirstRun, each
        progress line of the shader list) froze the window on Windows, close-on-play timer included."""
        if 'cache' in self.probes:
            self._cache_bytes = self.probes['cache']()
            return
        if self._measuring:
            self._measure_again = True
            return
        self._measuring = True
        settings = dict(self.settings)
        def work():
            try:
                size = tlg_run.shader_cache_size(settings)
            except Exception:
                size = 0
            self.cacheMeasured.emit(size)
        threading.Thread(target=work, daemon=True).start()

    def _on_cache_measured(self, size):
        self._measuring = False
        if self._measure_again:   # the cache changed while it was measured (cleared, folder changed)
            self._measure_again = False
            self.measure_cache()
        if size == self._cache_bytes:
            return
        self._cache_bytes = size
        self._compute_chips()
        self.chipsChanged.emit()
        self.stateChanged.emit()

    def cache_bytes(self):
        """The last measured size; 0 while the first measure runs."""
        return self._cache_bytes or 0

    def reload_user(self):
        """The data folder changed: tlg.ini (fix switches) and the last save come from the new one."""
        self.fix_states = tlg_run.experimental_states(self.settings)
        if tlg_run.fold_temporal_mode(self.fix_states):
            self._save_fixes()
        self.fixes.refresh_all()
        self.profileChanged.emit()
        self.refresh()
        self.modsChanged.emit()   # its own mods folder and mods.json
        if hasattr(self, 'saves'):
            self.saves.reload()

    # -- text -----------------------------------------------------------------------------------
    @Slot(str, result=str)
    def t(self, key):
        return self.t_(key)

    @Slot(str, 'QVariantMap', result=str)
    def tf(self, key, args):
        return self.t_(key, **args)

    # -- home state -----------------------------------------------------------------------------
    @Property('QVariantList', notify=chipsChanged)
    def chips(self):
        return self._chips

    @Property(bool, notify=stateChanged)
    def ready(self):
        return self._ready

    @Property(str, notify=stateChanged)
    def statusTitle(self):
        return self.t_('status_ready' if self._ready else 'status_attention')

    @Property('QVariantList', notify=stateChanged)
    def newsLines(self):
        return self._news

    @Property(str, notify=stateChanged)
    def version(self):
        return self.probe('version', logic.release_version)

    @Property(str, constant=True)
    def buildVersion(self):
        """About's version: the tag and commit this checkout is at ('sunny-beta-2026.11.07-132-ga2e8040'), else the release tag."""
        if not hasattr(self, '_build_version'):
            def describe():
                try:
                    return subprocess.run(['git', '-C', str(tlg_run.REPO_DIR), 'describe', '--tags', '--always'],
                                          capture_output=True, text=True, timeout=3).stdout.strip()
                except (OSError, subprocess.SubprocessError):
                    return ''
            self._build_version = self.probe('build_version', describe) or self.probe('version', logic.release_version)
        return self._build_version

    @Property(str, notify=stateChanged)
    def heroKicker(self):
        serial = (logic.shader_cache.title_id(self.settings.get('game_dir', '')) or 'CUSA00223')
        return self.t_('hero_kicker', serial=serial, version='1.07')

    @Property(str, notify=stateChanged)
    def playSubtitle(self):
        if self._jobs:
            return self.jobsText
        if self._play_state == 'launching':
            return self.t_('play_launching')
        if self._play_state == 'running':
            return self.t_('play_running')
        if not self.canPlay:
            return self.t_('play_blocked')
        return self.t_('play_continue', save=self._save) if self._save else self.t_('play_new')

    @Property(str, notify=stateChanged)
    def jobsText(self):
        """'Indexing the game files… 42%' while the generator runs, else ''."""
        if not self._jobs:
            return ''
        phase, done, total = self._jobs
        text = precompile.message(precompile.PHASE_TEXT.get(phase, 'jobs_phase_other'), self.settings, phase=phase)
        return f'{text} {100 * done // total}%' if total else text

    @Property(float, notify=stateChanged)
    def jobsProgressValue(self):
        """0..1 of the generator's current phase, -1 when it is not running."""
        if not self._jobs:
            return -1.0
        _, done, total = self._jobs
        return min(1.0, done / total) if total else 0.0

    def _on_jobs_progress(self, phase, done, total):
        self._jobs = (phase, done, total)
        self.stateChanged.emit()

    @Property(bool, notify=stateChanged)
    def canPlay(self):
        return self._ready and self._play_state == 'idle'

    @Property(str, notify=stateChanged)
    def playState(self):
        return self._play_state

    @Property(bool, notify=stateChanged)
    def showFirstRun(self):
        empty = self._cache_bytes == 0   # unknown (still measuring) is not empty
        return empty or backports_table.notice_pending(self.settings)

    @Property(str, notify=gameDirChanged)
    def gameDir(self):
        return self.settings.get('game_dir', '')

    # -- the Play pane's status rows ---------------------------------------------------------------
    @Property(bool, notify=stateChanged)
    def folderValid(self):
        """The game folder holds the supported game (Play also needs the port built)."""
        return self._game_state == 'ok'

    @Property(bool, notify=stateChanged)
    def portBuilt(self):
        return self._port_built

    @Property(str, notify=stateChanged)
    def gpuSummary(self):
        """'Apple M5 · Metal': the port's Vulkan runs on Metal (MoltenVK)."""
        if self._gpu == 'checking':
            return self.t_('chip_gpu_checking')
        return f'{self._gpu[0]} · Metal' if self._gpu else self.t_('chip_gpu_unknown')

    @Property(str, notify=stateChanged)
    def gameSummary(self):
        """'CUSA00004 · v1.07' (check_game accepts only the 1.07 eboot), else what is wrong with the folder."""
        if self._game_state == 'none':
            return self.t_('play_game_none')
        if self._game_state == 'bad':
            return logic.game_problem_text(self.t_, tlg_run.blocking(self._game_problems))
        return ' · '.join(filter(None, (logic.shader_cache.title_id(self.settings['game_dir']), 'v1.07')))

    @Property(str, notify=stateChanged)
    def cacheState(self):
        if self._cache_bytes is None:
            return self.t_('play_cache_measuring')
        return logic.shader_cache.human_size(self._cache_bytes) if self._cache_bytes else self.t_('play_cache_empty')

    @Property(str, notify=gameDirChanged)
    def keyArtUrl(self):
        """The game's own art in the player's game folder (never shipped): pic1.png, else pic0.png, else
        icon0.png; '' when there is none."""
        folder = self.settings.get('game_dir')
        for name in ('pic1.png', 'pic0.png', 'icon0.png') if folder else ():
            path = Path(folder) / 'sce_sys' / name
            if path.is_file():
                return QUrl.fromLocalFile(str(path)).toString()
        return ''

    @Property(str, notify=logChanged)
    def logText(self):
        return '\n'.join(self._log[-LOG_LINES:])

    @Property(str, notify=stateChanged)
    def notice(self):
        return self._message

    # -- game folder ----------------------------------------------------------------------------
    @Slot(str)
    def chooseFolder(self, url):
        path = QUrl(url).toLocalFile() if url.startswith('file:') else url
        if not path:
            return
        state, problems, _ = logic.game_state(self.settings, tlg_run.check_game(path) if 'problems' not in self.probes else [])
        blocking = tlg_run.blocking(problems)
        if blocking:
            self._message = self.t_('folder_rejected', why=logic.game_problem_text(self.t_, blocking))
        else:
            self.settings['game_dir'] = path
            tlg_run.save_settings(self.settings)
            self._message = self.t_('folder_saved')
        self.refresh()
        self.modsChanged.emit()   # the cheats offered follow the game's version

    @Slot()
    def clearNotice(self):
        self._message = ''
        self.stateChanged.emit()

    @Slot(str)
    def openUrl(self, name):
        tlg_run.open_external(URLS[name])

    # -- play (same path as the Tk launcher: prepare() then launch()) -------------------------------
    @Slot()
    def play(self):
        if self.updates.busy:
            return
        if self._play_state != 'idle' or not self._ready:
            return
        tlg_run.save_settings(self.settings)
        if backports_table.notice_pending(self.settings):
            backports_table.mark_notice_seen(self.settings)
            tlg_run.save_settings(self.settings)
        self._play_state = 'launching'
        self.precompiler = precompile.Precompiler()
        self._log = [self.t_('preparing')]
        self.logChanged.emit()
        self.stateChanged.emit()
        threading.Thread(target=self._run_game, daemon=True).start()

    def _run_game(self):
        try:
            tlg_run.prepare(self.settings, log=lambda line: self.lineReceived.emit(line))
            outcome = precompile.prepare_jobs(self.settings, log=lambda line: self.lineReceived.emit(line),
                                              progress=lambda *args: self.jobsProgress.emit(*args),
                                              precompiler=self.precompiler)
            self._jobs = None
            if outcome == 'cancelled':   # window closed or Quit in the middle: no half file, no game
                self.lineReceived.emit(self.t_('jobs_cancelled'))
                self.finished.emit(-1000)
                return
            self.process, log = tlg_run.launch(self.settings)
        except tlg_run.GameError as error:
            self.lineReceived.emit(self.t_(error.key, detail=error.detail))
            self.finished.emit(-1000)
            return
        except Exception as error:  # shown to the player; the launcher keeps running
            self._jobs = None
            self.lineReceived.emit(str(error))
            self.finished.emit(-1000)
            return
        self.lineReceived.emit(f"{self.t_('running')} {log}")
        self._play_state = 'running'
        self.stateChanged.emit()
        self.gameStarted.emit(str(log))
        code = self.process.wait()   # the game writes its own log file; here only the exit code
        self.process = None
        self.finished.emit(code)

    def _on_game_started(self, log):
        """Main thread: follow the log file once a second and, if asked, close the launcher."""
        self._tail = logtail.LogTail(log, LOG_LINES)
        self._tail_timer.start()
        if self.settings.get('close_on_play'):
            QTimer.singleShot(CLOSE_DELAY_MS, self._quit_app)

    def _quit_app(self):
        process = self.process
        if self.settings.get('reopen_launcher', True) and process is not None and process.poll() is None:
            tlg_run.reopen_after(process.pid)
        app = QCoreApplication.instance()
        if app is None:
            return
        for window in getattr(app, 'topLevelWindows', lambda: [])():   # gone at once, even if the exit is slow
            window.hide()
        app.quit()

    def _poll_log(self, final=False):
        """Reads the bytes the game appended; the panel is rebuilt once per tick, never per line."""
        if self._tail is None:
            return
        new = self._tail.poll(final=final)
        if new:
            self._log.extend(new)
            del self._log[:-LOG_LINES]
            self.logChanged.emit()

    def _on_line(self, line):
        self._log.append(line)
        del self._log[:-LOG_LINES]
        self.logChanged.emit()

    def _on_finished(self, code):
        self._play_state = 'idle'
        self._tail_timer.stop()
        self._poll_log(final=True)
        self._tail = None
        if code == -1000:
            pass
        else:
            self._log.append(self.t_('ended' if code in (0, -15) else 'crashed', code=code))
            self.logChanged.emit()
        self.refresh()

    def shutdown(self):
        # The game is an independent process: closing the launcher does not close it.
        self.precompiler.cancel()
        self._tail_timer.stop()

    # -- fixes ----------------------------------------------------------------------------------
    @Property(QObject, constant=True)
    def savesController(self):
        return self.saves

    @Property(QObject, constant=True)
    def fixesModel(self):
        return self.fixes

    @Property(QObject, constant=True)
    def advancedModel(self):
        return self.advanced

    @Property(str, notify=profileChanged)
    def advancedText(self):
        n = backports_table.advanced_changes(self.fix_states)
        return self.t_('adv_profile_default') if n == 0 else self.t_('adv_profile_custom', n=n)

    @Property(bool, notify=profileChanged)
    def advancedIsDefault(self):
        return backports_table.advanced_changes(self.fix_states) == 0

    @Slot(str)
    def setAdvancedFilter(self, text):
        self.advanced.set_filter(text)

    @Slot()
    def copyCombination(self):
        """Every ISS_ variable the player's choices add, one per line, for pasting into an issue."""
        from PySide6.QtGui import QGuiApplication
        env = backports_table.environment(self.fix_states, {})
        text = '\n'.join(f'{k}={v}' for k, v in sorted(env.items())) or '(defaults)'
        app = QGuiApplication.instance()
        if app is not None:
            app.clipboard().setText(text)
        self.message.emit(self.t_('adv_copied'))

    @Slot()
    def resetAdvanced(self):
        self.fix_states = backports_table.defaults(self.fix_states, 'adv')
        self._save_fixes()
        self.advanced.refresh_all()
        self.refresh()

    # -- mods/cheats -----------------------------------------------------------------------------
    def _mods_key_now(self):
        try:
            folder = tlg_run.mods_dir()
            return (self.settings.get('game_dir'), folder, folder.stat().st_mtime_ns)
        except OSError:   # no mods folder (read-only data folder): list_mods fails the same way
            return None

    def _list_mods(self):
        try:
            return tlg_run.list_mods(self.settings)
        except Exception:
            return []

    def _mod_rows(self):
        """list_mods, kept: it reads the whole eboot.bin. Read again when the game folder or the mods folder
        (path or mtime) changed, and after a toggle or Refresh. The first read runs on a worker thread
        (modsLoading, then modsChanged): the 133 MB eboot never blocks the first paint."""
        if self._mods_loading:
            return []
        key = self._mods_key_now()
        if self._mods is None or key != self._mods_key:
            if not self._mods_filled:
                self._mods_loading = True
                generation = self._mods_generation
                settings = dict(self.settings)

                def work():
                    try:
                        rows = tlg_run.list_mods(settings)
                    except Exception:
                        rows = []
                    self.modsLoaded.emit(generation, key, rows)
                threading.Thread(target=work, daemon=True).start()
                return []
            self._mods = self._list_mods()
            self._mods_key = key
        return self._mods

    def _on_mods_loaded(self, generation, key, rows):
        self._mods_loading = False
        self._mods_filled = True
        if generation == self._mods_generation:   # else a toggle or Refresh came first: read again, in place
            self._mods, self._mods_key = rows, key
        self.modsChanged.emit()

    @Property(bool, notify=modsChanged)
    def modsLoading(self):
        self._mod_rows()   # a first read from anywhere starts the worker
        return self._mods_loading

    @Property('QVariantList', notify=modsChanged)
    def modsModel(self):
        """Every shadPS4/GoldHEN cheat found in the mods folder for this game's version, with its current
        enabled state: the community cheats first, then the built-in options. Nothing here has any effect
        until turned on. Empty while the first listing is being read (modsLoading)."""
        rows = [{'key': key, 'name': name or key, 'note': note or '', 'on': bool(enabled)}
                for key, name, note, enabled in self._mod_rows()]
        return sorted(rows, key=lambda r: r['key'].startswith(('iss_builtin', 'native/')))

    @Property(int, notify=modsChanged)
    def cheatsOnCount(self):
        return sum(1 for *_, enabled in self._mod_rows() if enabled)

    @Slot(str, bool)
    def setModEnabled(self, key, enabled):
        tlg_run.set_mod_enabled(key, enabled)
        self._mods = None
        self._mods_generation += 1
        self.modsChanged.emit()

    @Slot()
    def refreshMods(self):
        self._mods = None
        self._mods_generation += 1
        self.modsChanged.emit()

    @Slot()
    def openModsFolder(self):
        tlg_run.open_external(tlg_run.mods_dir())

    @Property(str, notify=profileChanged)
    def profileText(self):
        n = backports_table.changes(self.fix_states)
        return self.t_('exp_profile_rec') if n == 0 else self.t_('exp_profile_custom', n=n)

    @Property(bool, notify=profileChanged)
    def profileIsRecommended(self):
        return backports_table.changes(self.fix_states) == 0

    @Property(bool, notify=profileChanged)
    def anyShaderChange(self):
        return any(self.fix_states.get(i.id) is not None and i.shader for i in logic.fix_items())

    def _occlusion_item(self):
        return next(i for i in backports_table.ITEMS if i.key == 'occlusion_mode')

    @Property('QVariantList', notify=profileChanged)
    def occlusionOptions(self):
        """The values of the "Light glare (occlusion)" selector on the Home screen."""
        item = self._occlusion_item()
        value = backports_table.effective(item, self.fix_states)
        return [{'value': v, 'label': self.t_.pair((en, pt)), 'on': v == value} for v, en, pt in item.choices]

    @Property(int, notify=profileChanged)
    def occlusionIndex(self):
        return next((n for n, o in enumerate(self.occlusionOptions) if o['on']), 0)

    @Slot(str)
    def setOcclusion(self, value):
        self.setFix(self._occlusion_item().id, value)

    def _verbose_log_item(self):
        return next(i for i in backports_table.ITEMS if i.key == 'verbose_log')

    @Property(bool, notify=profileChanged)
    def verboseLog(self):
        """The "Detailed log" switch on the Home screen, under the light glare selector."""
        return backports_table.effective(self._verbose_log_item(), self.fix_states) == '1'

    @Slot(bool)
    def setVerboseLog(self, on):
        self.setFix(self._verbose_log_item().id, '1' if on else '0')

    # -- upscaler (Home screen): one selector over fsr1 / iss_temporal / iss_temporal_backend ----------------
    _UPSCALER_KEYS = ('fsr1', 'iss_temporal', 'iss_temporal_backend', 'iss_temporal_mode')

    def _menu_item(self, key):
        return next(i for i in backports_table.ITEMS if i.key == key)

    def _menu_value(self, key):
        return backports_table.effective(self._menu_item(key), self.fix_states)

    def _upscaler_choice(self):
        """0 off, 1 FSR 1, 2 FSR 3.1 temporal, 3 NVIDIA DLSS temporal."""
        if self._menu_value('iss_temporal') == '1':
            return 3 if self._menu_value('iss_temporal_backend') == '1' else 2
        return 1 if self._menu_value('fsr1') == '1' else 0

    @Property('QVariantList', notify=profileChanged)
    def upscalerOptions(self):
        names = (('Off', 'Desligado'), ('FSR 1 (spatial)', 'FSR 1 (espacial)'),
                 ('FSR 3.1 (temporal)', 'FSR 3.1 (temporal)'), ('NVIDIA DLSS (temporal)', 'NVIDIA DLSS (temporal)'))
        return [{'value': str(n), 'label': self.t_.pair(pair)} for n, pair in enumerate(names)]

    @Property(int, notify=profileChanged)
    def upscalerIndex(self):
        return self._upscaler_choice()

    @Slot(str)
    def setUpscaler(self, value):
        """Writes the three switches behind the selector; a temporal one also needs an output resolution."""
        pick = int(value)
        self.fix_states[self._menu_item('fsr1').id] = backports_table.normalize(self._menu_item('fsr1'), '1' if pick == 1 else '0')
        self.fix_states[self._menu_item('iss_temporal').id] = backports_table.normalize(self._menu_item('iss_temporal'), '1' if pick >= 2 else '0')
        if pick >= 2:
            self.fix_states[self._menu_item('iss_temporal_backend').id] = backports_table.normalize(
                self._menu_item('iss_temporal_backend'), '1' if pick == 3 else '0')
            if self._menu_value('iss_temporal_mode') == '0':
                self.fix_states[self._menu_item('iss_temporal_mode').id] = backports_table.normalize(
                    self._menu_item('iss_temporal_mode'), '1')   # Quality, the usual choice
        self._save_fixes()
        for key in self._UPSCALER_KEYS:
            self.fixes.refresh_item(self._menu_item(key).id)
            self.advanced.refresh_item(self._menu_item(key).id)
        self.refresh()

    @Property(bool, notify=profileChanged)
    def upscalerHasQuality(self):
        return self._upscaler_choice() >= 2

    @Property('QVariantList', notify=profileChanged)
    def upscalerQualityOptions(self):
        item = self._menu_item('iss_temporal_mode')
        return [{'value': v, 'label': self.t_.pair((en, pt))} for v, en, pt in item.choices if v != '0']

    @Property(int, notify=profileChanged)
    def upscalerQualityIndex(self):
        value = self._menu_value('iss_temporal_mode')
        return max(0, int(value) - 1)

    @Slot(str)
    def setUpscalerQuality(self, value):
        self.setFix(self._menu_item('iss_temporal_mode').id, value)

    # -- game language (voice and text; the interface language above is separate) ---------------
    @Property('QVariantList', notify=gameLanguageChanged)
    def gameLanguageOptions(self):
        """'Automatic' first, then the languages of infamous_run.game_language_choices (those the game files have)."""
        return [{'value': k, 'label': self.t_('game_language_auto') if k == 'auto'
                 else logic.GAME_LANGUAGE_NAMES.get(k, k)} for k in tlg_run.game_language_choices(self.settings)]

    @Property(int, notify=gameLanguageChanged)
    def gameLanguageIndex(self):
        keys = tlg_run.game_language_choices(self.settings)
        return keys.index(self.settings.get('game_language')) if self.settings.get('game_language') in keys else 0

    @Property(str, notify=gameLanguageChanged)
    def gameLanguageNow(self):
        """'Automatic: Português (Brasil)' while 'auto' is active, else empty."""
        if self.settings.get('game_language', 'auto') in tlg_run.GAME_LANGUAGES:
            return ''
        return self.t_('game_language_now', name=logic.GAME_LANGUAGE_NAMES[tlg_run.resolve_game_language(self.settings)])

    @Slot(str)
    def setGameLanguage(self, key):
        if key != 'auto' and key not in tlg_run.GAME_LANGUAGES:
            return
        self.settings['game_language'] = key   # build_env reads it at the next start (ISS_LANGUAGE)
        tlg_run.save_settings(self.settings)
        self.gameLanguageChanged.emit()

    # -- controller guides ----------------------------------------------------------------------
    @Property('QVariantList', constant=True)
    def guideKinds(self):
        return [{'key': k, 'label': label} for k, label in logic.GUIDE_KINDS]

    @Property(str, notify=guideChanged)
    def guideKind(self):
        if self._guide_kind is None:   # preselect the detected controller; Xbox when unknown
            self._guide_kind = logic.guide_kind(self._pad)
        return self._guide_kind

    @Property(str, notify=guideChanged)
    def guideDetected(self):
        return self.t_('guides_detected', name=self._pad) if self._pad else ''

    def _guide_file(self):
        return logic.guide_path(self.guideKind, self.t_.pt(), self.probes.get('guide_dir'))

    @Property(str, notify=guideChanged)
    def guideSource(self):
        path = self._guide_file()
        return QUrl.fromLocalFile(str(path)).toString() if path else ''

    @Property(str, notify=guideChanged)
    def guideMessage(self):
        return self._guide_message

    @Slot(str)
    def setGuideKind(self, kind):
        self._guide_kind, self._guide_message = kind, ''
        self.guideChanged.emit()

    def _guide_say(self, text):
        self._guide_message = text
        self.guideChanged.emit()

    @Slot(str)
    def saveGuide(self, url):
        path, target = self._guide_file(), QUrl(url).toLocalFile() if url.startswith('file:') else url
        if not path or not target:
            return
        if not target.lower().endswith('.png'):
            target += '.png'
        try:
            import shutil
            shutil.copyfile(path, target)
        except OSError as error:
            self._guide_say(self.t_('guides_failed', detail=str(error)))
            return
        self._guide_say(self.t_('guides_saved', path=target))

    @Slot()
    def copyGuide(self):
        path = self._guide_file()
        if not path:
            return
        from PySide6.QtGui import QGuiApplication, QImage
        image = QImage(str(path))
        app = QGuiApplication.instance()
        if image.isNull() or app is None:
            self._guide_say(self.t_('guides_failed', detail=str(path)))
            return
        app.clipboard().setImage(image)
        self._guide_say(self.t_('guides_copied'))

    @Slot()
    def openGuide(self):
        """The guide image of the detected controller in the system viewer (Preview); the folder when it is absent."""
        tlg_run.open_external(self._guide_file() or self.probes.get('guide_dir') or logic.GUIDE_DIR)

    @Slot()
    def openGuidesFolder(self):
        tlg_run.open_external(self.probes.get('guide_dir') or logic.GUIDE_DIR)

    @Slot()
    def openReadme(self):
        # macOS: this fork's README.md; elsewhere the port's own README (README-UPSTREAM.md here).
        readme = tlg_run.REPO_DIR / 'README.md' if sys.platform == 'darwin' else logic.news_readme()
        if readme.is_file():
            tlg_run.open_external(readme)

    @Slot(str, str)
    def setFix(self, item_id, value):
        item = next(i for i in backports_table.ITEMS if i.id == item_id)
        self.fix_states[item_id] = backports_table.normalize(item, None if value == 'auto' else value)
        self._save_fixes()
        self.fixes.refresh_item(item_id)
        self.advanced.refresh_item(item_id)
        self.refresh()

    @Slot(str)
    def resetFixes(self, which):
        if which == 'previous':
            self.fix_states = backports_table.previous_release(self.fix_states)
        else:
            self.fix_states = backports_table.defaults(self.fix_states)
        self._save_fixes()
        self.fixes.refresh_all()
        self.advanced.refresh_all()
        self.refresh()

    def _save_fixes(self):
        if 'save_fixes' in self.probes:
            self.probes['save_fixes'](self.settings, self.fix_states)
        else:
            tlg_run.save_experimental(self.settings, self.fix_states)

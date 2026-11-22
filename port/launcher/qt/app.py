# SPDX-License-Identifier: GPL-2.0-or-later
"""Qt Quick application: the macOS style, the image providers, the QML engine and the window."""
import ctypes
import ctypes.util
import sys
from pathlib import Path

from PySide6.QtGui import QColor, QGuiApplication, QIcon, QImage, QImageReader, QPainter
from PySide6.QtQml import QQmlApplicationEngine
from PySide6.QtQuick import QQuickImageProvider, QQuickWindow
from PySide6.QtQuickControls2 import QQuickStyle

from . import logic
from .backend import Backend
from .controls import Controls
from .graphics import Graphics

HERE = Path(__file__).resolve().parent
QML_ROOT = HERE / 'qml'
ICONS = QML_ROOT / 'Launcher' / 'icons'


class IconProvider(QQuickImageProvider):
    """image://icon/<name>/<rrggbb>: the line icon icons/<name>.svg (Lucide) drawn in one color, at the
    size QML asks for."""

    def __init__(self):
        super().__init__(QQuickImageProvider.ImageType.Image)

    def requestImage(self, ident, size, requested):
        name, _, color = ident.partition('/')
        reader = QImageReader(str(ICONS / f'{name}.svg'))
        if requested.isValid():
            reader.setScaledSize(requested)
        image = reader.read().convertToFormat(QImage.Format.Format_ARGB32_Premultiplied)
        if not image.isNull():
            painter = QPainter(image)
            painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceIn)
            painter.fillRect(image.rect(), QColor('#' + color))
            painter.end()
        return image


def hide_title_text(window):
    """macOS: Qt draws the window title over the sidebar in the expanded title bar; hide it (titleVisibility)
    and keep it set, for the Window menu, Mission Control and VoiceOver."""
    objc = ctypes.cdll.LoadLibrary(ctypes.util.find_library('objc'))
    objc.sel_registerName.restype = ctypes.c_void_p
    objc.sel_registerName.argtypes = [ctypes.c_char_p]
    send = ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p)(('objc_msgSend', objc))
    send_long = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_long)(('objc_msgSend', objc))
    nswindow = send(int(window.winId()), objc.sel_registerName(b'window'))     # winId() is the window's NSView
    send_long(nswindow, objc.sel_registerName(b'setTitleVisibility:'), 1)       # NSWindowTitleHidden


class App:
    """Everything the window needs; `show()` for the real launcher, `grab()` for screenshots."""

    def __init__(self, backend=None, argv=None):
        self.qt = QGuiApplication.instance() or QGuiApplication(argv or sys.argv)
        self.qt.setApplicationName('infamous-launcher')
        self.qt.setDesktopFileName('infamous-pc')
        self.backend = backend or Backend()
        icon = HERE.parent / 'infamous.png'
        if icon.is_file():
            self.qt.setWindowIcon(QIcon(str(icon)))
        self.graphics = Graphics(self.backend, self.backend.probes.get('graphics'))
        self.controls = Controls(self.backend)
        QQuickStyle.setStyle('macOS')                       # native switches, pop-up buttons, sliders
        QQuickWindow.setTextRenderType(QQuickWindow.TextRenderType.NativeTextRendering)   # text as AppKit draws it
        self.engine = QQmlApplicationEngine()
        self.engine.addImageProvider('icon', IconProvider())
        self.engine.addImportPath(str(QML_ROOT))
        self.engine.rootContext().setContextProperty('Backend', self.backend)
        self.engine.rootContext().setContextProperty('Saves', self.backend.saves)
        self.engine.rootContext().setContextProperty('Graphics', self.graphics)
        self.engine.rootContext().setContextProperty('PadControls', self.controls)
        self.engine.rootContext().setContextProperty('Support', self.backend.support)
        self.engine.rootContext().setContextProperty('Updates', self.backend.updates)
        self.engine.loadFromModule('Launcher', 'Window')
        if not self.engine.rootObjects():
            raise RuntimeError('the QML interface did not load (see the messages above)')
        self.window = self.engine.rootObjects()[0]
        if self.qt.platformName() == 'cocoa':
            try:
                hide_title_text(self.window)
            except Exception as error:   # cosmetic: never a reason for the launcher not to start
                print(f'launcher: the title text stays visible ({error!r})', file=sys.stderr)

    def go(self, index):
        self.window.setProperty('current', index)

    def grab(self, path):
        """PNG of the current window contents (QQuickWindow.grabWindow)."""
        from PySide6.QtCore import QEventLoop, QTimer
        loop = QEventLoop()                       # let transitions (<= 250 ms) finish
        QTimer.singleShot(450, loop.quit)
        loop.exec()
        image = self.window.grabWindow()
        return image.save(str(path))

    def run(self):
        self.backend.start_probes()
        self.graphics.start()
        code = self.qt.exec()
        self.backend.shutdown()
        return code

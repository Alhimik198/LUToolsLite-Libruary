import ctypes
from ctypes import c_char_p, c_int, c_float, c_void_p, POINTER
import sys
import os
from PySide6.QtWidgets import QApplication, QMainWindow, QWidget, QVBoxLayout, QPushButton, QComboBox, QTextEdit, QFileDialog, QLabel
from PySide6.QtCore import Qt
from PySide6 import QtCore


# Глобальная переменная для логов
log_messages = []

# Загрузка DLL
try:
    lutools = ctypes.CDLL(r"D:\LT\LUToolsLite.dll")
except OSError as e:
    print(f"[Ошибка]: Не удалось загрузить DLL: {e}")
    exit(1)

# Настройка типов для функций
lutools.LUTools_Init.restype = c_int
lutools.LUTools_SetLogCallback.argtypes = [c_void_p, c_void_p]
lutools.LUTools_SetProgressCallback.argtypes = [c_void_p, c_void_p]
lutools.LUTools_GetLastErrorMessage.argtypes = [POINTER(c_char_p)]
lutools.LUTools_GetLastErrorMessage.restype = None
lutools.LUTools_Cleanup.restype = None

if hasattr(lutools, "LUTools_CreateLUTFromImages"):
    lutools.LUTools_CreateLUTFromImages.argtypes = [c_char_p, c_char_p, c_char_p, POINTER(c_int), c_int]
    lutools.LUTools_CreateLUTFromImages.restype = c_int
else:
    print("[Ошибка]: DLL не содержит LUTools_CreateLUTFromImages. Проверьте сборку библиотеки.")
    exit(1)

# Callback для логов
@ctypes.CFUNCTYPE(None, c_char_p, c_int, c_void_p)
def log_callback(message, is_error, user_data):
    msg = f"{'[Ошибка]' if is_error else '[Лог]'}: {message.decode('utf-8')}"
    log_messages.append(msg)

# Callback для прогресса
@ctypes.CFUNCTYPE(None, c_float, c_void_p)
def progress_callback(progress, user_data):
    msg = f"[Лог]: Прогресс создания LUT: {progress * 100:.1f}%"
    log_messages.append(msg)

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Тест создания LUT")
        self.setMinimumSize(600, 400)

        # Инициализация DLL
        if lutools.LUTools_Init() != 0:
            error_msg = c_char_p()
            lutools.LUTools_GetLastErrorMessage(ctypes.byref(error_msg))
            log_messages.append(f"[Ошибка]: Инициализация не удалась: {error_msg.value.decode('utf-8')}")
            print(f"[Ошибка]: Инициализация не удалась: {error_msg.value.decode('utf-8')}")
            exit(1)
        lutools.LUTools_SetLogCallback(log_callback, None)
        lutools.LUTools_SetProgressCallback(progress_callback, None)

        # GUI элементы
        self.central_widget = QWidget()
        self.setCentralWidget(self.central_widget)
        self.layout = QVBoxLayout()
        self.central_widget.setLayout(self.layout)

        # Поля для путей
        self.before_path = ""
        self.after_path = ""
        self.output_path = ""
        self.lut_size = 32

        # Кнопки и выбор размера
        self.select_before_button = QPushButton("Выбрать исходное изображение")
        self.select_before_button.clicked.connect(self.select_before_image)
        self.select_after_button = QPushButton("Выбрать обработанное изображение")
        self.select_after_button.clicked.connect(self.select_after_image)
        self.select_output_button = QPushButton("Выбрать путь для LUT")
        self.select_output_button.clicked.connect(self.select_output_path)
        self.lut_size_combo = QComboBox()
        self.lut_size_combo.addItems(["16", "32", "64"])
        self.lut_size_combo.setCurrentText("32")
        self.lut_size_combo.currentTextChanged.connect(self.update_lut_size)
        self.create_button = QPushButton("Создать LUT")
        self.create_button.clicked.connect(self.create_lut)
        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)

        # Добавление элементов в layout
        self.layout.addWidget(QLabel("Размер LUT:"))
        self.layout.addWidget(self.lut_size_combo)
        self.layout.addWidget(self.select_before_button)
        self.layout.addWidget(self.select_after_button)
        self.layout.addWidget(self.select_output_button)
        self.layout.addWidget(self.create_button)
        self.layout.addWidget(QLabel("Логи:"))
        self.layout.addWidget(self.log_text)

        # Таймер для обновления логов
        self.log_timer = QtCore.QTimer()
        self.log_timer.timeout.connect(self.update_logs)
        self.log_timer.start(100)

    def select_before_image(self):
        file_path, _ = QFileDialog.getOpenFileName(self, "Выбрать исходное изображение", "", "Images (*.jpg *.png)")
        if file_path:
            self.before_path = file_path
            log_messages.append(f"[Лог]: Выбрано исходное изображение: {file_path}")

    def select_after_image(self):
        file_path, _ = QFileDialog.getOpenFileName(self, "Выбрать обработанное изображение", "", "Images (*.jpg *.png)")
        if file_path:
            self.after_path = file_path
            log_messages.append(f"[Лог]: Выбрано обработанное изображение: {file_path}")

    def select_output_path(self):
        file_path, _ = QFileDialog.getSaveFileName(self, "Сохранить LUT", "", "LUT Files (*.cube)")
        if file_path:
            self.output_path = file_path
            log_messages.append(f"[Лог]: Выбран путь для LUT: {file_path}")

    def update_lut_size(self, size):
        try:
            self.lut_size = int(size)
            log_messages.append(f"[Лог]: Выбран размер LUT: {size}")
        except ValueError:
            log_messages.append("[Ошибка]: Неверный размер LUT")
            self.lut_size = 32
            self.lut_size_combo.setCurrentText("32")

    def create_lut(self):
        if not self.before_path or not os.path.exists(self.before_path):
            log_messages.append("[Ошибка]: Исходное изображение не выбрано или не существует")
            return
        if not self.after_path or not os.path.exists(self.after_path):
            log_messages.append("[Ошибка]: Обработанное изображение не выбрано или не существует")
            return
        if not self.output_path:
            log_messages.append("[Ошибка]: Путь для сохранения LUT не выбран")
            return

        lut_sizes = (c_int * 1)(self.lut_size)
        result = lutools.LUTools_CreateLUTFromImages(
            c_char_p(self.before_path.encode('utf-8')),
            c_char_p(self.after_path.encode('utf-8')),
            c_char_p(self.output_path.encode('utf-8')),
            lut_sizes, 1
        )

        if result == 0:
            log_messages.append(f"[Лог]: LUT успешно создан: {self.output_path}_{self.lut_size}.cube")
        else:
            error_msg = c_char_p()
            lutools.LUTools_GetLastErrorMessage(ctypes.byref(error_msg))
            log_messages.append(f"[Ошибка]: Не удалось создать LUT: {error_msg.value.decode('utf-8')}")

    def update_logs(self):
        if log_messages:
            self.log_text.append("\n".join(log_messages))
            log_messages.clear()

    def closeEvent(self, event):
        lutools.LUTools_Cleanup()
        event.accept()

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())
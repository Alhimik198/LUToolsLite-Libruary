import ctypes
from ctypes import c_char_p, c_int, c_float, POINTER, c_void_p, c_ubyte
import numpy as np
from PIL import Image
from PySide6.QtWidgets import QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QPushButton, QSlider, QLabel, QTextEdit, QFileDialog, QLineEdit
from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QImage, QPixmap
import sys
import os

# Загрузка DLL
try:
    lutools = ctypes.CDLL(r"D:\LT\LUToolsLite.dll")
except OSError as e:
    print(f"Ошибка загрузки DLL: {ctypes.WinError()}")
    exit(1)

# Настройка типов для функций
lutools.LUTools_Init.restype = c_int
lutools.LUTools_LoadLUT.argtypes = [c_char_p, c_float, POINTER(c_int)]
lutools.LUTools_LoadLUT.restype = c_int
lutools.LUTools_GeneratePreview.argtypes = [POINTER(c_ubyte), c_int, c_int, c_int, POINTER(c_int), c_int, c_float, c_float, c_float, c_float, c_float, c_int, c_int, POINTER(POINTER(c_ubyte)), POINTER(c_int), POINTER(c_int), POINTER(c_int)]
lutools.LUTools_GeneratePreview.restype = c_int
# --- новый PreviewFit  (может отсутствовать в старой DLL) -------
if hasattr(lutools, "LUTools_GeneratePreviewFit"):
    lutools.LUTools_GeneratePreviewFit.argtypes = [
        POINTER(c_ubyte),  # src data
        c_int, c_int, c_int,        # src W, H, C
        POINTER(c_int), c_int,      # lutIds*, lutCount
        c_float, c_float, c_float,  # WB, Tint, Bright
        c_float, c_float,           # Contrast, Saturation
        c_int, c_int,               # maxW, maxH
        POINTER(POINTER(c_ubyte)),  # *outData
        POINTER(c_int), POINTER(c_int), POINTER(c_int)  # outW, outH, outC
    ]
    lutools.LUTools_GeneratePreviewFit.restype = c_int
lutools.LUTools_ResizeImage.argtypes = [POINTER(c_ubyte), c_int, c_int, c_int, c_int, c_int, POINTER(POINTER(c_ubyte)), POINTER(c_int), POINTER(c_int), POINTER(c_int)]
lutools.LUTools_ResizeImage.restype = c_int
lutools.LUTools_ProcessFile.argtypes = [c_char_p, c_char_p, POINTER(c_int), c_int, c_float, c_float, c_float, c_float, c_float, c_void_p, c_void_p]
lutools.LUTools_ProcessFile.restype = c_int
lutools.LUTools_FreeMemory.argtypes = [c_void_p]

lutools.LUTools_SetLogCallback.argtypes = [c_void_p, c_void_p]
lutools.LUTools_GetLastErrorMessage.argtypes = [POINTER(c_char_p)]
lutools.LUTools_GetLastErrorMessage.restype = None
lutools.LUTools_Cleanup.restype = None

# Callback для логов
@ctypes.CFUNCTYPE(None, c_char_p, c_int, c_void_p)
def log_callback(message, is_error, user_data):
    global log_messages
    log_messages.append(f"{'[Ошибка]' if is_error else '[Лог]'}: {message.decode('utf-8')}")

# Глобальная переменная для логов
log_messages = []

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("LUTools GUI")
        self.setMinimumSize(900, 600)

        # Инициализация DLL
        if lutools.LUTools_Init() != 0:
            error_msg = c_char_p()
            lutools.LUTools_GetLastErrorMessage(ctypes.byref(error_msg))
            error = f"Ошибка инициализации: {error_msg.value.decode('utf-8')}"
            print(error)
            log_messages.append(error)
            exit(1)
        lutools.LUTools_SetLogCallback(log_callback, None)


        # Инициализация изображения
        self.image_path = r"D:\LT\test.jpg"
        self.img = None
        self.img_array = None
        self.width = 0
        self.height = 0
        self.channels = 3

        # Параметры цветокоррекции
        self.white_balance = 0.0
        self.tint = 0.0
        self.brightness = 0.0
        self.contrast = 0.0
        self.saturation = 0.0
        # Загрузка LUT
        self.lut_id = c_int()
        self.lut_path = r"D:\LT\BW6.cube"
        self.blend = 0.0
        self.load_lut(self.lut_path)
        # Параметры превью
        self.preview_width = 512
        self.preview_height = 512

        # Таймер для debounce
        self.preview_timer = QTimer()
        self.preview_timer.setSingleShot(True)
        self.preview_timer.timeout.connect(self.update_preview)

        # GUI элементы
        self.central_widget = QWidget()
        self.setCentralWidget(self.central_widget)
        self.layout = QHBoxLayout()
        self.central_widget.setLayout(self.layout)

        # Левый столбец: превью и кнопки
        self.preview_layout = QVBoxLayout()
        self.preview_label = QLabel("Превью не загружено")
        self.preview_label.setFixedSize(512, 512)
        self.preview_label.setAlignment(Qt.AlignCenter)
        self.load_image_button = QPushButton("Открыть изображение")
        self.load_image_button.clicked.connect(self.load_image)
        self.load_lut_button = QPushButton("Открыть LUT")
        self.load_lut_button.clicked.connect(self.load_lut_dialog)
        self.resize_button = QPushButton("Только ресайз")
        self.resize_button.clicked.connect(self.resize_image)
        self.save_button = QPushButton("Сохранить результат")
        self.save_button.clicked.connect(self.save_image)
        self.preview_layout.addWidget(self.preview_label)
        self.preview_layout.addWidget(self.load_image_button)
        self.preview_layout.addWidget(self.load_lut_button)
        self.preview_layout.addWidget(self.resize_button)
        self.preview_layout.addWidget(self.save_button)
        self.layout.addLayout(self.preview_layout)

        # Правый столбец: настройки и логи
        self.controls_layout = QVBoxLayout()
        self.create_slider("White Balance", self.white_balance, self.update_white_balance, -1.0, 1.0)
        self.create_slider("Tint", self.tint, self.update_tint, -1.0, 1.0)
        self.create_slider("Brightness", self.brightness, self.update_brightness, -1.0, 1.0)
        self.create_slider("Contrast", self.contrast, self.update_contrast, -1.0, 1.0)
        self.create_slider("Saturation", self.saturation, self.update_saturation, -1.0, 1.0)
        self.create_slider("LUT Blend", self.blend, self.update_blend, 0.0, 1.0)
        self.create_size_input("Preview Width", self.preview_width, self.update_preview_width)
        self.create_size_input("Preview Height", self.preview_height, self.update_preview_height)
        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)
        self.controls_layout.addWidget(self.log_text)
        self.layout.addLayout(self.controls_layout)

        # Таймер для обновления логов
        self.log_timer = QTimer()
        self.log_timer.timeout.connect(self.update_logs)
        self.log_timer.start(100)

        # Загрузка изображения по умолчанию
        self.load_image()

    def create_slider(self, label, value, callback, min_val, max_val):
        layout = QVBoxLayout()
        lbl = QLabel(f"{label}: {value:.2f}")
        slider = QSlider(Qt.Horizontal)
        slider.setRange(int(min_val * 100), int(max_val * 100))
        slider.setValue(int(value * 100))
        slider.valueChanged.connect(lambda val: callback(val / 100.0, lbl))
        layout.addWidget(lbl)
        layout.addWidget(slider)
        self.controls_layout.addLayout(layout)

    def create_size_input(self, label, value, callback):
        layout = QHBoxLayout()
        lbl = QLabel(label)
        input_field = QLineEdit(str(value))
        input_field.setFixedWidth(100)
        input_field.textChanged.connect(callback)
        layout.addWidget(lbl)
        layout.addWidget(input_field)
        self.controls_layout.addLayout(layout)

    def update_white_balance(self, value, label):
        self.white_balance = value
        label.setText(f"White Balance: {value:.2f}")
        self.preview_timer.start(100)

    def update_tint(self, value, label):
        self.tint = value
        label.setText(f"Tint: {value:.2f}")
        self.preview_timer.start(100)

    def update_brightness(self, value, label):
        self.brightness = value
        label.setText(f"Brightness: {value:.2f}")
        self.preview_timer.start(100)

    def update_contrast(self, value, label):
        self.contrast = value
        label.setText(f"Contrast: {value:.2f}")
        self.preview_timer.start(100)

    def update_saturation(self, value, label):
        self.saturation = value
        label.setText(f"Saturation: {value:.2f}")
        self.preview_timer.start(100)

    def update_blend(self, value, label):
        self.blend = value
        label.setText(f"LUT Blend: {value:.2f}")
        self.load_lut(self.lut_path)  # Перезагрузка LUT с новым blend
        self.preview_timer.start(500)

    def update_preview_width(self, text):
        try:
            self.preview_width = max(100, min(int(text), 1920))
        except ValueError:
            self.preview_width = 512
        self.preview_timer.start(300)

    def update_preview_height(self, text):
        try:
            self.preview_height = max(100, min(int(text), 1920))
        except ValueError:
            self.preview_height = 512
        self.preview_timer.start(300)

    def load_image(self):
        file_path, _ = QFileDialog.getOpenFileName(self, "Открыть изображение", "", "Images (*.jpg *.png)")
        if file_path:
            self.image_path = file_path
        try:
            self.img = Image.open(self.image_path).convert("RGB")
            self.img_array = np.array(self.img, dtype=np.uint8)
            self.width, self.height = self.img.size
            log_messages.append(f"[Лог]: Загружено изображение: {self.image_path}")
            self.update_preview()
        except Exception as e:
            log_messages.append(f"[Ошибка]: Не удалось загрузить изображение: {str(e)}")
            self.preview_label.setText("Ошибка загрузки изображения")

    def load_lut(self, lut_path):
        if self.lut_id.value:
            lutools.LUTools_UnloadLUT(self.lut_id.value)
    
    
        try:
            if lutools.LUTools_LoadLUT(c_char_p(lut_path.encode('utf-8')), c_float(self.blend), ctypes.byref(self.lut_id)) != 0:
                error_msg = c_char_p()
                lutools.LUTools_GetLastErrorMessage(ctypes.byref(error_msg))
                error = f"Ошибка загрузки LUT: {error_msg.value.decode('utf-8')}"
                print(error)
                log_messages.append(error)
                return False
            log_messages.append(f"[Лог]: Загружен LUT: {lut_path} с blend={self.blend:.2f}")
            self.lut_path = lut_path
            if self.img_array is not None:  # Проверка наличия img_array
                self.update_preview()
            return True
        except Exception as e:
            error = f"[Ошибка]: Не удалось загрузить LUT: {str(e)}"
            print(error)
            log_messages.append(error)
            return False
            
    def load_lut_dialog(self):
        lut_path, _ = QFileDialog.getOpenFileName(self, "Открыть LUT", "", "LUT Files (*.cube)")
        if lut_path:
            self.load_lut(lut_path)

    def resize_image(self):
        if self.img_array is None:
            log_messages.append("[Ошибка]: Изображение не загружено")
            return
        out_data = POINTER(c_ubyte)()
        out_width, out_height, out_channels = c_int(), c_int(), c_int()
        if not hasattr(lutools, "LUTools_ResizeImage"):
            log_messages.append("[Ошибка]: DLL собрана без LUTools_ResizeImage")
            return
        
        result = lutools.LUTools_ResizeImage(
            self.img_array.ctypes.data_as(POINTER(c_ubyte)), self.width, self.height, self.channels,
            self.preview_width, self.preview_height,
            ctypes.byref(out_data), ctypes.byref(out_width), ctypes.byref(out_height), ctypes.byref(out_channels)
        )
        if result != 0:
            error_msg = c_char_p()
            lutools.LUTools_GetLastErrorMessage(ctypes.byref(error_msg))
            log_messages.append(f"Ошибка ресайза: {error_msg.value.decode('utf-8')}")
            return
        try:
            preview_array = np.ctypeslib.as_array(
                out_data,
                shape=(out_height.value, out_width.value, out_channels.value)
            )
            stride = out_width.value * out_channels.value
            qimage  = QImage(preview_array.data, out_width.value, out_height.value, stride, QImage.Format_RGB888)
            pixmap  = QPixmap.fromImage(qimage.copy())  # делаем копию до free()
            self.preview_label.setPixmap(pixmap)
        finally:
            lutools.LUTools_FreeMemory(out_data)


    def save_image(self):
        if self.img_array is None:
            log_messages.append("[Ошибка]: Изображение не загружено")
            return
        save_path, _ = QFileDialog.getSaveFileName(self, "Сохранить изображение", "", "Images (*.jpg *.png)")
        if not save_path:
            return
        lut_ids = (c_int * 1)(self.lut_id.value)
        result = lutools.LUTools_ProcessFile(
            c_char_p(self.image_path.encode('utf-8')),
            c_char_p(save_path.encode('utf-8')),
            lut_ids, 1,
            c_float(self.white_balance), c_float(self.tint), c_float(self.brightness),
            c_float(self.contrast), c_float(self.saturation),
            log_callback, None
        )
        if result != 0:
            error_msg = c_char_p()
            lutools.LUTools_GetLastErrorMessage(ctypes.byref(error_msg))
            log_messages.append(f"Ошибка сохранения: {error_msg.value.decode('utf-8')}")
        else:
            log_messages.append(f"[Лог]: Изображение сохранено: {save_path}")

    def update_preview(self):
        if self.img_array is None:
            return
        lut_ids = (c_int * 1)(self.lut_id.value)
        preview_data = ctypes.POINTER(c_ubyte)()
        out_width, out_height, out_channels = c_int(), c_int(), c_int()
# выбираем подходящую функцию
        if hasattr(lutools, "LUTools_GeneratePreviewFit"):
            result = lutools.LUTools_GeneratePreviewFit(
                self.img_array.ctypes.data_as(POINTER(c_ubyte)),
                self.width, self.height, self.channels,
                lut_ids, 1,
                c_float(self.white_balance), c_float(self.tint), c_float(self.brightness),
                c_float(self.contrast), c_float(self.saturation),
                self.preview_width, self.preview_height,          # ► max side
                ctypes.byref(preview_data), ctypes.byref(out_width),
                ctypes.byref(out_height), ctypes.byref(out_channels)
            )
        else:
            result = lutools.LUTools_GeneratePreview(
                self.img_array.ctypes.data_as(POINTER(c_ubyte)),
                self.width, self.height, self.channels,
                lut_ids, 1,
                c_float(self.white_balance), c_float(self.tint), c_float(self.brightness),
                c_float(self.contrast), c_float(self.saturation),
                self.preview_width, self.preview_height,          # ► точный размер
                ctypes.byref(preview_data), ctypes.byref(out_width),
                ctypes.byref(out_height), ctypes.byref(out_channels)
            )

        if result != 0:
            error_msg = c_char_p()
            lutools.LUTools_GetLastErrorMessage(ctypes.byref(error_msg))
            log_messages.append(f"Ошибка генерации превью: {error_msg.value.decode('utf-8')}")
            return
        try:
            preview_array = np.ctypeslib.as_array(preview_data, shape=(out_height.value, out_width.value, out_channels.value))
            qimage = QImage(preview_array.data, out_width.value, out_height.value, out_width.value * out_channels.value, QImage.Format_RGB888)
            self.preview_label.setPixmap(QPixmap.fromImage(qimage.copy()))

            log_messages.append(f"[Лог]: Превью обновлено ({out_width.value}x{out_height.value})")
        finally:
            lutools.LUTools_FreeMemory(preview_data)

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
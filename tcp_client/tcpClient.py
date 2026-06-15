# ============================================================
# IMPORTS DE PYTHON ESTÁNDAR
# ============================================================
import sys          # Necesario para sys.argv al iniciar la app Qt
import socket       # Para crear y gestionar el socket TCP (vídeo)
import struct       # Para desempaquetar la cabecera "FRAM" + tamaño
import time         # Para medir tiempos (FPS, esperas, etc)

# ============================================================
# IMPORTS DE LIBRERÍAS EXTERNAS
# ============================================================
import numpy as np  # Para manejar los datos de imagen como arrays
import cv2          # OpenCV: para dibujar texto (FPS) y convertir formatos de imagen

# ============================================================
# IMPORTS DE PyQt5 (INTERFAZ GRÁFICA)
# ============================================================
from PyQt5.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QPushButton, QLineEdit
)
from PyQt5.QtCore import QThread, pyqtSignal
from PyQt5.QtGui import QImage, QPixmap


# ============================================================
# PARÁMETROS DE VÍDEO (VALORES POR DEFECTO)
# ============================================================
DEFAULT_IP     = "192.168.1.10"   # Dirección IP por defecto del servidor TCP
DEFAULT_PORT   = "5000"              # Puerto TCP por defecto

DEFAULT_WIDTH  = 640   # Ancho por defecto de la imagen en píxeles
DEFAULT_HEIGHT = 480   # Alto por defecto de la imagen en píxeles
DEFAULT_BPP    = 3     # Bytes por píxel (RGB888 -> 3 bytes: R, G, B)

# ============================================================
# PARÁMETROS DE LA VENTANA PRINCIPAL
# ============================================================
MAIN_WINDOW_TITLE = "Cliente TCP – Vídeo desde Zybo Z7"     # TITULO DE LA VENTANA PRINCIPAL

# ============================================================
# HILO TCP (RECEPCIÓN DE VÍDEO)
# ============================================================
class TcpReceiver(QThread):
    """
    Hilo dedicado a:
    - Conectarse al servidor TCP de la Zybo
    - Recibir continuamente frames de vídeo
    - Emitir cada frame a la interfaz mediante una señal Qt

    Se usa un hilo separado para NO bloquear la interfaz gráfica.
    """

    # SEÑALES
    frame_received = pyqtSignal(np.ndarray)     # Señal que envía un frame (numpy.ndarray) a la GUI
    connection_lost = pyqtSignal()              # Señal que avisa a la GUI de que se ha perdido la conexión

    def __init__(self, ip, port, width, height, bpp):
        """
        Constructor del hilo TCP.
        Recibe:
        - ip:   dirección IP del servidor (Zybo)
        - port: puerto TCP del servidor
        - width, height, bpp: parámetros de la imagen que esperamos recibir

        Guardamos estos parámetros en atributos de instancia para que
        el hilo sepa cómo interpretar los datos recibidos.
        """
        super().__init__()
        self.ip = ip                            # IP del servidor TCP
        self.port = port                        # Puerto del servidor TCP
        self.width = width                      # Ancho de la imagen
        self.height = height                    # Alto de la imagen
        self.bpp = bpp                          # Bytes por píxel (RGB888 = 3)
        self.frame_size = width * height * bpp  # Tamaño total del frame en bytes

        self.running = True                     # Flag para controlar el bucle principal del hilo
        self.sock = None                        # Socket TCP (se creará en run)

    def recv_all(self, size):
        """
        Recibe exactamente 'size' bytes del socket:
        - Sigue leyendo hasta completar el tamaño pedido
        - Lanza excepción si la conexión se cierra antes

        Parámetros:
        - size: número de bytes que queremos leer en total

        Devuelve:
        - data: bytes leídos del socket (longitud == size si todo va bien)
        """
        data = b''  # Acumulador de bytes recibidos
        while len(data) < size:
            # Recibe el resto que falta
            chunk = self.sock.recv(size - len(data))

            # Si no llega nada, la conexión se ha cerrado
            if not chunk:
                raise ConnectionError("Conexión cerrada por el servidor")
            data += chunk
        return data

    def run(self):
        """
        Bucle principal del hilo. Se ejecuta mientras self.running sea TRUE:
        - Crear socket
        - Conectarse a la Zybo
        - Recibir frames en bucle
        - Si hay error, emite connection_lost y reintenta tras 1 segundo
        """
        while self.running:
            try:
                
                # CREAR SOCKET Y CONECTARSE A LA ZYBO
                self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)   # Crear socket TCP IPv4
                self.sock.settimeout(5.0)                                       # Timeout para evitar bloqueos
                self.sock.connect((self.ip, self.port))                         # Conexión al servidor (Zybo)

                # BUCLE DE RECEPCION DE FRAMES
                while self.running:
                    # 1) Leer cabecera de 16 bytes:
                    #       "FRAM" (4 bytes)
                    #       width  (4 bytes)
                    #       height (4 bytes)
                    #       bpp    (4 bytes)
                    header = self.recv_all(16)

                    # Comprobar que los primeros 4 bytes son "FRAM"
                    if header[:4] != b"FRAM":
                        # Si no coincide, algo va mal: salir del bucle
                        break

                    # Extraer width, height, bpp (enteros de 32 bits big-endian)
                    width, height, bpp = struct.unpack("!III", header[4:16])

                    # Actualizar parámetros internos dinámicamente
                    self.width = width
                    self.height = height
                    self.bpp = bpp
                    self.frame_size = width * height * bpp

                    # 2) Recibir los datos del frame (frame_size bytes)
                    frame_data = self.recv_all(self.frame_size)

                    # 3) Convertir los bytes a un array numpy de forma (HEIGHT, WIDTH, 3)
                    #       - Usamos los parámetros de instancia (height, width, bpp).
                    #       - Para vídeo RGB888, bpp debe ser 3.
                    frame = np.frombuffer(frame_data, dtype=np.uint8)

                    # Comprobación básica: el tamaño del buffer debe ser width*height*bpp
                    if frame.size != self.frame_size:
                        # Si no coincide, algo ha ido mal en la recepción o en la configuración
                        break

                    # Reorganizar buffer a una imagen de 3 canales (alto, ancho, canales)
                    frame = frame.reshape((self.height, self.width, self.bpp))

                    # 4) Emitir el frame a la interfaz (se ejecutará update_frame en la GUI)
                    self.frame_received.emit(frame)

            except:
                # Si ocurre cualquier error avisamos a la GUI y esperamos un segundo antes de reintentar
                self.connection_lost.emit()
                time.sleep(1)

        # Al salir del bucle principal, cerramos el socket si existe
        if self.sock:
            try:
                self.sock.close()
            except:
                pass

    def stop(self):
        """
        Método para detener el hilo desde fuera.
        - Pone running = False
        - Cierra el socket para desbloquear cualquier recv()
        """
        self.running = False
        if self.sock:
            try:
                self.sock.close()
            except:
                pass


# ============================================================
# INTERFAZ PRINCIPAL (VENTANA)
# ============================================================
class MainWindow(QWidget):
    """
    Clase que define la ventana principal de la aplicación.

    Contiene:
    - Controles para configurar y conectar el vídeo TCP
    - Un QLabel donde se muestra el vídeo
    - Lógica para gestionar el hilo TCP y actualizar la interfaz
    """

    def __init__(self):
        super().__init__()
        self.setWindowTitle(MAIN_WINDOW_TITLE)

        # ----------------------------------------------------
        # 1) WIDGETS PARA TCP (VÍDEO)
        # ----------------------------------------------------
        # Parámetros servidor TCP
        self.ip_input = QLineEdit(DEFAULT_IP)               # Campo de texto para la IP del servidor (Zybo)
        self.port_input = QLineEdit(DEFAULT_PORT)           # Campo de texto para el puerto TCP

        # Botón conectar/desconectar el vídeo
        self.tcp_btn = QPushButton("Conectar vídeo")

        # ----------------------------------------------------
        # 2) WIDGET PARA MOSTRAR EL VÍDEO
        # ----------------------------------------------------
        self.video_label = QLabel()                                     # QLabel donde se mostrará el frame de vídeo
        self.video_label.setFixedSize(DEFAULT_WIDTH, DEFAULT_HEIGHT)    # (Inicialmente lo configuramos con el tamaño por defecto)
        self.video_label.setStyleSheet("background-color: black;")      # # Fondo negro por defecto (cuando no hay vídeo)

        # ----------------------------------------------------
        # 3) LAYOUTS (ORGANIZACION VISUAL)
        # ----------------------------------------------------
        # Layout horizontal para la parte TCP (IP, puerto, ancho, alto, bpp, botón)
        tcp_layout = QHBoxLayout()
        tcp_layout.addWidget(QLabel("IP:"))
        tcp_layout.addWidget(self.ip_input)
        tcp_layout.addWidget(QLabel("Puerto:"))
        tcp_layout.addWidget(self.port_input)
        tcp_layout.addWidget(self.tcp_btn)

        # Layout vertical principal
        main_layout = QVBoxLayout()
        main_layout.addLayout(tcp_layout)
        main_layout.addWidget(self.video_label)

        # Asignar layout principal a la ventana
        self.setLayout(main_layout)

        # ----------------------------------------------------
        # 4) ESTADO INTERNO
        # ----------------------------------------------------
        self.tcp_receiver = None        # Referencia al hilo TCP
        self.tcp_connected = False      # Indica si el vídeo está conectado

        # ----------------------------------------------------
        # 6) VARIABLES PARA FPS
        # ----------------------------------------------------
        self.frame_count = 0            # Contador de frames recibidos desde el último cálculo
        self.last_time = time.time()    # Tiempo de referencia para el cálculo de FPS
        self.fps = 0.0                  # Último valor de FPS calculado

        # ----------------------------------------------------
        # 7) CONEXIÓN DE SEÑALES (BOTONES)
        # ----------------------------------------------------
        self.tcp_btn.clicked.connect(self.toggle_tcp)    # Boton TCP: Cuando se pulsa el botón TCP, ejecutar toggle_tcp()

    # --------------------------------------------------------
    # CONECTAR / DESCONECTAR TCP
    # --------------------------------------------------------
    def toggle_tcp(self):
        """
        Gestiona el botón de conectar/desconectar vídeo TCP.
        """
        if not self.tcp_connected:
            # Leer IP y puerto de los campos de texto
            ip = self.ip_input.text().strip()

            # Intentar convertir el puerto a entero
            try:
                port = int(self.port_input.text().strip())
            except ValueError:
                # Si el puerto no es un entero válido, mostramos mensaje y salimos
                print("[TCP] Puerto inválido")
                return

            # Crear hilo TCP con esos parámetros
            self.tcp_receiver = TcpReceiver(ip, port, 0, 0, 0)
            self.tcp_receiver.frame_received.connect(self.update_frame)             # Conectar la señal frame_received al método update_frame
            self.tcp_receiver.connection_lost.connect(self.on_tcp_disconnected)     # Conectar la señal connection_lost al método on_tcp_disconnected
            self.tcp_receiver.start()                                               # Arrancar el hilo

            # Actualizar estado y texto del botón
            self.tcp_btn.setText("Desconectar vídeo")
            self.tcp_connected = True
            print(f"[TCP] Conectando a {ip}:{port}")

        else:
            # Si ya estaba conectado, detenemos el hilo
            if self.tcp_receiver:
                self.tcp_receiver.stop()
                self.tcp_receiver = None

            # Limpiar la pantalla (ponerla en negro)
            self.video_label.clear()
            self.video_label.setStyleSheet("background-color: black;")
            self.video_label.setFixedSize(DEFAULT_WIDTH, DEFAULT_HEIGHT)

            # Actualizar estado y texto del botón
            self.tcp_btn.setText("Conectar vídeo")
            self.tcp_connected = False
            print("[TCP] Vídeo desconectado")

    def on_tcp_disconnected(self):
        """
        Se llama cuando el hilo TCP emite la señal connection_lost.
        Actualiza el estado y el texto del botón y escribe en el log.
        """
        #self.tcp_connected = False
        #self.tcp_btn.setText("Conectar vídeo")
        print("[TCP] Conexión perdida. Reintentando...")

    # --------------------------------------------------------
    # ACTUALIZAR FRAME (CON FPS Y SOPORTE MONOCROMO)
    # --------------------------------------------------------
    def update_frame(self, frame):
        """
        Recibe un frame desde el hilo TCP, calcula los FPS,
        dibuja el texto de FPS sobre la imagen y la muestra en el QLabel.
        Soporta tanto flujos RGB (3 canales) como Escala de Grises (1 canal).
        """
        # 0) Extraer las dimensiones reales del frame recibido
        if len(frame.shape) == 3:
            h, w, ch = frame.shape  # Flujo de Color (ej: 480, 640, 3)
        else:
            h, w = frame.shape      # Flujo de Grises (ej: 480, 640)
            ch = 1

        # Ajustar el tamaño del QLabel al tamaño real del frame recibido
        self.video_label.setFixedSize(w, h)

        # 1) Actualizar contador de frames
        self.frame_count += 1
        now = time.time()                   # Tiempo actual
        elapsed = now - self.last_time      # Tiempo transcurrido

        # 2) Si ha pasado al menos 1 segundo, recalcular FPS
        if elapsed >= 1.0:
            self.fps = self.frame_count / elapsed
            self.frame_count = 0
            self.last_time = now

        # 3) Hacer copia del frame para poder modificarlo con OpenCV
        frame = frame.copy()

        # 4) Dibujar el texto de FPS en la esquina superior izquierda
        # Si la imagen es en escala de grises (1 canal), el color del texto debe ser un entero (255 = Blanco)
        color_texto = (0, 255, 0) if ch == 3 else 255

        cv2.putText(
            frame,
            f"FPS: {self.fps:.2f}",   # Texto a mostrar
            (10, 30),                 # Posición (x, y) en píxeles
            cv2.FONT_HERSHEY_SIMPLEX, # Tipo de fuente
            1.0,                      # Escala de la fuente
            color_texto,              # Color adaptativo
            2                         # Grosor de la línea
        )

        # 5) Convertir el buffer y definir el formato correcto para Qt
        if ch == 3:
            # Vídeo en Color: OpenCV lee en BGR, convertimos a RGB para Qt
            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            bytes_per_line = ch * w
            qimg = QImage(rgb.data, w, h, bytes_per_line, QImage.Format_RGB888)
        else:
            # Vídeo en Blanco y Negro (1 canal): No requiere conversión de OpenCV
            bytes_per_line = 1 * w
            qimg = QImage(frame.data, w, h, bytes_per_line, QImage.Format_Grayscale8)

        # 6) Convertir QImage a QPixmap y mostrarlo en el QLabel
        self.video_label.setPixmap(QPixmap.fromImage(qimg))


# ============================================================
# PUNTO DE ENTRADA (MAIN)
# ============================================================
if __name__ == "__main__":
    app = QApplication(sys.argv)
    win = MainWindow()
    win.show()
    sys.exit(app.exec_())

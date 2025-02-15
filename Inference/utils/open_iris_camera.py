import time
from serial.tools import list_ports
from serial import Serial
import cv2
import numpy as np
import traceback

# Serial communication protocol:
# header-begin (2 bytes)
# header-type (2 bytes)
# packet-size (2 bytes)
# packet (packet-size bytes)
ETVR_HEADER = b'\xff\xa0'
ETVR_HEADER_FRAME = b'\xff\xa1'
ETVR_HEADER_LEN = 6
WAIT_TIME = 0.1

class Camera():
    def __init__(self, port):
        self.port = port
        ser = Serial(
            port = self.port,
            baudrate=3000000,
            timeout=1,
            xonxoff=False,
            dsrdtr=False,
            rtscts=False,
        )
        time.sleep(WAIT_TIME)
        self.serial_connection = ser
        print(f"[INFO] ETVR Serial Tracker device connected on {self.port}")
        self.buffer = b''
        time.sleep(WAIT_TIME)

    def get_next_packet_bounds(self):
        beg = -1
        while beg == -1:
            self.buffer += self.serial_connection.read(2048)
            beg = self.buffer.find(ETVR_HEADER + ETVR_HEADER_FRAME)
        # Discard any data before the frame header.
        if beg > 0:
            self.buffer = self.buffer[beg:]
            beg = 0
        # We know exactly how long the jpeg packet is
        end = int.from_bytes(self.buffer[4:6], signed=False, byteorder="little")
        self.buffer += self.serial_connection.read(end - len(self.buffer))
        return beg, end

    def get_next_jpeg_frame(self):
        beg, end = self.get_next_packet_bounds()
        jpeg = self.buffer[beg+ETVR_HEADER_LEN:end+ETVR_HEADER_LEN]
        self.buffer = self.buffer[end+ETVR_HEADER_LEN:]
        return jpeg

    def get_serial_camera_picture(self):
        ret = False
        conn = self.serial_connection
        if conn is None:
            print('none')
            return False, None
        try:
            if conn.in_waiting:
                jpeg = self.get_next_jpeg_frame()
                if jpeg:
                    # Pass recieved jpeg frame since it's already encoded
                    #image = jpeg
                    image = cv2.imdecode(np.fromstring(jpeg, dtype=np.uint8), cv2.IMREAD_UNCHANGED)
                    ret = True
                    if image is None:
                        print(f"[WARN] Frame drop. Corrupted JPEG.")
                        return False, None
                    # Discard the serial buffer. This is due to the fact that it
                    # may build up some outdated frames. A bit of a workaround here tbh.
                    if conn.in_waiting >= 8192:
                        print(f"[INFO] Discarding the serial buffer ({conn.in_waiting} bytes)")
                        conn.reset_input_buffer()
                        self.buffer = b''
                return True, image
            return False, None
        except Exception as e:
            print(e)
            print(traceback.format_exc())
            print(f"[WARN] Serial capture source problem, assuming camera disconnected, waiting for reconnect.")
            conn.close()
            pass
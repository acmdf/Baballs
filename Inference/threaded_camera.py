import cv2
import threading

class ThreadedCamera:
    def __init__(self, source=0):
        self.source = source
        self.cap = cv2.VideoCapture(self.source)
        self.lock = threading.Lock()
        self.running = True
        self.frame = None
        self.thread = threading.Thread(target=self.update, daemon=True)
        self.thread.start()
    
    def update(self):
        while self.running:
            ret, frame = self.cap.read()
            if ret:
                with self.lock:
                    self.frame = frame
    
    def read(self):
        with self.lock:
            return self.frame.copy() if self.frame is not None else None
        
    def isOpened(self):
        return self.cap.isOpened()
    
    def stop(self):
        self.running = False
        self.thread.join()
        self.cap.release()
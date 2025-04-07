import io
import time
from typing import Iterator

from fastapi import FastAPI, Response
from fastapi.responses import StreamingResponse
import numpy as np
import cv2
from PIL import Image, ImageDraw, ImageFont

app = FastAPI()

def generate_blue_frame(counter: int) -> bytes:
    """Generate a blue image with a counter number rendered on it."""
    # Create a blue image using numpy
    img = np.zeros((128, 128, 3), dtype=np.uint8)
    img[:, :] = (255, 0, 0)  # Blue in BGR format
    
    # Convert to PIL for text rendering
    pil_img = Image.fromarray(cv2.cvtColor(img, cv2.COLOR_BGR2RGB))
    draw = ImageDraw.Draw(pil_img)
    
    # Add counter text in white
    draw.text((50, 50), str(counter), fill=(255, 255, 255))
    
    # Convert back to OpenCV format
    img = cv2.cvtColor(np.array(pil_img), cv2.COLOR_RGB2BGR)
    
    # Encode as JPEG
    _, jpeg_data = cv2.imencode('.jpg', img)
    return jpeg_data.tobytes()

def mjpeg_stream() -> Iterator[bytes]:
    """Generate an MJPEG stream at 30 FPS."""
    counter = 0
    frame_delay = 1/30  # 30 FPS
    
    while True:
        # Generate frame with current counter
        jpeg_data = generate_blue_frame(counter)
        
        # Format for MJPEG streaming
        yield b'--frame\r\nContent-Type: image/jpeg\r\n\r\n' + jpeg_data + b'\r\n'
        
        # Increment counter
        counter += 1
        if counter > 999:
            counter = 0
            
        # Delay to maintain 30 FPS
        time.sleep(frame_delay)

@app.get("/")
async def index():
    return {"message": "MJPEG Stream available at /stream"}

@app.get("/stream")
async def video_feed():
    return StreamingResponse(
        mjpeg_stream(),
        media_type="multipart/x-mixed-replace; boundary=frame"
    )

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
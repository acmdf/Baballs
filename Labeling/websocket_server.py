import asyncio
import websockets
import cv2
import json
import os
import time
import csv

from copy import deepcopy

# --- Global Setup ---
# Initialize OpenCV camera (change index if needed)
cap = cv2.VideoCapture(1)
ndc_data = {"x": 0, "y": 0, "z": 0}  # This will store the latest NDC data

# Create a directory to store frame images
frames_dir = "frames"
os.makedirs(frames_dir, exist_ok=True)

# Open a CSV file and write the header (one CSV for all frames)
csv_filename = "ndc_data.csv"
csv_file = open(csv_filename, mode="w", newline="")
csv_writer = csv.writer(csv_file)
csv_writer.writerow(["frame", "timestamp", "ndc_x", "ndc_y", "ndc_z"])

frame_count = 0  # To count frames

# --- WebSocket Server Setup ---
async def websocket_handler(websocket, path):
    global ndc_data
    async for message in websocket:
        try:
            # Update the global NDC data with incoming JSON from Unity
            ndc_data = json.loads(message)
        except json.JSONDecodeError:
            print("Error decoding JSON")

async def start_websocket():
    server = await websockets.serve(websocket_handler, "localhost", 8080)
    print("WebSocket server started on ws://localhost:8080")
    await server.wait_closed()

# --- OpenCV Camera Loop ---
def start_cv2_camera():
    global frame_count, ndc_data
    while True:
        ret, frame = cap.read()
        if not ret:
            print("Failed to grab frame")
            break

        height, width, _ = frame.shape

        # Convert NDC (range [-1,1]) to pixel coordinates on the frame
        screen_x = int((ndc_data["x"] + 1) * 0.5 * width)
        screen_y = int((1 - ndc_data["y"]) * 0.5 * height)  # Invert Y since image coordinates start at top-left

        # Draw a green circle at the mapped NDC position and overlay the NDC text
        frame = cv2.rotate(frame, cv2.ROTATE_90_CLOCKWISE)
        clean_frame = deepcopy(frame)
        cv2.circle(frame, (screen_x, screen_y), 10, (0, 255, 0), -1)
        cv2.putText(frame, f"NDC: {ndc_data}", (20, 40),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)

        # Save the frame as a PNG file
        frame_filename = os.path.join(frames_dir, f"frame_{frame_count:06d}.png")
        clean_frame = cv2.resize(clean_frame, (256,256))
        cv2.imwrite(frame_filename, clean_frame)

        # Write a row in the CSV file with frame number, timestamp, and NDC data
        timestamp = time.time()
        csv_writer.writerow([frame_count, timestamp, ndc_data["x"], ndc_data["y"], ndc_data["z"]])
        csv_file.flush()  # Ensure the row is written immediately

        # Display the frame
        cv2.imshow("Webcam with NDC Overlay", frame)

        frame_count += 1

        if cv2.waitKey(1) & 0xFF == ord("q"):
            break

    # Cleanup: close CSV file when done
    csv_file.close()

# --- Main Function to Run Both Tasks in Parallel ---
async def main():
    loop = asyncio.get_running_loop()
    # Run the CV2 camera loop in a separate thread
    loop.run_in_executor(None, start_cv2_camera)
    # Run the WebSocket server (this will run until closed)
    await start_websocket()

# --- Start the Program ---
asyncio.run(main())

# Final cleanup (if main exits)
cap.release()
cv2.destroyAllWindows()

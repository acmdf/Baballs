#!/usr/bin/env python3
"""
Dataset Scrubber - A simple video editor-style interface for browsing eye tracking datasets.

This tool loads binary capture files and provides a scrubbing interface to view:
- Left and right eye images
- All tracking labels and parameters
- Decoded routine state flags
- Frame-by-frame navigation with slider

Usage: python dataset_scrubber.py [dataset_file.bin]
"""

import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import struct
import cv2
import numpy as np
from PIL import Image, ImageTk
import io
import os

# Flag definitions (from flags.h)
FLAG_DEFINITIONS = {
    0: "ROUTINE_1", 1: "ROUTINE_2", 2: "ROUTINE_3", 3: "ROUTINE_4", 4: "ROUTINE_5",
    5: "ROUTINE_6", 6: "ROUTINE_7", 7: "ROUTINE_8", 8: "ROUTINE_9", 9: "ROUTINE_10",
    10: "ROUTINE_11", 11: "ROUTINE_12", 12: "ROUTINE_13", 13: "ROUTINE_14", 14: "ROUTINE_15",
    15: "ROUTINE_16", 16: "ROUTINE_17", 17: "ROUTINE_18", 18: "ROUTINE_19", 19: "ROUTINE_20",
    20: "ROUTINE_21", 21: "ROUTINE_22", 22: "ROUTINE_23", 23: "ROUTINE_24",
    24: "CONVERGENCE", 25: "IN_MOVEMENT", 26: "RESTING", 27: "DILATION_BLACK",
    28: "DILATION_WHITE", 29: "DILATION_GRADIENT", 30: "GOOD_DATA", 31: "ROUTINE_COMPLETE"
}

class DatasetScrubber:
    def __init__(self, root):
        self.root = root
        self.root.title("Dataset Scrubber - Eye Tracking Data Viewer")
        self.root.geometry("1400x800")
        
        # Data storage
        self.frames = []
        self.current_frame_idx = 0
        self.total_frames = 0
        
        # Setup UI
        self.setup_ui()
        
        # Auto-load if dataset file provided as argument
        import sys
        if len(sys.argv) > 1 and os.path.exists(sys.argv[1]):
            self.load_dataset(sys.argv[1])
    
    def setup_ui(self):
        """Create the user interface."""
        # Main container
        main_frame = ttk.Frame(self.root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Top section - File controls
        file_frame = ttk.Frame(main_frame)
        file_frame.pack(fill=tk.X, pady=(0, 10))
        
        ttk.Button(file_frame, text="Load Dataset", command=self.browse_dataset).pack(side=tk.LEFT)
        self.file_label = ttk.Label(file_frame, text="No dataset loaded")
        self.file_label.pack(side=tk.LEFT, padx=(10, 0))
        
        # Middle section - Images and data
        content_frame = ttk.Frame(main_frame)
        content_frame.pack(fill=tk.BOTH, expand=True)
        
        # Left side - Images
        image_frame = ttk.LabelFrame(content_frame, text="Eye Images")
        image_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(0, 10))
        
        # Eye image displays
        self.left_eye_label = ttk.Label(image_frame, text="Left Eye")
        self.left_eye_label.pack(pady=5)
        self.left_eye_canvas = tk.Canvas(image_frame, width=320, height=240, bg='black')
        self.left_eye_canvas.pack(pady=5)
        
        self.right_eye_label = ttk.Label(image_frame, text="Right Eye")
        self.right_eye_label.pack(pady=5)
        self.right_eye_canvas = tk.Canvas(image_frame, width=320, height=240, bg='black')
        self.right_eye_canvas.pack(pady=5)
        
        # Right side - Data display
        data_frame = ttk.LabelFrame(content_frame, text="Frame Data")
        data_frame.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True)
        
        # Frame info
        info_frame = ttk.Frame(data_frame)
        info_frame.pack(fill=tk.X, padx=10, pady=5)
        
        self.frame_info_label = ttk.Label(info_frame, text="Frame: 0 / 0", font=('Arial', 12, 'bold'))
        self.frame_info_label.pack()
        
        # Scrollable text for labels
        text_frame = ttk.Frame(data_frame)
        text_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)
        
        self.data_text = tk.Text(text_frame, font=('Courier', 10), wrap=tk.WORD)
        scrollbar = ttk.Scrollbar(text_frame, orient=tk.VERTICAL, command=self.data_text.yview)
        self.data_text.configure(yscrollcommand=scrollbar.set)
        
        self.data_text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Bottom section - Scrubbing controls
        control_frame = ttk.Frame(main_frame)
        control_frame.pack(fill=tk.X, pady=(10, 0))
        
        # Timeline slider
        self.timeline_var = tk.IntVar()
        self.timeline_slider = ttk.Scale(
            control_frame, 
            from_=0, 
            to=100,
            orient=tk.HORIZONTAL,
            variable=self.timeline_var,
            command=self.on_timeline_change
        )
        self.timeline_slider.pack(fill=tk.X, pady=5)
        
        # Playback controls
        button_frame = ttk.Frame(control_frame)
        button_frame.pack()
        
        ttk.Button(button_frame, text="⏮", command=self.first_frame).pack(side=tk.LEFT, padx=2)
        ttk.Button(button_frame, text="⏪", command=self.prev_frame_10).pack(side=tk.LEFT, padx=2)
        ttk.Button(button_frame, text="◀", command=self.prev_frame).pack(side=tk.LEFT, padx=2)
        ttk.Button(button_frame, text="▶", command=self.next_frame).pack(side=tk.LEFT, padx=2)
        ttk.Button(button_frame, text="⏩", command=self.next_frame_10).pack(side=tk.LEFT, padx=2)
        ttk.Button(button_frame, text="⏭", command=self.last_frame).pack(side=tk.LEFT, padx=2)
        
        # Frame entry
        entry_frame = ttk.Frame(control_frame)
        entry_frame.pack(pady=5)
        
        ttk.Label(entry_frame, text="Jump to frame:").pack(side=tk.LEFT)
        self.frame_entry = ttk.Entry(entry_frame, width=10)
        self.frame_entry.pack(side=tk.LEFT, padx=5)
        self.frame_entry.bind('<Return>', self.jump_to_frame)
        ttk.Button(entry_frame, text="Go", command=self.jump_to_frame).pack(side=tk.LEFT)
    
    def browse_dataset(self):
        """Open file browser to select dataset file."""
        filename = filedialog.askopenfilename(
            title="Select Dataset File",
            filetypes=[("Binary files", "*.bin"), ("All files", "*.*")]
        )
        if filename:
            self.load_dataset(filename)
    
    def load_dataset(self, filename):
        """Load dataset from binary file."""
        try:
            self.file_label.config(text=f"Loading: {os.path.basename(filename)}...")
            self.root.update()
            
            # CaptureFrame struct format (from capture_data.h)
            struct_format = '<11f3Q3I'  # 12 floats, 3 uint64, 3 uint32
            frame_size = struct.calcsize(struct_format)
            
            self.frames = []
            
            with open(filename, 'rb') as f:
                frame_count = 0
                while True:
                    # Read frame header
                    frame_data = f.read(frame_size)
                    if not frame_data or len(frame_data) < frame_size:
                        break
                    
                    # Unpack frame metadata
                    unpacked = struct.unpack(struct_format, frame_data)
                    
                    (routine_pitch, routine_yaw, routine_distance, fov_adjust_distance,
                     routine_left_lid, routine_right_lid, routine_brow_raise, routine_brow_angry,
                     routine_widen, routine_squint, routine_dilate,
                     timestamp, timestamp_left, timestamp_right,
                     routine_state, jpeg_data_left_length, jpeg_data_right_length) = unpacked
                    
                    # Read image data
                    image_left_data = f.read(jpeg_data_left_length) if jpeg_data_left_length > 0 else None
                    image_right_data = f.read(jpeg_data_right_length) if jpeg_data_right_length > 0 else None
                    
                    # Store frame data
                    frame = {
                        'routine_pitch': routine_pitch,
                        'routine_yaw': routine_yaw,
                        'routine_distance': routine_distance,
                        'fov_adjust_distance': fov_adjust_distance,
                        'routine_left_lid': routine_left_lid,
                        'routine_right_lid': routine_right_lid,
                        'routine_brow_raise': routine_brow_raise,
                        'routine_brow_angry': routine_brow_angry,
                        'routine_widen': routine_widen,
                        'routine_squint': routine_squint,
                        'routine_dilate': routine_dilate,
                        'timestamp': timestamp,
                        'timestamp_left': timestamp_left,
                        'timestamp_right': timestamp_right,
                        'routine_state': routine_state,
                        'image_left': image_left_data,
                        'image_right': image_right_data
                    }
                    
                    self.frames.append(frame)
                    frame_count += 1
                    
                    # Update progress for large files
                    if frame_count % 100 == 0:
                        self.file_label.config(text=f"Loading: {frame_count} frames...")
                        self.root.update()
            
            self.total_frames = len(self.frames)
            
            if self.total_frames == 0:
                messagebox.showerror("Error", "No frames found in dataset file")
                return
            
            # Update UI
            self.timeline_slider.config(to=self.total_frames - 1)
            self.current_frame_idx = 0
            self.file_label.config(text=f"{os.path.basename(filename)} - {self.total_frames} frames")
            
            # Display first frame
            self.update_display()
            
        except Exception as e:
            messagebox.showerror("Error", f"Failed to load dataset: {str(e)}")
    
    def decode_flags(self, flags):
        """Decode routine state flags into readable list."""
        active_flags = []
        for bit, name in FLAG_DEFINITIONS.items():
            if flags & (1 << bit):
                active_flags.append(name)
        return active_flags
    
    def format_timestamp(self, timestamp):
        """Format timestamp for display."""
        if timestamp == 0:
            return "N/A"
        # Convert microseconds to seconds with 3 decimal places
        return f"{timestamp / 1_000_000:.3f}s"
    
    def update_display(self):
        """Update the display with current frame data."""
        if not self.frames or self.current_frame_idx >= len(self.frames):
            return
        
        frame = self.frames[self.current_frame_idx]
        
        # Update frame info
        self.frame_info_label.config(text=f"Frame: {self.current_frame_idx + 1} / {self.total_frames}")
        
        # Update timeline slider
        self.timeline_var.set(self.current_frame_idx)
        
        # Update images
        self.display_image(frame['image_left'], self.left_eye_canvas, "Left Eye")
        self.display_image(frame['image_right'], self.right_eye_canvas, "Right Eye")
        
        # Update data display
        self.update_data_display(frame)
    
    def display_image(self, image_data, canvas, title):
        """Display JPEG image data on canvas."""
        canvas.delete("all")
        
        if not image_data:
            canvas.create_text(160, 120, text=f"No {title} Data", fill="white", font=('Arial', 12))
            return
        
        try:
            # Decode JPEG data
            nparr = np.frombuffer(image_data, np.uint8)
            cv_img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
            
            if cv_img is None:
                canvas.create_text(160, 120, text=f"Invalid {title} Image", fill="red", font=('Arial', 12))
                return
            
            # Convert BGR to RGB
            cv_img = cv2.cvtColor(cv_img, cv2.COLOR_BGR2RGB)
            
            # Resize to fit canvas while maintaining aspect ratio
            height, width = cv_img.shape[:2]
            canvas_width, canvas_height = 320, 240
            
            scale = min(canvas_width / width, canvas_height / height)
            new_width = int(width * scale)
            new_height = int(height * scale)
            
            cv_img = cv2.resize(cv_img, (new_width, new_height))
            
            # Convert to PIL and then to Tkinter format
            pil_img = Image.fromarray(cv_img)
            tk_img = ImageTk.PhotoImage(pil_img)
            
            # Center image on canvas
            x = (canvas_width - new_width) // 2
            y = (canvas_height - new_height) // 2
            
            canvas.create_image(x, y, anchor=tk.NW, image=tk_img)
            canvas.image = tk_img  # Keep reference to prevent garbage collection
            
        except Exception as e:
            canvas.create_text(160, 120, text=f"Error: {str(e)}", fill="red", font=('Arial', 10))
    
    def update_data_display(self, frame):
        """Update the data text display with frame information."""
        self.data_text.delete(1.0, tk.END)
        
        # Gaze tracking data
        self.data_text.insert(tk.END, "=== GAZE TRACKING ===\\n", "header")
        self.data_text.insert(tk.END, f"Pitch: {frame['routine_pitch']:.4f}\\n")
        self.data_text.insert(tk.END, f"Yaw: {frame['routine_yaw']:.4f}\\n")
        self.data_text.insert(tk.END, f"Distance: {frame['routine_distance']:.4f}\\n")
        self.data_text.insert(tk.END, f"FOV Adjust Distance: {frame['fov_adjust_distance']:.4f}\\n\\n")
        
        # Facial expressions
        self.data_text.insert(tk.END, "=== FACIAL EXPRESSIONS ===\\n", "header")
        self.data_text.insert(tk.END, f"Left Lid: {frame['routine_left_lid']:.4f}\\n")
        self.data_text.insert(tk.END, f"Right Lid: {frame['routine_right_lid']:.4f}\\n")
        self.data_text.insert(tk.END, f"Brow Raise: {frame['routine_brow_raise']:.4f}\\n")
        self.data_text.insert(tk.END, f"Brow Angry: {frame['routine_brow_angry']:.4f}\\n")
        self.data_text.insert(tk.END, f"Widen: {frame['routine_widen']:.4f}\\n")
        self.data_text.insert(tk.END, f"Squint: {frame['routine_squint']:.4f}\\n")
        self.data_text.insert(tk.END, f"Dilate: {frame['routine_dilate']:.4f}\\n\\n")
        
        # Timestamps
        self.data_text.insert(tk.END, "=== TIMESTAMPS ===\\n", "header")
        self.data_text.insert(tk.END, f"Main: {self.format_timestamp(frame['timestamp'])}\\n")
        self.data_text.insert(tk.END, f"Left Eye: {self.format_timestamp(frame['timestamp_left'])}\\n")
        self.data_text.insert(tk.END, f"Right Eye: {self.format_timestamp(frame['timestamp_right'])}\\n\\n")
        
        # Routine state flags
        active_flags = self.decode_flags(frame['routine_state'])
        self.data_text.insert(tk.END, "=== ROUTINE STATE FLAGS ===\\n", "header")
        self.data_text.insert(tk.END, f"Raw Value: 0x{frame['routine_state']:08X} ({frame['routine_state']})\\n")
        if active_flags:
            for flag in active_flags:
                self.data_text.insert(tk.END, f"• {flag}\\n")
        else:
            self.data_text.insert(tk.END, "No flags active\\n")
        
        # Configure text tags for headers
        self.data_text.tag_configure("header", font=('Arial', 10, 'bold'))
    
    # Navigation methods
    def on_timeline_change(self, value):
        """Handle timeline slider change."""
        self.current_frame_idx = int(float(value))
        self.update_display()
    
    def first_frame(self):
        """Go to first frame."""
        self.current_frame_idx = 0
        self.update_display()
    
    def last_frame(self):
        """Go to last frame."""
        self.current_frame_idx = max(0, self.total_frames - 1)
        self.update_display()
    
    def prev_frame(self):
        """Go to previous frame."""
        if self.current_frame_idx > 0:
            self.current_frame_idx -= 1
            self.update_display()
    
    def next_frame(self):
        """Go to next frame."""
        if self.current_frame_idx < self.total_frames - 1:
            self.current_frame_idx += 1
            self.update_display()
    
    def prev_frame_10(self):
        """Go back 10 frames."""
        self.current_frame_idx = max(0, self.current_frame_idx - 10)
        self.update_display()
    
    def next_frame_10(self):
        """Go forward 10 frames."""
        self.current_frame_idx = min(self.total_frames - 1, self.current_frame_idx + 10)
        self.update_display()
    
    def jump_to_frame(self, event=None):
        """Jump to specific frame number."""
        try:
            frame_num = int(self.frame_entry.get())
            if 1 <= frame_num <= self.total_frames:
                self.current_frame_idx = frame_num - 1
                self.update_display()
            else:
                messagebox.showerror("Error", f"Frame number must be between 1 and {self.total_frames}")
        except ValueError:
            messagebox.showerror("Error", "Please enter a valid frame number")

def main():
    """Main entry point."""
    root = tk.Tk()
    app = DatasetScrubber(root)
    root.mainloop()

if __name__ == "__main__":
    main()
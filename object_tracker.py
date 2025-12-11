#!/usr/bin/env python3
"""
Green Ball Tracker with TCP Output and Visual Display
Tracks green objects, displays camera feed with detection visualization,
and sends raw position data to VESC Tool over TCP.

Protocol:
- Connection: TCP to localhost:65102 (default)
- Message format: "JS:forward:turn\n"
  where:
  - forward: Raw object size (radius in pixels)
  - turn: Raw distance from center (pixels, negative = left, positive = right)

Display:
- Shows camera feed with detected objects highlighted
- Press 'q' to exit
"""

import cv2
import numpy as np
from picamera2 import Picamera2
import time
import socket
import argparse
import sys

class GreenBallTracker:
    def __init__(self, host='localhost', port=65102):
        # TCP connection parameters
        self.host = host
        self.port = port
        self.socket = None
        self.connected = False
        
        # Initialize camera
        self.picam2 = Picamera2()
        config = self.picam2.create_preview_configuration(
            main={"size": (1280, 720), "format": "RGB888"}
        )
        self.picam2.configure(config)
        self.picam2.start()
        time.sleep(2)  # Camera warm-up
        
        # Green color range in HSV
        self.lower_green = np.array([35, 50, 50])
        self.upper_green = np.array([85, 255, 255])
        
        # Detection parameters
        self.min_radius = 10
        self.max_radius = 200
        
    def connect(self):
        """Connect to the VESC Tool TCP server"""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.connect((self.host, self.port))
            self.connected = True
            print(f"Connected to VESC Tool at {self.host}:{self.port}")
            return True
        except ConnectionRefusedError:
            print(f"Connection refused. Make sure VESC Tool is running and TCP server is enabled on port {self.port}")
            return False
        except Exception as e:
            print(f"Connection error: {e}")
            return False
    
    def disconnect(self):
        """Disconnect from the VESC Tool TCP server"""
        if self.socket:
            self.socket.close()
        self.connected = False
        print("Disconnected from TCP server")
        
    def send_joystick_data(self, forward, turn):
        """
        Send joystick data to VESC Tool
        
        Args:
            forward (float): Raw object size (radius in pixels)
            turn (float): Raw distance from center (pixels, negative = left, positive = right)
        """
        if not self.connected:
            return False
        
        # No clamping - send raw values
        
        try:
            message = f"JS:{forward:.4f}:{turn:.4f}\n"
            self.socket.sendall(message.encode('utf-8'))
            return True
        except Exception as e:
            print(f"Error sending data: {e}")
            self.connected = False
            return False
        
    def detect_green_ball(self, frame):
        """Detect green ball in frame using color detection"""
        # Convert to HSV color space
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
        
        # Create mask for green color
        mask = cv2.inRange(hsv, self.lower_green, self.upper_green)
        
        # Apply morphological operations to reduce noise
        kernel = np.ones((5, 5), np.uint8)
        mask = cv2.erode(mask, kernel, iterations=2)
        mask = cv2.dilate(mask, kernel, iterations=2)
        
        # Find contours
        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, 
                                      cv2.CHAIN_APPROX_SIMPLE)
        
        if len(contours) == 0:
            return None, None
        
        # Find the largest contour
        largest_contour = max(contours, key=cv2.contourArea)
        
        # Get minimum enclosing circle
        ((x, y), radius) = cv2.minEnclosingCircle(largest_contour)
        
        # Only return if radius is within acceptable range
        if self.min_radius < radius < self.max_radius:
            # Get moments for more accurate center
            M = cv2.moments(largest_contour)
            if M["m00"] > 0:
                center_x = int(M["m10"] / M["m00"])
                center_y = int(M["m01"] / M["m00"])
                return (center_x, center_y), int(radius)
        
        return None, None
    
    def run(self):
        """Main tracking loop"""
        try:
            # Create a named window
            cv2.namedWindow("Green Ball Tracker", cv2.WINDOW_NORMAL)
            cv2.resizeWindow("Green Ball Tracker", 640, 360)  # Half resolution for display
            
            while True:
                # Capture frame
                frame = self.picam2.capture_array()
                frame = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)
                
                # Make a copy for display
                display_frame = frame.copy()
                
                # Detect green ball
                position, radius = self.detect_green_ball(frame)
                
                # Process detection results
                if position is not None:
                    x, y = position
                    # Calculate raw left/right distance from center (positive = right, negative = left)
                    frame_width = 1280
                    position_lr = x - (frame_width / 2)
                    
                    # Use raw radius value
                    forward_value = radius
                    
                    # Send data to VESC Tool
                    self.send_joystick_data(forward_value, position_lr)
                    
                    # Display raw values
                    print(f"Size: {radius}, Position from center: {position_lr} pixels")
                    
                    # Draw the circle and center point on the display frame
                    cv2.circle(display_frame, (x, y), radius, (0, 255, 0), 2)
                    cv2.circle(display_frame, (x, y), 5, (0, 0, 255), -1)
                    
                    # Add text with information
                    cv2.putText(display_frame, f"Size: {radius}", (10, 30), 
                                cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)
                    cv2.putText(display_frame, f"Position: {position_lr:.1f}", (10, 60), 
                                cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)
                else:
                    # No object detected, send zero values
                    self.send_joystick_data(0.0, 0.0)
                    print("No object detected - sending (0.0, 0.0)")
                    
                    # Add text indicating no detection
                    cv2.putText(display_frame, "No object detected", (10, 30), 
                                cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)
                
                # Display the frame
                cv2.imshow("Green Ball Tracker", display_frame)
                
                # Check for key press - exit on 'q'
                key = cv2.waitKey(1) & 0xFF
                if key == ord('q'):
                    break
                
                # Small delay
                time.sleep(0.05)  # 20Hz update rate
                
        except KeyboardInterrupt:
            print("\nExiting...")
        finally:
            # Clean up
            cv2.destroyAllWindows()
            self.picam2.stop()
            self.disconnect()

def main():
    parser = argparse.ArgumentParser(description='Green Ball Tracker with TCP Output')
    parser.add_argument('--host', default='localhost', help='VESC Tool host (default: localhost)')
    parser.add_argument('--port', type=int, default=65102, help='VESC Tool TCP port (default: 65102)')
    
    args = parser.parse_args()
    
    # Create tracker and connect to TCP server
    tracker = GreenBallTracker(host=args.host, port=args.port)
    if not tracker.connect():
        print("Failed to connect to TCP server. Exiting.")
        sys.exit(1)
    
    # Start tracking
    tracker.run()

if __name__ == "__main__":
    main()
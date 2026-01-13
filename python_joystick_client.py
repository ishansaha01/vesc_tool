#!/usr/bin/env python3
"""
VESC Tool Joystick Control Client

This script sends joystick control commands to the VESC Tool over TCP.
It can be used with various input devices or as a template for integration
with other Python applications.

Protocol:
- Connection: TCP to localhost:65102 (default)
- Message format: "JS:forward:turn\n"
  where forward and turn are floating point values between -1.0 and 1.0
  - forward: -1.0 (full reverse) to 1.0 (full forward)
  - turn: -1.0 (full left) to 1.0 (full right)

Example:
  "JS:0.5:0.0\n" - Half speed forward, no turning
  "JS:0.0:0.5\n" - No forward/backward motion, turning right at half rate
  "JS:0.5:-0.5\n" - Half speed forward while turning left at half rate
"""

import socket
import time
import argparse
import sys
import threading

try:
    import pygame
    PYGAME_AVAILABLE = True
except ImportError:
    PYGAME_AVAILABLE = False
    print("pygame not available. Joystick support disabled.")

class VescJoystickClient:
    def __init__(self, host='localhost', port=65102):
        self.host = host
        self.port = port
        self.socket = None
        self.connected = False
        self.running = True
        
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
        
    def send_joystick_data(self, forward, turn):
        """
        Send joystick data to VESC Tool
        
        Args:
            forward (float): Forward/backward value between -1.0 and 1.0
            turn (float): Turn left/right value between -1.0 and 1.0
        """
        if not self.connected:
            return False
        
        # Clamp values to valid range
        forward = max(-1.0, min(1.0, forward))
        turn = max(-1.0, min(1.0, turn))
        
        try:
            message = f"JS:{forward:.4f}:{turn:.4f}\n"
            self.socket.sendall(message.encode('utf-8'))
            return True
        except Exception as e:
            print(f"Error sending data: {e}")
            self.connected = False
            return False

def keyboard_control_loop(client):
    """Simple keyboard control loop using input()"""
    print("\nKeyboard Control Mode")
    print("Enter values as 'forward,turn' (e.g., '0.5,0.2')")
    print("Values should be between -1.0 and 1.0")
    print("Type 'q' to quit\n")
    
    while client.running:
        cmd = input("Enter joystick values (forward,turn): ")
        if cmd.lower() == 'q':
            client.running = False
            break
            
        try:
            parts = cmd.split(',')
            if len(parts) == 2:
                forward = float(parts[0])
                turn = float(parts[1])
                client.send_joystick_data(forward, turn)
                print(f"Sent: forward={forward:.2f}, turn={turn:.2f}")
        except ValueError:
            print("Invalid input. Use format: forward,turn")
        except Exception as e:
            print(f"Error: {e}")

def pygame_joystick_loop(client):
    """Joystick control loop using pygame"""
    pygame.init()
    pygame.joystick.init()
    
    # Check for joysticks
    joystick_count = pygame.joystick.get_count()
    if joystick_count == 0:
        print("No joysticks found!")
        return
    
    # Initialize the first joystick
    joystick = pygame.joystick.Joystick(0)
    joystick.init()
    print(f"Using joystick: {joystick.get_name()}")
    
    # Main loop
    clock = pygame.time.Clock()
    print("\nJoystick Control Mode")
    print("Use left stick for control")
    print("Press CTRL+C to exit\n")
    
    try:
        while client.running:
            # Process pygame events
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    client.running = False
            
            # Get joystick values (assuming left stick is axes 0 and 1)
            try:
                # Y-axis is usually inverted in pygame
                forward = -joystick.get_axis(1)  # Up/down
                turn = joystick.get_axis(0)      # Left/right
                
                # Apply deadzone
                deadzone = 0.05
                if abs(forward) < deadzone:
                    forward = 0.0
                if abs(turn) < deadzone:
                    turn = 0.0
                
                client.send_joystick_data(forward, turn)
                
                # Don't spam the console
                # print(f"Sent: forward={forward:.2f}, turn={turn:.2f}")
            except Exception as e:
                print(f"Joystick error: {e}")
            
            # Limit update rate
            clock.tick(20)  # 20 Hz update rate
    except KeyboardInterrupt:
        print("\nExiting...")
    finally:
        pygame.quit()

def main():
    parser = argparse.ArgumentParser(description='VESC Tool Joystick Control Client')
    parser.add_argument('--host', default='localhost', help='VESC Tool host (default: localhost)')
    parser.add_argument('--port', type=int, default=65102, help='VESC Tool TCP port (default: 65102)')
    parser.add_argument('--mode', choices=['keyboard', 'joystick'], default='keyboard',
                        help='Control mode: keyboard or joystick (default: keyboard)')
    
    args = parser.parse_args()
    
    # Check if joystick mode is requested but pygame is not available
    if args.mode == 'joystick' and not PYGAME_AVAILABLE:
        print("Error: Joystick mode requires pygame. Please install it with:")
        print("  pip install pygame")
        sys.exit(1)
    
    # Create client and connect
    client = VescJoystickClient(host=args.host, port=args.port)
    if not client.connect():
        print("Failed to connect. Exiting.")
        sys.exit(1)
    
    try:
        # Start control loop based on selected mode
        if args.mode == 'joystick' and PYGAME_AVAILABLE:
            pygame_joystick_loop(client)
        else:
            keyboard_control_loop(client)
    except KeyboardInterrupt:
        print("\nExiting...")
    finally:
        # Clean up
        client.disconnect()
        print("Disconnected.")

if __name__ == "__main__":
    main()

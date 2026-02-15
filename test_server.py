#!/usr/bin/env python3
"""
Simple HTTP server to receive BLE data from ESP32-C3
Run: python3 test_server.py
"""

from http.server import HTTPServer, BaseHTTPRequestHandler
import json
import datetime

class BLEDataHandler(BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path == '/ble-data':
            content_length = int(self.headers['Content-Length'])
            post_data = self.rfile.read(content_length)
            
            try:
                data = json.loads(post_data.decode('utf-8'))
                timestamp = datetime.datetime.now().strftime('%H:%M:%S.%f')[:-3]
                
                print(f"\n[{timestamp}] BLE Device:")
                print(f"  MAC:  {data.get('mac', 'N/A')}")
                print(f"  RSSI: {data.get('rssi', 'N/A')} dBm")
                print(f"  Data: {data.get('data', 'N/A')}")
                
                # Send OK response
                self.send_response(200)
                self.send_header('Content-type', 'application/json')
                self.end_headers()
                self.wfile.write(b'{"status":"ok"}')
                
            except json.JSONDecodeError as e:
                print(f"ERROR: Invalid JSON: {e}")
                self.send_response(400)
                self.end_headers()
        else:
            self.send_response(404)
            self.end_headers()
    
    def log_message(self, format, *args):
        # Suppress default HTTP logging
        pass

if __name__ == '__main__':
    PORT = 5000
    server = HTTPServer(('0.0.0.0', PORT), BLEDataHandler)
    print(f"🎧 BLE Data Server listening on port {PORT}")
    print(f"   Waiting for data from ESP32-C3...")
    print(f"   Press Ctrl+C to stop\n")
    
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n\n✓ Server stopped")
        server.shutdown()

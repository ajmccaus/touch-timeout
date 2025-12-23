#!/usr/bin/env python3
"""
http-wake.py - HTTP endpoint for waking touch-timeout display

Listens on 127.0.0.1:8765/wake and sends SIGUSR1 to touch-timeout daemon.
Enables containerized services and external systems to wake the display.

Install:  sudo scripts/install-http-wake.sh
Test:     curl -X POST http://127.0.0.1:8765/wake

Integration - shairport-sync (HifiBerryOS):

    1. Edit /data/extensions/shairport/docker/pause-others.sh:
        #!/bin/sh
        curl -X POST http://172.17.0.1:81/api/player/stop-all/shairport-sync
        curl -sS -X POST http://127.0.0.1:8765/wake

    2. Mount script into container - add to docker-compose.yaml volumes:
        - /data/extensions/shairport/docker/pause-others.sh:/pause-others.sh

    3. Restart container:
        cd /data/extensions/shairport && docker-compose down && docker-compose up -d

Integration - other services:

    curl -sS -X POST http://127.0.0.1:8765/wake

Troubleshooting:

    Service logs:     journalctl -u http-wake -n 20
    Port in use:      lsof -i :8765
    Test signal:      pkill -USR1 touch-timeout
"""
from http.server import HTTPServer, BaseHTTPRequestHandler
import subprocess
import sys

class WakeHandler(BaseHTTPRequestHandler):
    """HTTP request handler that sends wake signal to touch-timeout"""

    def do_POST(self):
        """Handle POST request to /wake endpoint"""
        if self.path == "/wake":
            try:
                subprocess.run(
                    ["pkill", "-USR1", "touch-timeout"],
                    check=True,
                    capture_output=True
                )
                self.send_response(200)
                self.send_header("Content-Type", "text/plain")
                self.end_headers()
                self.wfile.write(b"Display wake signal sent\n")
            except subprocess.CalledProcessError as e:
                self.send_response(500)
                self.send_header("Content-Type", "text/plain")
                self.end_headers()
                error_msg = f"Error sending signal: {e}\n"
                self.wfile.write(error_msg.encode())
        else:
            self.send_response(404)
            self.send_header("Content-Type", "text/plain")
            self.end_headers()
            self.wfile.write(b"Not found. Use POST /wake\n")

    def log_message(self, format, *args):
        """Suppress HTTP access logs to reduce journal noise"""
        pass


if __name__ == "__main__":
    try:
        server = HTTPServer(("127.0.0.1", 8765), WakeHandler)
        print("touch-timeout HTTP wake endpoint listening on 127.0.0.1:8765",
              file=sys.stderr)
        server.serve_forever()
    except OSError as e:
        print(f"Error starting server: {e}", file=sys.stderr)
        print("Is another instance already running? Check: lsof -i :8765",
              file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        print("\nShutting down...", file=sys.stderr)
        sys.exit(0)

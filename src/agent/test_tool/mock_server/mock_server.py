# -*- coding: utf-8 -*-
import json
import os
import ssl
import jwt
import threading
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from datetime import datetime, timedelta
from urllib.parse import urlparse, parse_qs, unquote


# Configuration of paths
RESPONSES_DIR = 'responses'
GROUPS_FILES_DIR = 'group_files'

# Secret key to sign the JWT (you can change it for a secure key)
SECRET_KEY = "my_secret_key"

def generate_authentication_response(expiration_seconds=60):
    # Set the expiration time in seconds
    expiration_time = datetime.utcnow() + timedelta(seconds=expiration_seconds)
    expiration_timestamp = int(expiration_time.timestamp())

    # Generate the JWT token with the data and the configured expiration time
    payload = {
        "iss": "wazuh",
        "aud": "Wazuh Communications API",
        "iat": datetime.utcnow(),
        "exp": expiration_time,
        "uuid": "edab9ef6-f02d-4a4b-baa4-f2ad12789890"
    }

    token = jwt.encode(payload, SECRET_KEY, algorithm="HS256")

    response = {
        "token": token,
        "exp": expiration_timestamp,
        "error": 0
    }

    return json.dumps(response)

class MockHandler(BaseHTTPRequestHandler):
    def _set_headers(self, code, content_type="application/json", content_length=None):
        self.send_response(code)
        self.send_header("Content-type", content_type)
        if content_length:
            self.send_header("Content-Length", str(content_length))
        self.send_header("Connection", "close")
        self.end_headers()

    def _load_response(self, filename):
        try:
            with open(os.path.join(RESPONSES_DIR, filename), 'r') as f:
                return f.read()
        except FileNotFoundError:
            return json.dumps({"error": "Response file not found"})

    def do_POST(self):
        response = None

        if self.path == "/security/user/authenticate":
            response = self._load_response("user_authenticate.json")
            self._set_headers(code=200, content_length=len(response))
        elif self.path == "/agents":
            response = self._load_response("agents.json")
            self._set_headers(code=200, content_length=len(response))
        elif self.path == "/api/v1/authentication":
            response = generate_authentication_response()
            self._set_headers(code=200, content_length=len(response))
        elif self.path == "/api/v1/events/stateful":
            #response = self._load_response("events_stateful.json")
            self._set_headers(code=200) # If an response is given, add the size of the response.
        elif self.path == "/api/v1/events/stateless":
            #response = self._load_response("events_stateless.json")
            self._set_headers(code=200) # If an response is given, add the size of the response.
        else:
            self.send_error(404, f"Not found: {self.path}")
            return

        if response:
            self.wfile.write(response.encode('utf-8'))
        self.wfile.flush()
        self.close_connection = True

    def do_GET(self):
        response = None

        if self.path == "/api/v1/commands":
            response = self._load_response("commands.json")
            self._set_headers(code=200, content_length=len(response))
        elif self.path.startswith("/api/v1/files"):
            parsed_url = urlparse(self.path)
            query_params = parse_qs(parsed_url.query)
            file_name = query_params.get("file_name", [None])[0]
            if file_name:
                filepath = os.path.join(GROUPS_FILES_DIR, file_name)
                filepath = unquote(filepath)

                if os.path.isfile(filepath):
                    try:
                        self.send_response(200)
                        self.send_header('Content-Type', 'application/octet-stream')
                        self.send_header('Content-Disposition', f'attachment; filename="{os.path.basename(filepath)}"')
                        self.send_header('Content-Length', str(os.path.getsize(filepath)))
                        self.send_header('Connection', 'close')
                        self.end_headers()

                        with open(filepath, 'rb') as file:
                            file_data = file.read()
                            self.wfile.write(file_data)

                        self.wfile.flush()
                    except Exception as e:
                        self.send_response(500)
                        self.end_headers()
                        self.wfile.write(f'Error reading file: {str(e)}'.encode())
            else:
                self._set_headers(code=400)
        else:
            self.send_error(404, f"Not found: {self.path}")

        if response:
            self.wfile.write(response.encode('utf-8'))
        self.wfile.flush()
        self.close_connection = True

def run_server(port, ssl_key=None, ssl_cert=None, handler_class=MockHandler, use_https=True):
    server_address = ('', port)

    # Use HTTPS if both an SSL certificate and key are provided and the protocol is HTTPS
    if use_https and ssl_key and ssl_cert:
        httpd = ThreadingHTTPServer(server_address, handler_class)
        context = ssl.create_default_context(ssl.Purpose.CLIENT_AUTH)
        context.load_cert_chain(certfile=ssl_cert, keyfile=ssl_key)
        httpd.socket = context.wrap_socket(httpd.socket, server_side=True)
        protocol = "HTTPS"
    else:
        # If not using HTTPS, default to HTTP
        httpd = ThreadingHTTPServer(server_address, handler_class)
        protocol = "HTTP"

    print(f"Mock {protocol} server running on port {port}")
    httpd.serve_forever()

if __name__ == "__main__":
    # Argument parser setup
    parser = argparse.ArgumentParser(description="Configurable Mock Server")

    # Define command-line arguments
    parser.add_argument(
        "--port1", type=int, default=55000,
        help="Port number for the first server (default: 55000)"
    )
    parser.add_argument(
        "--port2", type=int, default=27000,
        help="Port number for the second server (default: 27000)"
    )
    parser.add_argument(
        "--ssl_key", type=str, default="key.pem",
        help="Path to the SSL key file (default: key.pem)"
    )
    parser.add_argument(
        "--ssl_cert", type=str, default="cert.pem",
        help="Path to the SSL certificate file (default: cert.pem)"
    )
    parser.add_argument(
        "--protocol", type=str, choices=["http", "https"], default="https",
        help="Specify whether to use HTTP or HTTPS (default: https)"
    )

    # Parse the command-line arguments
    args = parser.parse_args()

    use_https = args.protocol == "https"

    # Create threads for each server with the configured parameters
    thread1 = threading.Thread(target=run_server, args=(args.port1, args.ssl_key, args.ssl_cert, MockHandler, use_https))
    thread2 = threading.Thread(target=run_server, args=(args.port2, args.ssl_key, args.ssl_cert, MockHandler, use_https))

    # Start servers
    thread1.start()
    thread2.start()

    # Wait for both threads to finish
    thread1.join()
    thread2.join()

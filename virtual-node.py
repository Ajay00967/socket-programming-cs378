import socket
import struct
import sys

ON_IP = sys.argv[1]
UDP_PORT = int(sys.argv[2])

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((ON_IP, 5000))

vn_ip = s.getsockname()[0]

msg = struct.pack("!4sH", socket.inet_aton(vn_ip), UDP_PORT)
s.sendall(msg)

print(f"Sent CONNECT: {vn_ip}:{UDP_PORT}")
input("Press Enter to disconnect...")
s.close()

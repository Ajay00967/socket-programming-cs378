import socket
import struct
import sys
import time
import select

def recvAll(s, size):
    data = b""

    while len(data) < size:
        part = s.recv(size - len(data))
        if not part:
            break
        data += part

    return data

if len(sys.argv) != 3:
    print("Usage: python3 virtual-node.py ON_IP UDP_PORT")
    raise SystemExit(1)

ON_IP = sys.argv[1]
UDP_PORT = int(sys.argv[2])

udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
udp.bind(("", UDP_PORT))

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((ON_IP, 5000))

vn_ip = s.getsockname()[0]
msg = struct.pack("!4sH", socket.inet_aton(vn_ip), UDP_PORT)
s.sendall(msg)

data = recvAll(s, 1)

if len(data) != 1:
    raise SystemExit(1)

n = data[0]
data = recvAll(s, n * 11)

if len(data) != n * 11:
    raise SystemExit(1)

neighbours = {}
my_id = None

for i in range(n):
    entry = data[i * 11:(i + 1) * 11]
    node, ip, port, cost = struct.unpack("!c4sHI", entry)

    node = node.decode()
    ip = socket.inet_ntoa(ip)

    if cost == 0:
        my_id = node
    else:
        neighbours[node] = (ip, port, cost)

if my_id is None:
    raise SystemExit("Node ID not received")

# print("My node:", my_id)
# print("Neighbours:")

# for node, (ip, port, cost) in neighbours.items():
#     print(node, ip, port, cost)

print(",".join([f"{my_id}=0"] + [f"{node}={cost}" for node, (ip, port, cost) in neighbours.items()]))

def make_lsp(my_id, seq, neighbours):
    packet = struct.pack("!cIB", my_id.encode(), seq, len(neighbours))

    for node, (ip, port, cost) in neighbours.items():
        packet += struct.pack("!cI", node.encode(), cost)

    return packet

def print_graph(known):
    print("Known graph:")

    for node in sorted(known):
        links = known[node]
        print(node + ":", end=" ")

        print(", ".join(
            f"{neighbour}={cost}"
            for neighbour, cost in sorted(links.items())
        ))

def send_lsp(packet, neighbours, skip=None):
    for node, (ip, port, cost) in neighbours.items():
        if (ip, port) != skip:
            udp.sendto(packet, (ip, port))

seq = 1
packet = make_lsp(my_id, seq, neighbours)

latest_seq = {my_id: seq}
known = {
    my_id: {
        node: cost
        for node, (ip, port, cost) in neighbours.items()
    }
}

send_lsp(packet, neighbours)

last_lsp = time.monotonic()
last_graph = time.monotonic()

s.setblocking(False)
tcp_buffer = b""

while True:
    now = time.monotonic()

    if now - last_lsp >= 15:
        seq += 1
        packet = make_lsp(my_id, seq, neighbours)
        latest_seq[my_id] = seq
        send_lsp(packet, neighbours)
        last_lsp = now

    if now - last_graph >= 15:
        print_graph(known)
        last_graph = now

    ready, _, _ = select.select([s, udp], [], [], 1)

    for sock in ready:
        if sock == s:
            data = s.recv(4096)

            if not data:
                print("Oracle Node disconnected")
                raise SystemExit(0)

            tcp_buffer += data

            while len(tcp_buffer) >= 1:
                n = tcp_buffer[0]
                size = 1 + n * 11

                if len(tcp_buffer) < size:
                    break

                data = tcp_buffer[1:size]
                tcp_buffer = tcp_buffer[size:]

                new_neighbours = {}
                new_id = my_id

                for i in range(n):
                    entry = data[i * 11:(i + 1) * 11]
                    node, ip, port, cost = struct.unpack("!c4sHI", entry)

                    node = node.decode()
                    ip = socket.inet_ntoa(ip)

                    if cost == 0:
                        new_id = node
                    else:
                        new_neighbours[node] = (ip, port, cost)

                my_id = new_id
                neighbours = new_neighbours

                known[my_id] = {
                    node: cost
                    for node, (ip, port, cost) in neighbours.items()
                }

                seq += 1
                packet = make_lsp(my_id, seq, neighbours)
                latest_seq[my_id] = seq

                send_lsp(packet, neighbours)
                last_lsp = time.monotonic()

                # print("Updated neighbours:")
                # print("My node:", my_id)

                # for node, (ip, port, cost) in neighbours.items():
                #     print(node, ip, port, cost)
                
                print(",".join([f"{my_id}=0"] + [f"{node}={cost}" for node, (ip, port, cost) in neighbours.items()]))

                print_graph(known)

        else:
            data, addr = udp.recvfrom(1024)

            if len(data) < 6:
                continue

            origin, rseq, count = struct.unpack("!cIB", data[:6])
            origin = origin.decode()

            if len(data) != 6 + count * 5:
                continue

            if origin == my_id:
                continue

            if origin not in latest_seq or rseq > latest_seq[origin]:
                latest_seq[origin] = rseq

                links = {}

                for i in range(count):
                    node, cost = struct.unpack(
                        "!cI", data[6 + i * 5:11 + i * 5]
                    )
                    links[node.decode()] = cost

                known[origin] = links

                print("\nReceived LSP from", origin, "sequence", rseq)
                print_graph(known)

                send_lsp(data, neighbours, addr)
                
                
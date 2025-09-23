import network
import socket
import _thread
import time
import machine

# ----------------------
# --- CONFIG Wi-Fi AP ---
# ----------------------
ap_ssid = "City-News"
ap_password = "12345678"  # 8+ caractères

ap = network.WLAN(network.AP_IF)
ap.active(True)
ap.config(essid=ap_ssid, password=ap_password, authmode=3, max_clients=4)
ap_ip = ap.ifconfig()[0]
print("AP actif:", ap_ssid, "IP:", ap_ip)

# ----------------------
# --- TEST INTERNET ---
# ----------------------
def internet_disponible(host="8.8.8.8", port=53, timeout=3):
    try:
        s = socket.socket()
        s.settimeout(timeout)
        s.connect((host, port))
        s.close()
        return True
    except:
        return False

def check_internet_loop():
    while True:
        if internet_disponible():
            print("Internet disponible !")
            # Ici tu peux ajouter du code pour télécharger des fichiers depuis ton serveur Node
        else:
            print("Pas d'internet")
        time.sleep(30)  # vérifie toutes les 30 secondes

_thread.start_new_thread(check_internet_loop, ())

# ----------------------
# --- SERVEUR DNS ---
# ----------------------
def start_dns(ap_ip=ap_ip):
    ip_bytes = bytes(map(int, ap_ip.split(".")))
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("0.0.0.0", 53))
    print(">>> Serveur DNS actif, toutes les requêtes pointent vers", ap_ip)

    while True:
        try:
            data, addr = sock.recvfrom(512)
            transaction_id = data[:2]
            flags = b"\x81\x80"
            qdcount = b"\x00\x01"
            ancount = b"\x00\x01"
            nscount = b"\x00\x00"
            arcount = b"\x00\x00"
            dns_header = transaction_id + flags + qdcount + ancount + nscount + arcount
            dns_question = data[12:]
            dns_answer = b"\xc0\x0c" + b"\x00\x01" + b"\x00\x01" + b"\x00\x00\x00\x3c" + b"\x00\x04" + ip_bytes
            response = dns_header + dns_question + dns_answer
            sock.sendto(response, addr)
        except Exception as e:
            print("Erreur DNS :", e)

_thread.start_new_thread(start_dns, (ap_ip,))

# ----------------------
# --- SERVEUR WEB ---
# ----------------------
def get_mime_type(filename):
    if filename.endswith(".html"): return "text/html"
    if filename.endswith(".css"): return "text/css"
    if filename.endswith(".js"): return "application/javascript"
    if filename.endswith(".json"): return "application/json"
    return "text/plain"

def serve_file(path):
    try:
        with open(path, "rb") as f:
            return f.read()
    except:
        return None

def start_web_server():
    addr = socket.getaddrinfo("0.0.0.0", 80)[0][-1]
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(addr)
    s.listen(1)
    print(">>> Serveur web actif sur http://", ap_ip)

    while True:
        try:
            cl, addr = s.accept()
            req = cl.recv(1024).decode()
            path = req.split(" ")[1] if len(req.split(" ")) > 1 else "/"
            if path == "/": path = "/index.html"
            filepath = path.lstrip("/")
            content = serve_file(filepath)
            if content:
                mime = get_mime_type(filepath)
                cl.send(f"HTTP/1.0 200 OK\r\nContent-Type: {mime}\r\n\r\n".encode())
                cl.send(content)
            else:
                cl.send(b"HTTP/1.0 404 NOT FOUND\r\nContent-Type: text/html\r\n\r\n")
                cl.send(b"<h1>404 - Fichier non trouve</h1>")
            cl.close()
        except Exception as e:
            print("Erreur serveur web:", e)

# ----------------------
# --- LANCEMENT SERVEUR WEB ---
# ----------------------
start_web_server()

import network
import time

# --- Réinitialiser interfaces ---
sta = network.WLAN(network.STA_IF)
ap = network.WLAN(network.AP_IF)
sta.active(False)
ap.active(False)
time.sleep(1)

# --- Activer AP (toujours actif) ---
ap.active(True)
ap.config(essid="City-News", password="password")
print("AP actif:", ap.ifconfig()[0])

# --- Essayer STA (Internet) --- 
# Mettre en veille 2s pour éviter bug
sta.active(True)
sta.connect("TestCo", "testPassword")
for _ in range(10):
    if sta.isconnected():
        break
    time.sleep(1)

if sta.isconnected():
    print("Connecté à Internet :", sta.ifconfig()[0])
else:
    print("Pas d'Internet, AP seul.")

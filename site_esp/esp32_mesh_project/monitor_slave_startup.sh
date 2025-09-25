#!/bin/bash

echo "🔄 Redémarrage et surveillance ESP32 Slave"
echo "Port: /dev/cu.usbmodem5A840187951"
echo ""

# Fonction pour surveiller avec Python
monitor_esp32() {
    python3 -c "
import serial
import time
import signal
import sys

def signal_handler(sig, frame):
    print('\n⏹️  Monitoring arrêté')
    sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)

try:
    ser = serial.Serial('/dev/cu.usbmodem5A840187951', 115200, timeout=1)
    print('🔍 Surveillance ESP32 Slave - DÉMARRAGE COMPLET')
    print('📊 Recherche des logs WiFi AP...')
    print('-' * 60)
    
    wifi_logs = []
    ap_started = False
    server_started = False
    
    for i in range(60):  # 60 secondes max
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        if line:
            print(f'{line}')
            
            # Chercher les logs importants
            if 'wifi' in line.lower() and ('ap' in line.lower() or 'ssid' in line.lower()):
                wifi_logs.append(line)
                print('🌐 >>> LOG WIFI DÉTECTÉ <<<')
                
            if 'esp32_mesh_slave' in line.lower():
                print('📡 >>> SSID TROUVÉ <<<')
                ap_started = True
                
            if 'serveur' in line.lower() and ('web' in line.lower() or 'http' in line.lower()):
                print('🌐 >>> SERVEUR WEB DÉTECTÉ <<<')
                server_started = True
                
            if 'mesh_slave' in line.lower() and 'démarré' in line.lower():
                print('✅ >>> SLAVE DÉMARRÉ <<<')
        
        time.sleep(0.5)
    
    ser.close()
    print('-' * 60)
    print('📋 RÉSUMÉ:')
    print(f'   WiFi AP: {\"✅\" if ap_started else \"❌\"} {\"Démarré\" if ap_started else \"Non détecté\"}')
    print(f'   Serveur Web: {\"✅\" if server_started else \"❌\"} {\"Actif\" if server_started else \"Non détecté\"}')
    print(f'   Logs WiFi trouvés: {len(wifi_logs)}')
    
    if wifi_logs:
        print('\n🔍 Logs WiFi importants:')
        for log in wifi_logs:
            print(f'   📝 {log}')
        
except Exception as e:
    print(f'❌ Erreur: {e}')
"
}

# Instruction pour redémarrer
echo "⚠️  INSTRUCTIONS:"
echo "1. Débranchez et rebranchez l'ESP32 slave"
echo "2. Attendez 2-3 secondes"
echo "3. Appuyez sur ENTRÉE pour commencer la surveillance"
read -p "👆 Prêt ? (Appuyez sur ENTRÉE après reconnexion)"

echo ""
echo "🚀 Démarrage de la surveillance..."
monitor_esp32
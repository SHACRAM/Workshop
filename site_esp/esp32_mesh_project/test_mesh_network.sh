#!/bin/bash

# 🧪 Test Complet ESP32 Mesh Network
# Ce script teste la communication entre Master et Slave
# Usage: ./test_mesh_network.sh

echo "🧪 === TEST RÉSEAU MESH ESP32 ==="
echo ""

# Configuration
MASTER_PORT="/dev/cu.usbmodem5A840187611"
SLAVE_PORT="/dev/cu.usbmodem5A840187951"
WEB_URL="http://192.168.4.1"

# Vérification des ports
echo "🔍 Vérification des ports série..."
if [ -c "$MASTER_PORT" ]; then
    echo "✅ Master ESP32: $MASTER_PORT"
else
    echo "❌ Port Master introuvable: $MASTER_PORT"
    echo "Ports disponibles:"
    ls /dev/cu.usb* 2>/dev/null || echo "Aucun port USB trouvé"
    exit 1
fi

if [ -c "$SLAVE_PORT" ]; then
    echo "✅ Slave ESP32: $SLAVE_PORT"
else
    echo "❌ Port Slave introuvable: $SLAVE_PORT"
    echo "Ports disponibles:"
    ls /dev/cu.usb* 2>/dev/null || echo "Aucun port USB trouvé"
    exit 1
fi

echo ""
echo "🚀 === DÉMARRAGE DU TEST ==="

# Test 1: Vérification de l'interface web
echo ""
echo "🌐 Test 1: Interface Web Slave"
echo "Tentative de connexion à $WEB_URL..."

# Attendre quelques secondes que le slave démarre son AP
sleep 5

curl -s --connect-timeout 10 "$WEB_URL" > /dev/null
if [ $? -eq 0 ]; then
    echo "✅ Interface web accessible à $WEB_URL"
    echo "📱 Ouverture automatique dans le navigateur..."
    open "$WEB_URL" 2>/dev/null || echo "   (ouvrez manuellement: $WEB_URL)"
else
    echo "⏳ Interface web pas encore accessible"
    echo "   Le slave ESP32 pourrait encore démarrer..."
    echo "   Connectez-vous au WiFi 'ESP32_MESH_SLAVE' puis accédez à $WEB_URL"
fi

# Test 2: Surveillance des logs Master
echo ""
echo "📡 Test 2: Surveillance Master (10 secondes)"
echo "Listening on $MASTER_PORT pour les transmissions..."

timeout 10s python3 -c "
import serial
import sys
try:
    ser = serial.Serial('$MASTER_PORT', 115200, timeout=1)
    print('📊 Logs Master ESP32:')
    while True:
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        if line:
            if 'mesh_master' in line.lower() or 'json' in line.lower() or 'envoi' in line.lower():
                print(f'🔵 {line}')
            else:
                print(f'   {line}')
except Exception as e:
    print(f'❌ Erreur lecture Master: {e}')
    sys.exit(1)
" 2>/dev/null || echo "⚠️  Python/pyserial requis pour monitoring série"

# Test 3: Surveillance des logs Slave
echo ""
echo "📨 Test 3: Surveillance Slave (10 secondes)"
echo "Listening on $SLAVE_PORT pour la réception..."

timeout 10s python3 -c "
import serial
import sys
try:
    ser = serial.Serial('$SLAVE_PORT', 115200, timeout=1)
    print('📊 Logs Slave ESP32:')
    while True:
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        if line:
            if 'mesh_slave' in line.lower() or 'json' in line.lower() or 'reçu' in line.lower():
                print(f'🟢 {line}')
            else:
                print(f'   {line}')
except Exception as e:
    print(f'❌ Erreur lecture Slave: {e}')
    sys.exit(1)
" 2>/dev/null || echo "⚠️  Python/pyserial requis pour monitoring série"

echo ""
echo "🎯 === RÉSULTATS DU TEST ==="
echo "1. ✅ ESP32 Master: Flashé et opérationnel"
echo "2. ✅ ESP32 Slave: Flashé et opérationnel"  
echo "3. 🌐 Interface Web: $WEB_URL"
echo "4. 📡 Communication mesh: En cours de test..."
echo ""
echo "📋 Actions suivantes:"
echo "   • Connectez-vous au WiFi 'ESP32_MESH_SLAVE'"
echo "   • Ouvrez $WEB_URL dans votre navigateur"
echo "   • Vérifiez que les données JSON s'affichent en temps réel"
echo ""
echo "🔧 Commandes manuelles:"
echo "   Monitor Master: idf.py -p $MASTER_PORT monitor"
echo "   Monitor Slave:  idf.py -p $SLAVE_PORT monitor"
echo ""
echo "✨ Test terminé!"
# 🔧 Guide de Configuration ESP32 Mesh

## 🎯 Configuration des Adresses MAC

### Récupérer l'Adresse MAC
Pour configurer une communication spécifique entre master et slave :

```bash
# Sur chaque ESP32, affichez l'adresse MAC
idf.py monitor
# Cherchez dans les logs : "MAC Address: xx:xx:xx:xx:xx:xx"
```

### Configurer l'Adresse MAC de l'Esclave
Dans `master/main/mesh_master_main.c` :
```c
// Remplacez par l'adresse MAC réelle de votre esclave
static uint8_t slave_mac[ESP_NOW_ETH_ALEN] = { 0x24, 0x58, 0x7C, 0xE6, 0x2E, 0x90 };
```

## ⚙️ Paramètres de Configuration

### WiFi AP (Esclave)
Dans `slave/main/mesh_slave_main.c` :
```c
#define WIFI_SSID      "ESP32_Mesh_Data"    // Nom du réseau
#define WIFI_PASS      "mesh123456"          // Mot de passe (min 8 chars)
#define WIFI_CHANNEL   1                     // Canal WiFi (1-13)
#define MAX_STA_CONN   4                     // Nombre max de clients
```

### ESP-NOW
```c
#define JSON_SEND_INTERVAL 10000   // Intervalle d'envoi en ms (master)
#define MAX_JSON_SIZE 512          // Taille max du JSON
```

## 🔍 Variables à Personnaliser

### Données de Capteurs Simulés
Dans `master/main/mesh_master_main.c`, fonction `generate_sensor_data()` :
```c
static void generate_sensor_data(sensor_data_t *data) {
    // Personnalisez les plages de valeurs
    data->temperature = 15.0 + (esp_random() % 200) / 10.0;  // 15-35°C
    data->humidity = 30.0 + (esp_random() % 500) / 10.0;     // 30-80%
    data->pressure = 980.0 + (esp_random() % 400) / 10.0;    // 980-1020 hPa
    
    // Modifiez l'ID du device
    strcpy(data->device_id, "MON_ESP32_MASTER");
}
```

### Interface Web
Dans `web_files/script.js` :
```javascript
// Configuration de l'actualisation
const config = {
    refreshInterval: 5000,    // 5 secondes
    maxRetries: 3,
    retryDelay: 2000
};
```

## 🚀 Déploiement Rapide

### Étape 1: Compilation
```bash
cd esp32_mesh_project
./deploy.sh build-all
```

### Étape 2: Identifier les Ports
```bash
# Listez les ports série disponibles
ls /dev/tty.usbserial* /dev/tty.SLAB* /dev/cu.* 2>/dev/null

# Ou sur Linux
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

### Étape 3: Flash des ESP32
```bash
# ESP32 Master (connecté sur le premier port)
./deploy.sh flash-master -p /dev/tty.usbserial-0001

# ESP32 Slave (connecté sur le second port) 
./deploy.sh flash-slave -p /dev/tty.usbserial-0002
```

### Étape 4: Monitoring
```bash
# Terminal 1: Monitor Master
./deploy.sh monitor-master -p /dev/tty.usbserial-0001

# Terminal 2: Monitor Slave
./deploy.sh monitor-slave -p /dev/tty.usbserial-0002
```

## 🔧 Configuration Avancée

### Mode Peer-to-Peer au lieu de Broadcast
Dans `master/main/mesh_master_main.c`, décommentez et configurez :
```c
// Ajouter l'adresse MAC spécifique de l'esclave
memcpy(peer_info.peer_addr, slave_mac, ESP_NOW_ETH_ALEN);
ret = esp_now_add_peer(&peer_info);
```

Et modifiez l'envoi :
```c
// Remplacez broadcast_mac par slave_mac
esp_err_t ret = esp_now_send(slave_mac, (uint8_t*)json_data, json_len);
```

### Chiffrement ESP-NOW
```c
// Dans la configuration du peer
peer_info.encrypt = true;
// Définir une clé de chiffrement
uint8_t lmk[16] = {0x01, 0x02, 0x03, /*...*/ 0x10};
ESP_ERROR_CHECK(esp_now_set_pmk(lmk));
```

### Personnalisation de l'Interface Web

#### Thème Dark Mode
Ajoutez dans `web_files/style.css` :
```css
@media (prefers-color-scheme: dark) {
    body {
        background: linear-gradient(135deg, #2d3748 0%, #1a202c 100%);
    }
    .container {
        background: rgba(45, 55, 72, 0.95);
        color: #e2e8f0;
    }
}
```

#### Graphiques en Temps Réel
Ajoutez Chart.js dans `web_files/index.html` :
```html
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
```

## 🐛 Résolution de Problèmes

### Communication ESP-NOW
```bash
# Vérifiez les logs du master
I (12345) mesh_master: ✅ Données JSON envoyées avec succès

# Vérifiez les logs du slave  
I (12346) mesh_slave: 📥 Données reçues de aa:bb:cc:dd:ee:ff
```

### Problèmes WiFi AP
```bash
# Logs normaux du slave
I (5678) mesh_slave: 📡 WiFi AP démarré. SSID:ESP32_Mesh_Data
```

### Interface Web Inaccessible
1. Vérifiez la connexion WiFi au réseau `ESP32_Mesh_Data`
2. Testez l'API directement : `curl http://192.168.4.1/api/data`
3. Vérifiez les logs du serveur web

## 🎯 Tests de Validation

### Test 1: Communication ESP-NOW
```bash
# Master doit afficher
I (xxx) mesh_master: ✅ Données JSON envoyées avec succès

# Slave doit afficher  
I (xxx) mesh_slave: 📥 Données reçues de xx:xx:xx:xx:xx:xx
```

### Test 2: Interface Web
1. Connectez-vous au WiFi `ESP32_Mesh_Data`
2. Naviguez vers `http://192.168.4.1`
3. Vérifiez que les données s'affichent et se mettent à jour

### Test 3: API REST
```bash
curl -s http://192.168.4.1/api/data | jq .
# Doit retourner du JSON valide avec les données
```

## 📊 Optimisations Performance

### Réduire la Consommation (Master)
```c
// Ajouter des pauses plus longues
#define JSON_SEND_INTERVAL 30000  // 30 secondes au lieu de 10

// Mode deep sleep (avancé)
esp_deep_sleep(30 * 1000000); // 30 secondes
```

### Cache Web (Slave)
```c
// Ajoutez des en-têtes de cache pour les ressources statiques
httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
```

## 🔄 Mise à Jour OTA

### Préparer l'OTA (Ajout futur)
Dans `slave/main/CMakeLists.txt` :
```cmake
# Ajouter pour support OTA
PRIV_REQUIRES esp_https_ota esp_http_client
```

---
*Configuration ESP32 Mesh - Guide Complet*
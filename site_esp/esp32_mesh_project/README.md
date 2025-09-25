# 🌐 ESP32 Mesh Network Project

## 📋 Description du Projet
Ce projet implémente un réseau mesh simple avec ESP-NOW utilisant deux ESP32 S3 :
- **ESP32 Maître** : Génère et envoie des données de capteurs au format JSON
- **ESP32 Esclave** : Reçoit les données JSON et les affiche via une interface web en mode AP

## 🏗️ Architecture

```
ESP32 MAÎTRE                    ESP32 ESCLAVE
┌─────────────────┐            ┌─────────────────┐
│ Génération JSON │            │ Réception JSON  │
│      ↓          │   ESP-NOW  │      ↓          │
│ Envoi ESP-NOW   │ ────────→  │ Stockage données│
└─────────────────┘            │      ↓          │
                               │ Serveur Web AP  │
                               │      ↓          │
                               │ Interface Web   │
                               └─────────────────┘
                                       ↑
                               📱 Utilisateurs WiFi
```

## 📁 Structure du Projet

```
esp32_mesh_project/
├── master/                 # Code ESP32 Maître
│   ├── main/
│   │   ├── mesh_master_main.c
│   │   └── CMakeLists.txt
│   └── CMakeLists.txt
├── slave/                  # Code ESP32 Esclave  
│   ├── main/
│   │   ├── mesh_slave_main.c
│   │   └── CMakeLists.txt
│   └── CMakeLists.txt
└── web_files/             # Interface Web
    ├── index.html
    ├── style.css
    └── script.js
```

## 🚀 Installation et Compilation

### Prérequis
- ESP-IDF v4.4 ou plus récent
- Deux ESP32-S3
- Environnement ESP-IDF configuré

### Compilation

#### ESP32 Maître
```bash
cd master
source $HOME/esp/esp-idf/export.sh
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

#### ESP32 Esclave  
```bash
cd slave
source $HOME/esp/esp-idf/export.sh
idf.py build
idf.py -p /dev/ttyUSB1 flash monitor
```

## ⚙️ Configuration

### Paramètres WiFi AP (Esclave)
- **SSID**: `ESP32_Mesh_Data`
- **Mot de passe**: `mesh123456`
- **Canal**: 1
- **IP**: 192.168.4.1

### Paramètres ESP-NOW
- **Canal**: 1
- **Mode**: Broadcast (peut être configuré pour une adresse spécifique)

## 📊 Format des Données JSON

Le maître envoie des données au format suivant :
```json
{
  "temperature": 23.5,
  "humidity": 65.2,
  "pressure": 1013.2,
  "timestamp": 1695456789,
  "device_id": "ESP32_MASTER_01",
  "signal_strength": -45,
  "status": "active"
}
```

## 🌐 Interface Web

### Accès à l'Interface
1. Connectez-vous au WiFi `ESP32_Mesh_Data` (mot de passe: `mesh123456`)
2. Ouvrez votre navigateur sur `http://192.168.4.1`

### Fonctionnalités
- ✅ Affichage en temps réel des données de capteurs
- ✅ Visualisation du JSON brut
- ✅ Actualisation manuelle et automatique
- ✅ Indicateur de statut de connexion
- ✅ Interface responsive

### API REST
- `GET /api/data` - Récupérer les dernières données JSON
- `HEAD /api/ping` - Vérifier la connectivité

## 🔧 Personnalisation

### Modifier l'Interface Web
Les fichiers sont dans `web_files/` :
- **HTML**: Structure de la page
- **CSS**: Style et responsive design
- **JavaScript**: Logique d'actualisation et API calls

Après modification, recompilez l'esclave pour intégrer les changements.

### Ajouter de Nouveaux Capteurs
Dans `mesh_master_main.c`, modifiez la fonction `generate_sensor_data()` :
```c
static void generate_sensor_data(sensor_data_t *data) {
    // Ajouter nouveaux capteurs ici
    data->new_sensor = read_new_sensor();
}
```

### Changer les Paramètres Réseau
Dans `mesh_slave_main.c` :
```c
#define WIFI_SSID      "VotreNouveauSSID"
#define WIFI_PASS      "VotreNouveauMotDePasse"
#define WIFI_CHANNEL   6
```

## 🐛 Dépannage

### Problèmes de Communication ESP-NOW
1. Vérifiez que les deux ESP32 sont sur le même canal
2. Contrôlez les logs série pour voir les messages d'envoi/réception
3. Testez d'abord en mode broadcast avant d'utiliser des MAC spécifiques

### Interface Web Inaccessible
1. Vérifiez que l'AP est actif dans les logs
2. Assurez-vous d'être connecté au bon réseau WiFi
3. Essayez de redémarrer l'ESP32 esclave

### Données non Actualisées
1. Vérifiez les logs de réception ESP-NOW sur l'esclave
2. Testez l'API directement : `http://192.168.4.1/api/data`
3. Vérifiez le format JSON dans les logs du maître

## 📈 Améliorations Possibles

### Fonctionnalités Avancées
- 🔄 Confirmation de réception (ACK)
- 🔐 Chiffrement des données ESP-NOW
- 📊 Historique des données avec stockage local
- ⚠️ Alertes et seuils configurables
- 🌐 Mode mesh multi-esclaves

### Optimisations
- 🔋 Gestion de l'énergie (deep sleep)
- 📱 Application mobile companion
- ☁️ Intégration cloud (MQTT, HTTP)
- 🎯 Interface de configuration web

## 📝 Logs Utiles

### Maître (Envoi)
```
I (12345) mesh_master: 📡 Envoi données JSON (156 bytes):
I (12345) mesh_master: 🌡️ Température: 23.5°C
I (12345) mesh_master: ✅ Données JSON envoyées avec succès
```

### Esclave (Réception)
```
I (12346) mesh_slave: 📥 Données reçues de aa:bb:cc:dd:ee:ff, 156 bytes
I (12346) mesh_slave: 🌡️ Température: 23.5°C
I (12346) mesh_slave: ✅ Données JSON mises à jour
```

## 🤝 Support

Pour toute question ou problème :
1. Vérifiez les logs série des deux ESP32
2. Consultez la documentation ESP-IDF
3. Testez les composants individuellement

---
*Projet ESP32 Mesh Network - Version 1.0*
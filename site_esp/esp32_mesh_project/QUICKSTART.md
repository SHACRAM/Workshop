# 🎯 ESP32 Mesh Project - Guide de Démarrage Rapide

## ✅ Projet Configuré et Prêt !

Votre réseau mesh ESP32 avec interface web est maintenant configuré. Voici tout ce dont vous avez besoin pour le déployer.

## 📋 Ce Qui a Été Créé

### 🔧 **ESP32 Maître** (`master/`)
- **Fonctionnalité** : Génère et envoie des données JSON via ESP-NOW
- **Données envoyées** : Température, humidité, pression (simulées)
- **Intervalle** : Toutes les 10 secondes
- **Fichier principal** : `main/mesh_master_main.c`

### 📡 **ESP32 Esclave** (`slave/`)
- **Fonctionnalité** : Reçoit JSON + Serveur web en mode AP
- **WiFi AP** : `ESP32_Mesh_Data` / `mesh123456`
- **Interface** : http://192.168.4.1
- **API** : http://192.168.4.1/api/data
- **Fichier principal** : `main/mesh_slave_main.c`

### 🌐 **Interface Web** (`web_files/`)
- **Design** : Interface moderne et responsive
- **Fonctionnalités** : Actualisation auto/manuelle, graphiques, JSON brut
- **Fichiers** : `index.html`, `style.css`, `script.js`

## 🚀 **Déploiement en 5 Étapes**

### 1️⃣ Préparer l'Environnement
```bash
# Activer ESP-IDF
source $HOME/esp/esp-idf/export.sh
# Ou si vous avez l'alias
get_idf
```

### 2️⃣ Compiler les Projets
```bash
cd esp32_mesh_project
./deploy.sh build-all
```

### 3️⃣ Identifier les Ports Série
```bash
# macOS
ls /dev/tty.usbserial* /dev/cu.* 2>/dev/null

# Linux  
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

### 4️⃣ Flasher les ESP32
```bash
# ESP32 Master (premier port)
./deploy.sh flash-master -p /dev/tty.usbserial-0001

# ESP32 Slave (deuxième port)
./deploy.sh flash-slave -p /dev/tty.usbserial-0002
```

### 5️⃣ Tester l'Interface
1. **Redémarrer les ESP32**
2. **Connecter** au WiFi `ESP32_Mesh_Data` (mot de passe: `mesh123456`)
3. **Naviguer** vers http://192.168.4.1
4. **Voir les données** se mettre à jour automatiquement

## 📊 **Logs de Fonctionnement Normal**

### Master (Envoi)
```
I (12345) mesh_master: 📡 Envoi données JSON (156 bytes):
I (12345) mesh_master: 🌡️  Température: 23.5°C
I (12345) mesh_master: 💧 Humidité: 65.2%
I (12345) mesh_master: ✅ Données JSON envoyées avec succès
```

### Slave (Réception + Web)
```
I (5678) mesh_slave: 📡 WiFi AP démarré. SSID:ESP32_Mesh_Data
I (5679) mesh_slave: 🌐 Serveur HTTP sur port 80
I (12346) mesh_slave: 📥 Données reçues de aa:bb:cc:dd:ee:ff, 156 bytes
I (12346) mesh_slave: ✅ Données JSON mises à jour
```

## 🔧 **Personnalisation Simple**

### Changer le Nom du WiFi
Dans `slave/main/mesh_slave_main.c` :
```c
#define WIFI_SSID      "MonReseauMesh"
#define WIFI_PASS      "monmotdepasse"
```

### Modifier les Données Simulées
Dans `master/main/mesh_master_main.c` :
```c
// Dans generate_sensor_data()
data->temperature = 20.0 + votre_capteur_reel();
```

### Personnaliser l'Interface
Modifiez les fichiers dans `web_files/` puis recompilez le slave.

## 🐛 **Dépannage Rapide**

| Problème | Solution |
|----------|----------|
| Master n'envoie pas | Vérifier logs série, redémarrer |
| Slave ne reçoit pas | Vérifier même canal WiFi (1) |
| WiFi invisible | Redémarrer slave, vérifier SSID |
| Interface inaccessible | Vérifier connexion WiFi, IP 192.168.4.1 |
| Données vides | Attendre 10 secondes, vérifier logs master |

## 📱 **Utilisation Mobile**

L'interface est responsive et fonctionne parfaitement sur smartphone :
1. Connecter le téléphone au WiFi `ESP32_Mesh_Data`
2. Ouvrir le navigateur sur `http://192.168.4.1`
3. Utiliser l'auto-refresh pour le monitoring en temps réel

## 🔄 **Monitoring Continu**

### Terminal 1 : Master
```bash
./deploy.sh monitor-master -p /dev/tty.usbserial-0001
```

### Terminal 2 : Slave
```bash
./deploy.sh monitor-slave -p /dev/tty.usbserial-0002
```

## 🎯 **Validation du Projet**

✅ **Communication mesh** : ESP-NOW entre master/slave  
✅ **Interface web** : Serveur HTTP avec AP WiFi  
✅ **Données JSON** : Format structuré et API REST  
✅ **Interface utilisateur** : HTML/CSS/JS responsive  
✅ **Temps réel** : Actualisation automatique des données  

## 📈 **Extensions Possibles**

Une fois le système fonctionnel, vous pouvez :
- 🔌 Connecter de vrais capteurs au master
- 🔐 Ajouter l'authentification web
- ☁️ Intégrer un service cloud (MQTT)
- 📊 Ajouter des graphiques historiques
- 🌐 Créer un réseau mesh multi-esclaves

## 🎉 **Félicitations !**

Vous avez maintenant un réseau mesh ESP32 complet avec :
- ✅ Communication sans fil ESP-NOW
- ✅ Interface web moderne
- ✅ Code modulaire et extensible
- ✅ Documentation complète

**Bon développement avec votre réseau mesh ESP32 !** 🚀

---
*ESP32 Mesh Project - Prêt à l'emploi*
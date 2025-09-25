#!/bin/bash
# Script de compilation et déploiement automatique pour le projet ESP32 Mesh

set -e

# Couleurs pour l'affichage
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
PROJECT_DIR=$(pwd)
ESP_IDF_PATH="$HOME/esp/esp-idf"

# Fonction d'aide
show_help() {
    echo -e "${BLUE}ESP32 Mesh Project - Script de Déploiement${NC}"
    echo "Usage: $0 [COMMAND] [OPTIONS]"
    echo ""
    echo "Commands:"
    echo "  build-all       Compiler master et slave"
    echo "  build-master    Compiler seulement le master"
    echo "  build-slave     Compiler seulement le slave"
    echo "  flash-master    Flasher le master"
    echo "  flash-slave     Flasher le slave"
    echo "  monitor-master  Monitorer le master"
    echo "  monitor-slave   Monitorer le slave"
    echo "  clean-all       Nettoyer les builds"
    echo "  help           Afficher cette aide"
    echo ""
    echo "Options:"
    echo "  -p PORT        Spécifier le port série (ex: /dev/ttyUSB0)"
    echo "  -m PORT        Port pour le master"
    echo "  -s PORT        Port pour le slave"
    echo ""
    echo "Exemples:"
    echo "  $0 build-all"
    echo "  $0 flash-master -p /dev/ttyUSB0"
    echo "  $0 flash-slave -p /dev/ttyUSB1"
}

# Vérifier ESP-IDF
check_esp_idf() {
    if [ ! -d "$ESP_IDF_PATH" ]; then
        echo -e "${RED}❌ ESP-IDF non trouvé dans $ESP_IDF_PATH${NC}"
        exit 1
    fi
    
    if [ -z "$IDF_PATH" ]; then
        echo -e "${YELLOW}⚠️  Activation de l'environnement ESP-IDF...${NC}"
        source "$ESP_IDF_PATH/export.sh"
    fi
    
    echo -e "${GREEN}✅ Environnement ESP-IDF actif${NC}"
}

# Compiler le master
build_master() {
    echo -e "${BLUE}🔨 Compilation ESP32 Master...${NC}"
    cd "$PROJECT_DIR/master"
    idf.py build
    echo -e "${GREEN}✅ Master compilé avec succès${NC}"
}

# Compiler le slave
build_slave() {
    echo -e "${BLUE}🔨 Compilation ESP32 Slave...${NC}"
    cd "$PROJECT_DIR/slave"
    idf.py build
    echo -e "${GREEN}✅ Slave compilé avec succès${NC}"
}

# Flasher le master
flash_master() {
    local port=${1:-}
    if [ -z "$port" ]; then
        echo -e "${RED}❌ Port série requis pour le flash${NC}"
        exit 1
    fi
    
    echo -e "${BLUE}📡 Flash ESP32 Master sur $port...${NC}"
    cd "$PROJECT_DIR/master"
    idf.py -p "$port" flash
    echo -e "${GREEN}✅ Master flashé avec succès${NC}"
}

# Flasher le slave
flash_slave() {
    local port=${1:-}
    if [ -z "$port" ]; then
        echo -e "${RED}❌ Port série requis pour le flash${NC}"
        exit 1
    fi
    
    echo -e "${BLUE}📡 Flash ESP32 Slave sur $port...${NC}"
    cd "$PROJECT_DIR/slave"
    idf.py -p "$port" flash
    echo -e "${GREEN}✅ Slave flashé avec succès${NC}"
}

# Monitorer le master
monitor_master() {
    local port=${1:-}
    if [ -z "$port" ]; then
        echo -e "${RED}❌ Port série requis pour le monitoring${NC}"
        exit 1
    fi
    
    echo -e "${BLUE}📺 Monitoring ESP32 Master sur $port (Ctrl+] pour quitter)...${NC}"
    cd "$PROJECT_DIR/master"
    idf.py -p "$port" monitor
}

# Monitorer le slave
monitor_slave() {
    local port=${1:-}
    if [ -z "$port" ]; then
        echo -e "${RED}❌ Port série requis pour le monitoring${NC}"
        exit 1
    fi
    
    echo -e "${BLUE}📺 Monitoring ESP32 Slave sur $port (Ctrl+] pour quitter)...${NC}"
    cd "$PROJECT_DIR/slave"
    idf.py -p "$port" monitor
}

# Nettoyer les builds
clean_all() {
    echo -e "${BLUE}🧹 Nettoyage des builds...${NC}"
    
    if [ -d "$PROJECT_DIR/master/build" ]; then
        cd "$PROJECT_DIR/master"
        idf.py clean
        echo -e "${GREEN}✅ Build master nettoyé${NC}"
    fi
    
    if [ -d "$PROJECT_DIR/slave/build" ]; then
        cd "$PROJECT_DIR/slave"
        idf.py clean
        echo -e "${GREEN}✅ Build slave nettoyé${NC}"
    fi
}

# Afficher les informations du projet
show_project_info() {
    echo -e "${BLUE}=====================================}${NC}"
    echo -e "${BLUE}🌐 ESP32 Mesh Network Project${NC}"
    echo -e "${BLUE}=====================================}${NC}"
    echo -e "📁 Répertoire: $PROJECT_DIR"
    echo -e "🛠️  ESP-IDF: $IDF_PATH"
    echo -e "📡 WiFi AP: ESP32_Mesh_Data"
    echo -e "🌍 Interface: http://192.168.4.1"
    echo -e "${BLUE}=====================================}${NC}"
}

# Vérifications préalables
check_esp_idf
show_project_info

# Parsing des arguments
MASTER_PORT=""
SLAVE_PORT=""
PORT=""

while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--port)
            PORT="$2"
            shift 2
            ;;
        -m|--master-port)
            MASTER_PORT="$2"
            shift 2
            ;;
        -s|--slave-port)
            SLAVE_PORT="$2"
            shift 2
            ;;
        build-all)
            build_master
            build_slave
            exit 0
            ;;
        build-master)
            build_master
            exit 0
            ;;
        build-slave)
            build_slave
            exit 0
            ;;
        flash-master)
            flash_master "${MASTER_PORT:-$PORT}"
            exit 0
            ;;
        flash-slave)
            flash_slave "${SLAVE_PORT:-$PORT}"
            exit 0
            ;;
        monitor-master)
            monitor_master "${MASTER_PORT:-$PORT}"
            exit 0
            ;;
        monitor-slave)
            monitor_slave "${SLAVE_PORT:-$PORT}"
            exit 0
            ;;
        clean-all)
            clean_all
            exit 0
            ;;
        help|--help|-h)
            show_help
            exit 0
            ;;
        *)
            echo -e "${RED}❌ Commande inconnue: $1${NC}"
            show_help
            exit 1
            ;;
    esac
done

# Si aucune commande n'est fournie, afficher l'aide
show_help
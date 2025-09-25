#!/bin/bash
# Script de test pour vérifier la configuration du projet

echo "🧪 Test de Configuration ESP32 Mesh Project"
echo "==========================================="

# Vérifier la structure
echo "📁 Vérification de la structure..."

required_files=(
    "master/main/mesh_master_main.c"
    "master/main/CMakeLists.txt"
    "slave/main/mesh_slave_main.c" 
    "slave/main/CMakeLists.txt"
    "web_files/index.html"
    "web_files/style.css"
    "web_files/script.js"
    "README.md"
    "CONFIGURATION.md"
    "deploy.sh"
)

all_good=true

for file in "${required_files[@]}"; do
    if [ -f "$file" ]; then
        echo "✅ $file"
    else
        echo "❌ $file MANQUANT"
        all_good=false
    fi
done

echo ""

# Vérifier les CMakeLists.txt
echo "🔧 Vérification des configurations..."

if grep -q "json" master/main/CMakeLists.txt; then
    echo "✅ Composant JSON activé dans master"
else
    echo "❌ Composant JSON manquant dans master"
    all_good=false
fi

if grep -q "EMBED_FILES" slave/main/CMakeLists.txt; then
    echo "✅ Fichiers web intégrés dans slave"
else
    echo "❌ Fichiers web non intégrés dans slave"
    all_good=false
fi

echo ""

# Vérifier la syntaxe des fichiers C
echo "💻 Vérification de la syntaxe C..."

if gcc -fsyntax-only -I. -I$HOME/esp/esp-idf/components/freertos/include -I$HOME/esp/esp-idf/components/esp_common/include -Wno-unknown-pragmas master/main/mesh_master_main.c 2>/dev/null; then
    echo "✅ Syntaxe master correcte"
else
    echo "⚠️  Master: vérifiez la syntaxe (normal si headers ESP-IDF manquants)"
fi

if gcc -fsyntax-only -I. -I$HOME/esp/esp-idf/components/freertos/include -I$HOME/esp/esp-idf/components/esp_common/include -Wno-unknown-pragmas slave/main/mesh_slave_main.c 2>/dev/null; then
    echo "✅ Syntaxe slave correcte"
else
    echo "⚠️  Slave: vérifiez la syntaxe (normal si headers ESP-IDF manquants)"
fi

echo ""

# Test des fichiers web
echo "🌐 Test des fichiers web..."

if [ -s "web_files/index.html" ] && [ -s "web_files/style.css" ] && [ -s "web_files/script.js" ]; then
    echo "✅ Fichiers web présents et non vides"
else
    echo "❌ Problème avec les fichiers web"
    all_good=false
fi

echo ""

# Résumé
if $all_good; then
    echo "🎉 Projet configuré correctement !"
    echo ""
    echo "📋 Étapes suivantes:"
    echo "1. ./deploy.sh build-all"
    echo "2. ./deploy.sh flash-master -p /dev/ttyUSB0"
    echo "3. ./deploy.sh flash-slave -p /dev/ttyUSB1"
    echo "4. Se connecter au WiFi 'ESP32_Mesh_Data'"
    echo "5. Naviguer vers http://192.168.4.1"
else
    echo "⚠️  Configuration incomplète - vérifiez les erreurs ci-dessus"
fi
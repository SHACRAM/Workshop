#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "cJSON.h"

#define TAG "MESH_SLAVE"
#define WIFI_SSID "City-news"
#define WIFI_PASS "password123"
#define WIFI_CHANNEL 9  // Même canal que le hotspot TestCo
#define MAX_MESSAGE_COUNT 50
#define FRAGMENT_SIZE 256
#define MAX_RECONSTRUCTED_SIZE 2048
#define FRAGMENT_TIMEOUT_MS 10000  // 10 secondes timeout

// Structure de fragmentation ESP-NOW (identique au master)
typedef struct {
    uint32_t message_id;
    uint8_t fragment_num;
    uint8_t total_fragments;
    uint16_t data_len;
    uint8_t data[FRAGMENT_SIZE];
} __attribute__((packed)) fragment_packet_t;

// Structure pour un message reçu complet
typedef struct {
    int id;
    char title[128];
    char content[256];
    char timestamp[32];
} received_message_t;

// Structure pour gérer les fragments en cours de reconstruction
typedef struct {
    uint32_t message_id;
    uint8_t total_fragments;
    uint8_t received_fragments;
    char* data_buffer;
    int total_size;
    TickType_t last_update;
    bool fragments_received[32]; // Max 32 fragments supportés
} message_reconstruction_t;

// Variables globales
static received_message_t received_messages[MAX_MESSAGE_COUNT];
static int message_count = 0;
static uint8_t master_mac[ESP_NOW_ETH_ALEN] = {0xb4, 0x3a, 0x45, 0xa1, 0x1f, 0x38}; // MAC du master (ESP32 #1)
static httpd_handle_t server = NULL;
static message_reconstruction_t pending_messages[5]; // Max 5 messages en reconstruction simultanée
static int pending_message_count = 0;

// Déclaration des contenus embeddes
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t login_html_start[] asm("_binary_login_html_start");
extern const uint8_t login_html_end[] asm("_binary_login_html_end");
extern const uint8_t news_html_start[] asm("_binary_news_html_start");
extern const uint8_t news_html_end[] asm("_binary_news_html_end");
extern const uint8_t main_css_start[] asm("_binary_main_css_start");
extern const uint8_t main_css_end[] asm("_binary_main_css_end");
extern const uint8_t hidden_css_start[] asm("_binary_hidden_css_start");
extern const uint8_t hidden_css_end[] asm("_binary_hidden_css_end");
extern const uint8_t login_css_start[] asm("_binary_login_css_start");
extern const uint8_t login_css_end[] asm("_binary_login_css_end");
extern const uint8_t script_js_start[] asm("_binary_script_js_start");
extern const uint8_t script_js_end[] asm("_binary_script_js_end");
extern const uint8_t dembele_png_start[] asm("_binary_dembele_png_start");
extern const uint8_t dembele_png_end[] asm("_binary_dembele_png_end");
extern const uint8_t favicon_ico_start[] asm("_binary_favicon_ico_start");
extern const uint8_t favicon_ico_end[] asm("_binary_favicon_ico_end");
extern const uint8_t logo_png_start[] asm("_binary_logo_png_start");
extern const uint8_t logo_png_end[] asm("_binary_logo_png_end");

// Fonction pour vérifier si l'adresse MAC est celle du master autorisé
static bool is_mac_authorized(const uint8_t *mac_addr) {
    return memcmp(mac_addr, master_mac, ESP_NOW_ETH_ALEN) == 0;
}

// Fonction pour traiter un message JSON reconstruit complet
static void process_complete_json_message(const char* json_string) {
    ESP_LOGI(TAG, "📄 Traitement JSON reconstruit: %.100s", json_string);
    
    cJSON *json = cJSON_Parse(json_string);
    if (json == NULL) {
        ESP_LOGE(TAG, "❌ Erreur parsing JSON reconstruit");
        return;
    }
    
    // Extraire les données du nouveau format complet
    cJSON *id_item = cJSON_GetObjectItem(json, "id");
    cJSON *title_item = cJSON_GetObjectItem(json, "title");
    cJSON *content_item = cJSON_GetObjectItem(json, "content");
    cJSON *created_at_item = cJSON_GetObjectItem(json, "created_at");
    
    // Fallback pour l'ancien format court
    if (!title_item) title_item = cJSON_GetObjectItem(json, "t");
    if (!content_item) content_item = cJSON_GetObjectItem(json, "c");
    
    if (id_item && title_item && content_item) {
        // Ajouter le message au tableau (rotation si plein)
        int index = message_count % MAX_MESSAGE_COUNT;
        received_messages[index].id = id_item->valueint;
        
        strncpy(received_messages[index].title, 
                cJSON_IsString(title_item) ? title_item->valuestring : "Sans titre", 
                sizeof(received_messages[index].title) - 1);
        received_messages[index].title[sizeof(received_messages[index].title) - 1] = '\0';
        
        strncpy(received_messages[index].content, 
                cJSON_IsString(content_item) ? content_item->valuestring : "Sans contenu", 
                sizeof(received_messages[index].content) - 1);
        received_messages[index].content[sizeof(received_messages[index].content) - 1] = '\0';
        
        strncpy(received_messages[index].timestamp, 
                cJSON_IsString(created_at_item) ? created_at_item->valuestring : "Inconnue", 
                sizeof(received_messages[index].timestamp) - 1);
        received_messages[index].timestamp[sizeof(received_messages[index].timestamp) - 1] = '\0';
        
        message_count++;
        
        ESP_LOGI(TAG, "✅ Message ajouté - ID: %d, Titre: '%.30s', Total: %d", 
                received_messages[index].id, received_messages[index].title, message_count);
    } else {
        ESP_LOGW(TAG, "⚠️ JSON incomplet - champs manquants");
    }
    
    cJSON_Delete(json);
}

// Fonction pour nettoyer les reconstructions expirées
static void cleanup_expired_reconstructions(void) {
    TickType_t current_time = xTaskGetTickCount();
    
    for (int i = 0; i < pending_message_count; i++) {
        if (current_time - pending_messages[i].last_update > pdMS_TO_TICKS(FRAGMENT_TIMEOUT_MS)) {
            ESP_LOGW(TAG, "🧹 Nettoyage reconstruction expirée ID %lu", pending_messages[i].message_id);
            
            if (pending_messages[i].data_buffer) {
                free(pending_messages[i].data_buffer);
            }
            
            // Déplacer le dernier élément à la place de celui supprimé
            if (i < pending_message_count - 1) {
                memcpy(&pending_messages[i], &pending_messages[pending_message_count - 1], 
                       sizeof(message_reconstruction_t));
            }
            pending_message_count--;
            i--; // Retraiter le même index
        }
    }
}

// Fonction pour trouver ou créer une reconstruction de message
static message_reconstruction_t* find_or_create_reconstruction(uint32_t message_id, uint8_t total_fragments) {
    // D'abord, chercher si le message existe déjà
    for (int i = 0; i < pending_message_count; i++) {
        if (pending_messages[i].message_id == message_id) {
            return &pending_messages[i];
        }
    }
    
    // Si pas trouvé et qu'on a de la place, créer un nouveau
    if (pending_message_count < 5) {
        message_reconstruction_t* msg = &pending_messages[pending_message_count];
        pending_message_count++;
        
        // Initialiser la structure
        msg->message_id = message_id;
        msg->total_fragments = total_fragments;
        msg->received_fragments = 0;
        msg->total_size = 0;
        msg->last_update = xTaskGetTickCount();
        msg->data_buffer = malloc(MAX_RECONSTRUCTED_SIZE);
        memset(msg->fragments_received, false, sizeof(msg->fragments_received));
        
        if (msg->data_buffer == NULL) {
            ESP_LOGE(TAG, "❌ Erreur allocation mémoire pour reconstruction");
            pending_message_count--;
            return NULL;
        }
        
        ESP_LOGI(TAG, "🔄 Nouvelle reconstruction créée pour message ID %lu (%d fragments)", 
                message_id, total_fragments);
        return msg;
    }
    
    ESP_LOGW(TAG, "⚠️ Trop de messages en cours de reconstruction, ignoré");
    return NULL;
}

// Callback ESP-NOW amélioré pour la fragmentation
static void esp_now_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    ESP_LOGI(TAG, "Fragment reçu de: %02x:%02x:%02x:%02x:%02x:%02x, longueur: %d", 
            recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2], 
            recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5], len);
    
    // Vérifier si l'adresse MAC est autorisée
    if (!is_mac_authorized(recv_info->src_addr)) {
        ESP_LOGW(TAG, "Fragment ignoré - MAC non autorisée");
        return;
    }
    
    // Vérifier que c'est bien un paquet fragmenté
    if (len != sizeof(fragment_packet_t)) {
        ESP_LOGW(TAG, "Taille de paquet incorrecte: %d, attendu: %d", len, sizeof(fragment_packet_t));
        return;
    }
    
    // Nettoyer les reconstructions expirées
    cleanup_expired_reconstructions();
    
    // Convertir les données en structure fragment
    fragment_packet_t *fragment = (fragment_packet_t*)data;
    
    ESP_LOGI(TAG, "🧩 Fragment - ID: %lu, Fragment: %d/%d, Longueur: %d", 
            fragment->message_id, fragment->fragment_num, fragment->total_fragments, fragment->data_len);
    
    // Cas spécial : fragment unique (rétrocompatibilité)
    if (fragment->fragment_num == 1 && fragment->total_fragments == 1) {
        ESP_LOGI(TAG, "📦 Fragment unique détecté");
        
        char *json_string = malloc(fragment->data_len + 1);
        if (json_string == NULL) {
            ESP_LOGE(TAG, "❌ Erreur allocation mémoire fragment unique");
            return;
        }
        
        memcpy(json_string, fragment->data, fragment->data_len);
        json_string[fragment->data_len] = '\0';
        
        process_complete_json_message(json_string);
        free(json_string);
        return;
    }
    
    // Gestion des fragments multiples
    message_reconstruction_t* msg = find_or_create_reconstruction(fragment->message_id, fragment->total_fragments);
    if (msg == NULL) {
        ESP_LOGE(TAG, "❌ Impossible de créer/trouver reconstruction pour ID %lu", fragment->message_id);
        return;
    }
    
    // Vérifier si ce fragment a déjà été reçu
    if (fragment->fragment_num < 1 || fragment->fragment_num > fragment->total_fragments) {
        ESP_LOGW(TAG, "⚠️ Numéro de fragment invalide: %d", fragment->fragment_num);
        return;
    }
    
    int frag_index = fragment->fragment_num - 1; // Index 0-based
    if (msg->fragments_received[frag_index]) {
        ESP_LOGW(TAG, "⚠️ Fragment %d déjà reçu pour message ID %lu", fragment->fragment_num, fragment->message_id);
        return;
    }
    
    // Vérifier qu'on ne dépasse pas la taille du buffer
    if (msg->total_size + fragment->data_len >= MAX_RECONSTRUCTED_SIZE) {
        ESP_LOGE(TAG, "❌ Message trop grand pour reconstruction (total=%d, fragment=%d)", msg->total_size, fragment->data_len);
        return;
    }
    
    // Copier les données du fragment de manière séquentielle
    memcpy(msg->data_buffer + msg->total_size, fragment->data, fragment->data_len);
    msg->total_size += fragment->data_len;
    msg->fragments_received[frag_index] = true;
    msg->received_fragments++;
    msg->last_update = xTaskGetTickCount();
    
    ESP_LOGI(TAG, "✅ Fragment %d/%d ajouté (total: %d bytes, reçus: %d/%d)", 
            fragment->fragment_num, fragment->total_fragments, 
            msg->total_size, msg->received_fragments, msg->total_fragments);
    
    // Vérifier si le message est complet
    if (msg->received_fragments == msg->total_fragments) {
        ESP_LOGI(TAG, "🎯 Message complet reconstruit ! ID: %lu (%d bytes)", 
                fragment->message_id, msg->total_size);
        
        // Null-terminer le JSON
        msg->data_buffer[msg->total_size] = '\0';
        
        // Traiter le message complet
        process_complete_json_message(msg->data_buffer);
        
        // Nettoyer cette reconstruction
        free(msg->data_buffer);
        
        // Supprimer de la liste des reconstructions en cours
        for (int i = 0; i < pending_message_count; i++) {
            if (&pending_messages[i] == msg) {
                if (i < pending_message_count - 1) {
                    memcpy(&pending_messages[i], &pending_messages[pending_message_count - 1], 
                           sizeof(message_reconstruction_t));
                }
                pending_message_count--;
                break;
            }
        }
        
        ESP_LOGI(TAG, "🧹 Reconstruction nettoyée");
    }
}

// Handlers HTTP (version simplifiée)
static esp_err_t index_handler(httpd_req_t *req) {
    size_t content_length = index_html_end - index_html_start;
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char*)index_html_start, content_length);
    return ESP_OK;
}

static esp_err_t api_news_get_handler(httpd_req_t *req) {
    cJSON *json_array = cJSON_CreateArray();
    if (json_array == NULL) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "Erreur création JSON", -1);
        return ESP_FAIL;
    }
    
    int count = (message_count < MAX_MESSAGE_COUNT) ? message_count : MAX_MESSAGE_COUNT;
    ESP_LOGI(TAG, "📡 API /api/news - Envoi de %d messages", count);
    
    if (count == 0) {
        cJSON *no_data_msg = cJSON_CreateObject();
        cJSON_AddStringToObject(no_data_msg, "status", "waiting");
        cJSON_AddStringToObject(no_data_msg, "message", "En attente - Aucune donnée reçue du master");
        cJSON_AddItemToArray(json_array, no_data_msg);
    } else {
        for (int i = 0; i < count; i++) {
            int index = (message_count > MAX_MESSAGE_COUNT) ? 
                       ((message_count - MAX_MESSAGE_COUNT + i) % MAX_MESSAGE_COUNT) : i;
            
            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", received_messages[index].id);
            cJSON_AddStringToObject(item, "title", received_messages[index].title);
            cJSON_AddStringToObject(item, "content", received_messages[index].content);
            cJSON_AddStringToObject(item, "created_at", received_messages[index].timestamp);
            
            cJSON_AddItemToArray(json_array, item);
        }
    }
    
    char *json_string = cJSON_Print(json_array);
    if (json_string == NULL) {
        cJSON_Delete(json_array);
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "Erreur génération JSON", -1);
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, json_string, strlen(json_string));
    
    free(json_string);
    cJSON_Delete(json_array);
    return ESP_OK;
}

// Autres handlers simplifiés...
static esp_err_t login_handler(httpd_req_t *req) {
    size_t content_length = login_html_end - login_html_start;
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char*)login_html_start, content_length);
    return ESP_OK;
}

static esp_err_t news_handler(httpd_req_t *req) {
    size_t content_length = news_html_end - news_html_start;
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char*)news_html_start, content_length);
    return ESP_OK;
}

static esp_err_t main_css_handler(httpd_req_t *req) {
    size_t content_length = main_css_end - main_css_start;
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, (const char*)main_css_start, content_length);
    return ESP_OK;
}

static esp_err_t script_js_handler(httpd_req_t *req) {
    size_t content_length = script_js_end - script_js_start;
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char*)script_js_start, content_length);
    return ESP_OK;
}

// Handlers pour les assets
static esp_err_t favicon_handler(httpd_req_t *req) {
    size_t content_length = favicon_ico_end - favicon_ico_start;
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char*)favicon_ico_start, content_length);
    return ESP_OK;
}

static esp_err_t logo_handler(httpd_req_t *req) {
    size_t content_length = logo_png_end - logo_png_start;
    httpd_resp_set_type(req, "image/png");
    httpd_resp_send(req, (const char*)logo_png_start, content_length);
    return ESP_OK;
}

static esp_err_t dembele_handler(httpd_req_t *req) {
    size_t content_length = dembele_png_end - dembele_png_start;
    httpd_resp_set_type(req, "image/png");
    httpd_resp_send(req, (const char*)dembele_png_start, content_length);
    return ESP_OK;
}

static esp_err_t login_css_handler(httpd_req_t *req) {
    size_t content_length = login_css_end - login_css_start;
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, (const char*)login_css_start, content_length);
    return ESP_OK;
}

static esp_err_t hidden_css_handler(httpd_req_t *req) {
    size_t content_length = hidden_css_end - hidden_css_start;
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, (const char*)hidden_css_start, content_length);
    return ESP_OK;
}

// Démarrage du serveur web
static esp_err_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    
    if (httpd_start(&server, &config) == ESP_OK) {
        // Routes essentielles
        httpd_uri_t index_uri = { .uri = "/", .method = HTTP_GET, .handler = index_handler };
        httpd_uri_t index_html_uri = { .uri = "/index.html", .method = HTTP_GET, .handler = index_handler };
        httpd_uri_t login_uri = { .uri = "/login", .method = HTTP_GET, .handler = login_handler };
        httpd_uri_t login_html_uri = { .uri = "/login.html", .method = HTTP_GET, .handler = login_handler };
        httpd_uri_t news_uri = { .uri = "/news", .method = HTTP_GET, .handler = news_handler };
        httpd_uri_t news_html_uri = { .uri = "/news.html", .method = HTTP_GET, .handler = news_handler };
        httpd_uri_t main_css_uri = { .uri = "/main.css", .method = HTTP_GET, .handler = main_css_handler };
        httpd_uri_t login_css_uri = { .uri = "/login.css", .method = HTTP_GET, .handler = login_css_handler };
        httpd_uri_t hidden_css_uri = { .uri = "/hidden.css", .method = HTTP_GET, .handler = hidden_css_handler };
        httpd_uri_t script_js_uri = { .uri = "/script.js", .method = HTTP_GET, .handler = script_js_handler };
        httpd_uri_t api_news_uri = { .uri = "/api/news", .method = HTTP_GET, .handler = api_news_get_handler };
        
        // Routes pour les assets
        httpd_uri_t favicon_uri = { .uri = "/assets/favicon.ico", .method = HTTP_GET, .handler = favicon_handler };
        httpd_uri_t favicon_alt_uri = { .uri = "/favicon.ico", .method = HTTP_GET, .handler = favicon_handler };
        httpd_uri_t logo_uri = { .uri = "/assets/logo.png", .method = HTTP_GET, .handler = logo_handler };
        httpd_uri_t dembele_uri = { .uri = "/assets/dembele.png", .method = HTTP_GET, .handler = dembele_handler };
        
        httpd_register_uri_handler(server, &index_uri);
        httpd_register_uri_handler(server, &index_html_uri);
        httpd_register_uri_handler(server, &login_uri);
        httpd_register_uri_handler(server, &login_html_uri);
        httpd_register_uri_handler(server, &news_uri);
        httpd_register_uri_handler(server, &news_html_uri);
        httpd_register_uri_handler(server, &main_css_uri);
        httpd_register_uri_handler(server, &login_css_uri);
        httpd_register_uri_handler(server, &hidden_css_uri);
        httpd_register_uri_handler(server, &script_js_uri);
        httpd_register_uri_handler(server, &api_news_uri);
        
        // Enregistrer les assets
        httpd_register_uri_handler(server, &favicon_uri);
        httpd_register_uri_handler(server, &favicon_alt_uri);
        httpd_register_uri_handler(server, &logo_uri);
        httpd_register_uri_handler(server, &dembele_uri);
        
        ESP_LOGI(TAG, "✅ Serveur web démarré sur le port 80");
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "❌ Erreur démarrage serveur web");
    return ESP_FAIL;
}

// Initialisation WiFi AP
static void wifi_init_ap(void) {
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = WIFI_CHANNEL,
            .password = WIFI_PASS,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "✅ WiFi AP initialisé. SSID:%s mot_de_passe:%s canal:%d", WIFI_SSID, WIFI_PASS, WIFI_CHANNEL);
}

// Initialisation ESP-NOW
static esp_err_t esp_now_init_slave(void) {
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(esp_now_recv_cb));
    
    ESP_LOGI(TAG, "✅ ESP-NOW initialisé");
    return ESP_OK;
}

void app_main(void) {
    ESP_LOGI(TAG, "🚀 DÉMARRAGE SLAVE ESP-NOW AVEC FRAGMENTATION AVANCÉE");
    
    // Initialisation NVS
    ESP_ERROR_CHECK(nvs_flash_init());
    
    // Initialisation réseau
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    
    // Initialisation WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Démarrer le WiFi AP
    wifi_init_ap();
    
    // Initialiser ESP-NOW
    esp_now_init_slave();
    
    // Démarrer le serveur web
    start_webserver();
    
    ESP_LOGI(TAG, "🎯 SLAVE PRÊT - WiFi AP 'City-news' + Fragmentation ESP-NOW");
    ESP_LOGI(TAG, "🌐 Interface web: http://192.168.4.1");
    ESP_LOGI(TAG, "📡 En attente de fragments du master...");
}
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_log.h"
#include "cJSON.h"

#define TAG "MESH_SLAVE"
#define WIFI_SSID "City-news"
#define WIFI_PASS "password123"
#define WIFI_CHANNEL 9
#define MAX_MESSAGE_COUNT 50
#define FRAGMENT_SIZE 256
#define MAX_RECONSTRUCTED_SIZE 2048

// Structure de fragmentation ESP-NOW (identique au master)
typedef struct {
    uint32_t message_id;
    uint8_t fragment_num;
    uint8_t total_fragments;
    uint16_t data_len;
    uint8_t data[FRAGMENT_SIZE];
} __attribute__((packed)) fragment_packet_t;

// Structure pour les messages reçus
typedef struct {
    int id;
    char title[128];
    char content[256];
    char timestamp[32];
} received_message_t;

// Structure pour gérer la reconstruction des fragments
typedef struct {
    uint32_t message_id;
    uint8_t total_fragments;
    uint8_t received_fragments;
    char* data_buffer;
    int total_size;
    bool fragments_received[32];
    bool in_use;
} message_reconstruction_t;

// Variables globales
static received_message_t received_messages[MAX_MESSAGE_COUNT];
static int message_count = 0;
static uint8_t master_mac[ESP_NOW_ETH_ALEN] = {0xb4, 0x3a, 0x45, 0xa1, 0x1f, 0x38};
static httpd_handle_t server = NULL;
static message_reconstruction_t pending_reconstructions[5];

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

// Fonction pour vérifier l'adresse MAC autorisée
static bool is_mac_authorized(const uint8_t *mac) {
    return memcmp(mac, master_mac, ESP_NOW_ETH_ALEN) == 0;
}

// Fonction pour traiter un message JSON complet
static void process_complete_message(const char* json_string) {
    ESP_LOGI(TAG, "📄 Traitement JSON: %s", json_string);
    
    cJSON *json = cJSON_Parse(json_string);
    if (json == NULL) {
        ESP_LOGE(TAG, "❌ Erreur parsing JSON");
        return;
    }
    
    cJSON *id_item = cJSON_GetObjectItem(json, "id");
    cJSON *title_item = cJSON_GetObjectItem(json, "title");
    cJSON *content_item = cJSON_GetObjectItem(json, "content");
    cJSON *created_at_item = cJSON_GetObjectItem(json, "created_at");
    
    if (id_item && title_item && content_item) {
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
        ESP_LOGI(TAG, "✅ Message ajouté - ID: %d, Titre: '%.30s'", 
                received_messages[index].id, received_messages[index].title);
    }
    
    cJSON_Delete(json);
}

// Fonction callback pour réception ESP-NOW
static void esp_now_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (!is_mac_authorized(recv_info->src_addr) || len != sizeof(fragment_packet_t)) {
        return;
    }
    
    fragment_packet_t *fragment = (fragment_packet_t*)data;
    ESP_LOGI(TAG, "🧩 Fragment reçu - ID: %lu, Fragment: %d/%d", 
            fragment->message_id, fragment->fragment_num, fragment->total_fragments);
    
    // Si c'est un fragment unique, traiter directement
    if (fragment->fragment_num == 1 && fragment->total_fragments == 1) {
        char *json_string = malloc(fragment->data_len + 1);
        if (json_string) {
            memcpy(json_string, fragment->data, fragment->data_len);
            json_string[fragment->data_len] = '\0';
            process_complete_message(json_string);
            free(json_string);
        }
        return;
    }
    
    // Gestion fragments multiples - trouver ou créer reconstruction
    message_reconstruction_t* recon = NULL;
    for (int i = 0; i < 5; i++) {
        if (pending_reconstructions[i].in_use && pending_reconstructions[i].message_id == fragment->message_id) {
            recon = &pending_reconstructions[i];
            break;
        }
    }
    
    // Si pas trouvé, créer nouveau
    if (!recon) {
        for (int i = 0; i < 5; i++) {
            if (!pending_reconstructions[i].in_use) {
                recon = &pending_reconstructions[i];
                recon->in_use = true;
                recon->message_id = fragment->message_id;
                recon->total_fragments = fragment->total_fragments;
                recon->received_fragments = 0;
                recon->total_size = 0;
                recon->data_buffer = malloc(MAX_RECONSTRUCTED_SIZE);
                memset(recon->fragments_received, false, sizeof(recon->fragments_received));
                break;
            }
        }
    }
    
    if (!recon || !recon->data_buffer) return;
    
    // Ajouter ce fragment
    int frag_idx = fragment->fragment_num - 1;
    if (frag_idx >= 0 && frag_idx < 32 && !recon->fragments_received[frag_idx]) {
        if (recon->total_size + fragment->data_len < MAX_RECONSTRUCTED_SIZE) {
            memcpy(recon->data_buffer + recon->total_size, fragment->data, fragment->data_len);
            recon->total_size += fragment->data_len;
            recon->fragments_received[frag_idx] = true;
            recon->received_fragments++;
            
            ESP_LOGI(TAG, "✅ Fragment %d/%d ajouté (%d bytes)", 
                    fragment->fragment_num, fragment->total_fragments, recon->total_size);
            
            // Si complet, traiter
            if (recon->received_fragments == recon->total_fragments) {
                recon->data_buffer[recon->total_size] = '\0';
                ESP_LOGI(TAG, "🎯 Message complet reconstruit: %d bytes", recon->total_size);
                process_complete_message(recon->data_buffer);
                
                // Nettoyer
                free(recon->data_buffer);
                memset(recon, 0, sizeof(message_reconstruction_t));
            }
        }
    }
}

// Handlers HTTP
static esp_err_t index_handler(httpd_req_t *req) {
    const size_t index_html_size = (index_html_end - index_html_start);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char*)index_html_start, index_html_size);
    return ESP_OK;
}

static esp_err_t login_handler(httpd_req_t *req) {
    const size_t login_html_size = (login_html_end - login_html_start);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char*)login_html_start, login_html_size);
    return ESP_OK;
}

static esp_err_t news_handler(httpd_req_t *req) {
    const size_t news_html_size = (news_html_end - news_html_start);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char*)news_html_start, news_html_size);
    return ESP_OK;
}

static esp_err_t main_css_handler(httpd_req_t *req) {
    const size_t main_css_size = (main_css_end - main_css_start);
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, (const char*)main_css_start, main_css_size);
    return ESP_OK;
}

static esp_err_t hidden_css_handler(httpd_req_t *req) {
    const size_t hidden_css_size = (hidden_css_end - hidden_css_start);
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, (const char*)hidden_css_start, hidden_css_size);
    return ESP_OK;
}

static esp_err_t login_css_handler(httpd_req_t *req) {
    const size_t login_css_size = (login_css_end - login_css_start);
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, (const char*)login_css_start, login_css_size);
    return ESP_OK;
}

static esp_err_t script_js_handler(httpd_req_t *req) {
    const size_t script_js_size = (script_js_end - script_js_start);
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char*)script_js_start, script_js_size);
    return ESP_OK;
}

static esp_err_t dembele_png_handler(httpd_req_t *req) {
    const size_t dembele_png_size = (dembele_png_end - dembele_png_start);
    httpd_resp_set_type(req, "image/png");
    httpd_resp_send(req, (const char*)dembele_png_start, dembele_png_size);
    return ESP_OK;
}

static esp_err_t favicon_ico_handler(httpd_req_t *req) {
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char*)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
}

static esp_err_t logo_png_handler(httpd_req_t *req) {
    const size_t logo_png_size = (logo_png_end - logo_png_start);
    httpd_resp_set_type(req, "image/png");
    httpd_resp_send(req, (const char*)logo_png_start, logo_png_size);
    return ESP_OK;
}

static esp_err_t api_news_get_handler(httpd_req_t *req) {
    cJSON *json_response = cJSON_CreateObject();
    
    if (message_count > 0) {
        cJSON *news_array = cJSON_CreateArray();
        
        int start_idx = (message_count > MAX_MESSAGE_COUNT) ? message_count - MAX_MESSAGE_COUNT : 0;
        int count = (message_count > MAX_MESSAGE_COUNT) ? MAX_MESSAGE_COUNT : message_count;
        
        for (int i = 0; i < count; i++) {
            int idx = (start_idx + i) % MAX_MESSAGE_COUNT;
            cJSON *news_item = cJSON_CreateObject();
            cJSON_AddNumberToObject(news_item, "id", received_messages[idx].id);
            cJSON_AddStringToObject(news_item, "title", received_messages[idx].title);
            cJSON_AddStringToObject(news_item, "content", received_messages[idx].content);
            cJSON_AddStringToObject(news_item, "timestamp", received_messages[idx].timestamp);
            cJSON_AddItemToArray(news_array, news_item);
        }
        
        cJSON_AddItemToObject(json_response, "news", news_array);
        cJSON_AddNumberToObject(json_response, "count", count);
        cJSON_AddStringToObject(json_response, "status", "success");
    } else {
        cJSON_AddStringToObject(json_response, "status", "waiting");
        cJSON_AddStringToObject(json_response, "message", "En attente - Aucune donnée reçue du master");
    }
    
    char *json_string = cJSON_Print(json_response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, json_string, strlen(json_string));
    
    free(json_string);
    cJSON_Delete(json_response);
    return ESP_OK;
}

// Démarrage du serveur web
static esp_err_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_open_sockets = 7; // Réduit pour correspondre aux limites système
    
    if (httpd_start(&server, &config) == ESP_OK) {
        // Routes
        httpd_uri_t routes[] = {
            {"/", HTTP_GET, index_handler, NULL},
            {"/login", HTTP_GET, login_handler, NULL},
            {"/news", HTTP_GET, news_handler, NULL},
            {"/css/main.css", HTTP_GET, main_css_handler, NULL},
            {"/css/hidden.css", HTTP_GET, hidden_css_handler, NULL},
            {"/css/login.css", HTTP_GET, login_css_handler, NULL},
            {"/js/script.js", HTTP_GET, script_js_handler, NULL},
            {"/images/dembele.png", HTTP_GET, dembele_png_handler, NULL},
            {"/favicon.ico", HTTP_GET, favicon_ico_handler, NULL},
            {"/images/logo.png", HTTP_GET, logo_png_handler, NULL},
            {"/api/news", HTTP_GET, api_news_get_handler, NULL}
        };
        
        for (int i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
            httpd_register_uri_handler(server, &routes[i]);
        }
        
        ESP_LOGI(TAG, "✅ Serveur web démarré sur port 80");
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
            .pmf_cfg = {.required = false},
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "✅ WiFi AP '%s' démarré sur canal %d", WIFI_SSID, WIFI_CHANNEL);
}

// Initialisation ESP-NOW
static esp_err_t esp_now_init_slave(void) {
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(esp_now_recv_cb));
    ESP_LOGI(TAG, "✅ ESP-NOW initialisé");
    return ESP_OK;
}

void app_main(void) {
    ESP_LOGI(TAG, "🚀 DÉMARRAGE MESH SLAVE avec fragmentation avancée");
    
    // Initialiser NVS
    ESP_ERROR_CHECK(nvs_flash_init());
    
    // Initialiser réseau
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    
    // Initialiser WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Initialiser structures de reconstruction
    memset(pending_reconstructions, 0, sizeof(pending_reconstructions));
    
    // Démarrer WiFi AP
    wifi_init_ap();
    
    // Initialiser ESP-NOW
    esp_now_init_slave();
    
    // Démarrer serveur web
    start_webserver();
    
    ESP_LOGI(TAG, "🎯 SLAVE PRÊT - WiFi: %s, Canal: %d", WIFI_SSID, WIFI_CHANNEL);
    ESP_LOGI(TAG, "🌐 Interface web disponible sur http://192.168.4.1");
}
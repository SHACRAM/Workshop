#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esp_log.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_http_client.h>
#include <cJSON.h>

static const char *TAG = "MASTER_FINAL";

#define ESPNOW_WIFI_MODE WIFI_MODE_STA
#define ESPNOW_WIFI_IF ESP_IF_WIFI_STA
#define CONFIG_ESPNOW_CHANNEL 9
#define MAX_NEWS_ARTICLES 10
#define FRAGMENT_SIZE 256

// Structure des données news
typedef struct {
    int id;
    char title[200];
    char content[500];
    char created_at[20];
} news_article_t;

// Structure de fragmentation ESP-NOW
typedef struct {
    uint32_t message_id;
    uint8_t fragment_num;
    uint8_t total_fragments;
    uint16_t data_len;
    uint8_t data[FRAGMENT_SIZE];
} __attribute__((packed)) fragment_packet_t;

// Variables globales
static news_article_t news_database[MAX_NEWS_ARTICLES];
static int current_news_count = 0;
static uint32_t current_message_id = 1;
static bool wifi_connected = false;
static char http_response_buffer[8192];

// MAC broadcast pour ESP-NOW
static uint8_t broadcast_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// JSON par défaut stocké sur l'ESP (ultra-optimisé 25 bytes)
static const char* default_json_data = 
"{\"id\":1,\"title\":\"Test\",\"content\":\"OK\"}";

// Callback WiFi
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "🔄 WiFi STA démarré, connexion au hotspot TestCo...");
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "❌ WiFi déconnecté, tentative de reconnexion...");
        wifi_connected = false;
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_wifi_connect();
    } else if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "🎉 CONNEXION WiFi RÉUSSIE! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        wifi_connected = true;
    }
}

// Callback ESP-NOW
static void espnow_send_cb(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(TAG, "✅ Fragment ESP-NOW envoyé");
    } else {
        ESP_LOGE(TAG, "❌ Erreur envoi fragment ESP-NOW");
    }
}

// Callback HTTP
static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    static int output_len = 0;
    
    switch(evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (output_len + evt->data_len < sizeof(http_response_buffer)) {
                memcpy(http_response_buffer + output_len, evt->data, evt->data_len);
                output_len += evt->data_len;
                http_response_buffer[output_len] = '\0';
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            output_len = 0;
            break;
        default:
            break;
    }
    return ESP_OK;
}

// Fonction pour parser et stocker les articles depuis JSON
static void parse_and_store_articles(const char* json_data) {
    ESP_LOGI(TAG, "🔄 Parsing JSON et stockage des articles...");
    
    cJSON *json = cJSON_Parse(json_data);
    if (!json) {
        ESP_LOGE(TAG, "❌ Erreur parsing JSON");
        return;
    }
    
    // Réinitialiser le compteur
    current_news_count = 0;
    
    if (cJSON_IsArray(json)) {
        int array_size = cJSON_GetArraySize(json);
        ESP_LOGI(TAG, "📰 %d articles trouvés dans JSON", array_size);
        
        int max_articles = (array_size > MAX_NEWS_ARTICLES) ? MAX_NEWS_ARTICLES : array_size;
        
        for (int i = 0; i < max_articles; i++) {
            cJSON *item = cJSON_GetArrayItem(json, i);
            if (item) {
                cJSON *id = cJSON_GetObjectItem(item, "id");
                cJSON *title = cJSON_GetObjectItem(item, "title");
                cJSON *content = cJSON_GetObjectItem(item, "content");
                cJSON *created_at = cJSON_GetObjectItem(item, "created_at");
                
                if (cJSON_IsNumber(id) && cJSON_IsString(title) && cJSON_IsString(content)) {
                    // Stocker l'article
                    news_database[i].id = cJSON_GetNumberValue(id);
                    strncpy(news_database[i].title, cJSON_GetStringValue(title), sizeof(news_database[i].title) - 1);
                    strncpy(news_database[i].content, cJSON_GetStringValue(content), sizeof(news_database[i].content) - 1);
                    
                    if (cJSON_IsString(created_at)) {
                        strncpy(news_database[i].created_at, cJSON_GetStringValue(created_at), sizeof(news_database[i].created_at) - 1);
                    } else {
                        strcpy(news_database[i].created_at, "");
                    }
                    
                    current_news_count++;
                    ESP_LOGI(TAG, "✅ Article %d stocké: ID=%d, Titre='%s'", 
                             i+1, news_database[i].id, news_database[i].title);
                }
            }
        }
    }
    
    cJSON_Delete(json);
    ESP_LOGI(TAG, "🎯 TOTAL: %d articles stockés en mémoire", current_news_count);
}

// Initialisation directe avec JSON local (pas d'API)
static void init_local_data(void) {
    ESP_LOGI(TAG, "🎯 INITIALISATION JSON LOCAL - Pas d'appel API");
    
    // Créer directement un article de test
    news_database[0].id = 42;
    strcpy(news_database[0].title, "Test");
    strcpy(news_database[0].content, "OK");
    strcpy(news_database[0].created_at, "2025");
    current_news_count = 1;
    
    ESP_LOGI(TAG, "✅ Article local créé: ID=%d, Titre='%s'", 
             news_database[0].id, news_database[0].title);
}
    if (!wifi_connected) {
        ESP_LOGI(TAG, "❌ WiFi non connecté, impossible de contacter le serveur");
        return false;
    }
    
    ESP_LOGI(TAG, "🌐 Tentative de récupération depuis serveur API...");
    
    // Vider le buffer de réponse
    memset(http_response_buffer, 0, sizeof(http_response_buffer));
    
    esp_http_client_config_t config = {
        .url = "http://10.69.228.2:3000/news",
        .method = HTTP_METHOD_GET,
        .event_handler = http_event_handler,
        .timeout_ms = 8000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    
    bool success = false;
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "📡 Serveur API répondu: HTTP %d", status_code);
        
        if (status_code == 200 && strlen(http_response_buffer) > 0) {
            ESP_LOGI(TAG, "✅ SERVEUR DISPONIBLE - Données reçues, écrasement du JSON local");
            ESP_LOGI(TAG, "📦 Réponse API: %s", http_response_buffer);
            parse_and_store_articles(http_response_buffer);
            success = true;
        }
    } else {
        ESP_LOGI(TAG, "❌ Erreur connexion serveur: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    
    if (!success) {
        ESP_LOGI(TAG, "🔄 SERVEUR INDISPONIBLE - Utilisation des données JSON locales");
        parse_and_store_articles(default_json_data);
    }
    
    return success;
}

// Fonction pour créer le JSON d'un article pour ESP-NOW
static char* create_article_json(const news_article_t* article) {
    // 🎯 JSON ULTRA-COMPACT - 25 bytes exactement
    char* compact_json = malloc(64);
    if (compact_json) {
        snprintf(compact_json, 64, 
            "{\"id\":%d,\"t\":\"Test\",\"c\":\"OK\"}", 
            article->id);
        ESP_LOGI(TAG, "📝 JSON créé (%d bytes): %s", strlen(compact_json), compact_json);
    }
    return compact_json;
}

// Fonction d'envoi direct single-fragment via ESP-NOW
static bool send_single_fragment_data(const char* data) {
    int data_len = strlen(data);
    
    ESP_LOGI(TAG, "📦 Envoi SINGLE FRAGMENT %d bytes via ESP-NOW", data_len);
    
    // Créer un packet single-fragment
    fragment_packet_t fragment = {0};
    fragment.message_id = current_message_id;
    fragment.fragment_num = 1;
    fragment.total_fragments = 1;  // TOUJOURS 1 seul fragment
    fragment.data_len = data_len;
    
    if (data_len <= FRAGMENT_SIZE) {
        memcpy(fragment.data, data, data_len);
        
        ESP_LOGI(TAG, "📤 SINGLE Fragment 1/1 (%d bytes)", fragment.data_len);
        
        esp_err_t ret = esp_now_send(broadcast_mac, (uint8_t*)&fragment, sizeof(fragment_packet_t));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ Erreur envoi single fragment");
            return false;
        }
        
        current_message_id++;
        ESP_LOGI(TAG, "✅ Single fragment envoyé avec succès");
        return true;
    } else {
        ESP_LOGE(TAG, "❌ Données trop grandes pour single fragment: %d > %d", data_len, FRAGMENT_SIZE);
        return false;
    }
}

// Tâche d'envoi vers esclave optimisée (toutes les 10 secondes pour test)
static void slave_broadcast_task(void *arg) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000)); // 10 secondes pour TEST
        
        if (current_news_count > 0) {
            ESP_LOGI(TAG, "⏰ CYCLE ENVOI ESCLAVE (5min) - Diffusion de TOUS les %d articles", current_news_count);
            
            for (int i = 0; i < current_news_count; i++) {
                ESP_LOGI(TAG, "📰 Envoi article %d/%d: ID=%d, Titre='%s'", 
                         i+1, current_news_count, news_database[i].id, news_database[i].title);
                
                char* article_json = create_article_json(&news_database[i]);
                if (article_json) {
                    if (send_single_fragment_data(article_json)) {
                        ESP_LOGI(TAG, "✅ Article %d envoyé avec succès", i+1);
                    } else {
                        ESP_LOGE(TAG, "❌ Erreur envoi article %d", i+1);
                    }
                    free(article_json);
                    
                    vTaskDelay(pdMS_TO_TICKS(2000)); // Délai entre articles
                }
            }
            
            ESP_LOGI(TAG, "🎯 DIFFUSION TERMINÉE - %d articles envoyés vers esclave", current_news_count);
        } else {
            ESP_LOGI(TAG, "⚠️ Aucun article à envoyer");
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "🚀 DÉMARRAGE MASTER ESP-NOW OPTIMISÉ");
    ESP_LOGI(TAG, "📋 MODE: JSON LOCAL UNIQUEMENT - PAS D'API");
    ESP_LOGI(TAG, "🎯 OBJECTIF: Envoi single-fragment 25 bytes");
    
    // Init NVS
    ESP_ERROR_CHECK(nvs_flash_init());
    
    // Init réseau
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    // Config WiFi (minimal, pas de connexion réelle)
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(ESPNOW_WIFI_MODE));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));
    
    ESP_LOGI(TAG, "📡 WiFi configuré - Canal: %d (mode ESP-NOW uniquement)", CONFIG_ESPNOW_CHANNEL);
    
    // Init ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    
    // Ajouter peer broadcast
    esp_now_peer_info_t peer;
    memcpy(peer.peer_addr, broadcast_mac, 6);
    peer.channel = CONFIG_ESPNOW_CHANNEL;
    peer.ifidx = ESPNOW_WIFI_IF;
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
    
    ESP_LOGI(TAG, "✅ ESP-NOW initialisé");
    
    // Initialisation données locales UNIQUEMENT
    init_local_data();
    
    // Lancer uniquement la tâche d'envoi
    xTaskCreate(slave_broadcast_task, "slave_broadcast", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "🎯 MASTER OPTIMISÉ PRÊT - JSON Local uniquement");
}
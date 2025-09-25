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
#include <freertos/event_groups.h>
#include <cJSON.h>

static const char *TAG = "MASTER_OPTIMIZED";

#define ESPNOW_WIFI_MODE WIFI_MODE_STA
#define ESPNOW_WIFI_IF ESP_IF_WIFI_STA
#define ESPNOW_CHANNEL 9  // Même canal que le hotspot TestCo
#define FRAGMENT_SIZE 256

// Configuration WiFi hotspot
#define WIFI_SSID "TestCo"
#define WIFI_PASS "testPassword"
#define WIFI_MAXIMUM_RETRY 5
#define SERVER_IP_PRIMARY "10.69.228.2"
#define SERVER_IP_FALLBACK "192.168.56.1"
#define SERVER_PORT "3000"

// Event bits pour WiFi
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

// Variables pour gestion WiFi
static EventGroupHandle_t wifi_event_group;
static int wifi_retry_num = 0;

// Structure des données news (ajustée pour correspondre au slave)
typedef struct {
    int id;
    char title[128];
    char content[256];
    char created_at[32];
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
static news_article_t news_database[1];
static int current_news_count = 0;
static uint32_t current_message_id = 1;

// MAC du slave pour ESP-NOW (ESP32 #2)
static uint8_t slave_mac[] = {0x94, 0xa9, 0x90, 0x2e, 0x8c, 0xec};

// Event handler pour WiFi
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (wifi_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            wifi_retry_num++;
            ESP_LOGI(TAG, "Retry connecting to WiFi (%d/%d)", wifi_retry_num, WIFI_MAXIMUM_RETRY);
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "Connect to WiFi failed");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "✅ WiFi connecté - IP: " IPSTR, IP2STR(&event->ip_info.ip));
        wifi_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Initialisation WiFi
static void wifi_init_sta(void) {
    wifi_event_group = xEventGroupCreate();

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "📶 Connexion au hotspot '%s'...", WIFI_SSID);

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "✅ WiFi connecté avec succès");
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "❌ Échec connexion WiFi");
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

// Buffer pour stockage des données HTTP
#define HTTP_BUFFER_SIZE 2048
static char http_buffer[HTTP_BUFFER_SIZE];
static int http_buffer_len = 0;

// Event handler pour client HTTP
static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (http_buffer_len + evt->data_len < HTTP_BUFFER_SIZE - 1) {
                memcpy(http_buffer + http_buffer_len, evt->data, evt->data_len);
                http_buffer_len += evt->data_len;
                http_buffer[http_buffer_len] = '\0';
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}

// Fonction pour récupérer les données depuis le serveur Node.js
static bool http_get_news(void) {
    ESP_LOGI(TAG, "🌐 Récupération des données depuis le serveur...");
    
    // Reset buffer
    http_buffer_len = 0;
    memset(http_buffer, 0, HTTP_BUFFER_SIZE);
    
    // Essai avec IP primaire
    char url_primary[128];
    snprintf(url_primary, sizeof(url_primary), "http://%s:%s/news", SERVER_IP_PRIMARY, SERVER_PORT);
    
    esp_http_client_config_t config = {
        .url = url_primary,
        .event_handler = http_event_handler,
        .timeout_ms = 10000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "📡 HTTP GET Status = %d, content_length = %d",
                status_code, esp_http_client_get_content_length(client));
        
        if (status_code == 200 && http_buffer_len > 0) {
            ESP_LOGI(TAG, "✅ Données reçues (%d bytes): %.*s", http_buffer_len, http_buffer_len, http_buffer);
            esp_http_client_cleanup(client);
            return true;
        }
    } else {
        ESP_LOGW(TAG, "⚠️ Échec IP primaire %s, essai fallback...", SERVER_IP_PRIMARY);
    }
    
    esp_http_client_cleanup(client);
    
    // Essai avec IP fallback
    char url_fallback[128];
    snprintf(url_fallback, sizeof(url_fallback), "http://%s:%s/news", SERVER_IP_FALLBACK, SERVER_PORT);
    
    config.url = url_fallback;
    client = esp_http_client_init(&config);
    err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "📡 HTTP GET Fallback Status = %d, content_length = %d",
                status_code, esp_http_client_get_content_length(client));
        
        if (status_code == 200 && http_buffer_len > 0) {
            ESP_LOGI(TAG, "✅ Données reçues via fallback (%d bytes): %.*s", http_buffer_len, http_buffer_len, http_buffer);
            esp_http_client_cleanup(client);
            return true;
        }
    }
    
    ESP_LOGE(TAG, "❌ Échec récupération des données sur les deux IPs");
    esp_http_client_cleanup(client);
    return false;
}

// Fonction pour parser le JSON reçu et mettre à jour les données
static void update_news_data(void) {
    if (http_buffer_len == 0) {
        ESP_LOGW(TAG, "⚠️ Aucune donnée HTTP à parser");
        return;
    }
    
    ESP_LOGI(TAG, "📄 Parsing JSON reçu...");
    
    cJSON *json = cJSON_Parse(http_buffer);
    if (json == NULL) {
        ESP_LOGE(TAG, "❌ Erreur parsing JSON");
        return;
    }
    
    if (cJSON_IsArray(json)) {
        int array_size = cJSON_GetArraySize(json);
        ESP_LOGI(TAG, "📰 %d articles trouvés dans le JSON", array_size);
        
        // Limiter à 1 article pour l'instant
        current_news_count = (array_size > 0) ? 1 : 0;
        
        if (current_news_count > 0) {
            cJSON *item = cJSON_GetArrayItem(json, 0);
            
            cJSON *id = cJSON_GetObjectItem(item, "id");
            cJSON *title = cJSON_GetObjectItem(item, "title");
            cJSON *content = cJSON_GetObjectItem(item, "content");
            cJSON *created_at = cJSON_GetObjectItem(item, "created_at");
            
            if (cJSON_IsNumber(id)) {
                news_database[0].id = id->valueint;
            }
            
            if (cJSON_IsString(title)) {
                strncpy(news_database[0].title, title->valuestring, sizeof(news_database[0].title) - 1);
                news_database[0].title[sizeof(news_database[0].title) - 1] = '\0';
            }
            
            if (cJSON_IsString(content)) {
                strncpy(news_database[0].content, content->valuestring, sizeof(news_database[0].content) - 1);
                news_database[0].content[sizeof(news_database[0].content) - 1] = '\0';
            }
            
            if (cJSON_IsString(created_at)) {
                strncpy(news_database[0].created_at, created_at->valuestring, sizeof(news_database[0].created_at) - 1);
                news_database[0].created_at[sizeof(news_database[0].created_at) - 1] = '\0';
            }
            
            ESP_LOGI(TAG, "✅ Article mis à jour: ID=%d, Titre='%s', Contenu='%s'", 
                     news_database[0].id, news_database[0].title, news_database[0].content);
        }
    } else {
        ESP_LOGE(TAG, "❌ JSON reçu n'est pas un array");
    }
    
    cJSON_Delete(json);
}

// Callback ESP-NOW
static void espnow_send_cb(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(TAG, "✅ Fragment ESP-NOW envoyé");
    } else {
        ESP_LOGE(TAG, "❌ Erreur envoi fragment ESP-NOW");
    }
}

// Initialisation avec données du serveur
static void init_server_data(void) {
    ESP_LOGI(TAG, "� INITIALISATION AVEC DONNÉES SERVEUR");
    
    if (http_get_news()) {
        update_news_data();
        ESP_LOGI(TAG, "✅ Données serveur chargées avec succès");
    } else {
        ESP_LOGW(TAG, "⚠️ Échec récupération serveur, utilisation données par défaut");
        // Données par défaut en cas d'échec
        news_database[0].id = 42;
        strcpy(news_database[0].title, "Test Local");
        strcpy(news_database[0].content, "Serveur indisponible");
        strcpy(news_database[0].created_at, "2025");
        current_news_count = 1;
    }
}

// Fonction pour créer le JSON d'un article pour ESP-NOW (format base de données complet)
static char* create_article_json(const news_article_t* article) {
    char* json_string = malloc(1024); // Buffer plus large pour les données complètes
    if (json_string) {
        // Format JSON complet correspondant à la base de données
        snprintf(json_string, 1024, 
            "{\"id\":%d,\"title\":\"%s\",\"content\":\"%s\",\"created_at\":\"%s\"}", 
            article->id, article->title, article->content, article->created_at);
        ESP_LOGI(TAG, "📝 JSON format BD créé (%d bytes): %s", strlen(json_string), json_string);
    }
    return json_string;
}

// Fonction d'envoi multi-fragments via ESP-NOW
static bool send_multi_fragment_data(const char* data) {
    int data_len = strlen(data);
    int max_payload = FRAGMENT_SIZE - 1; // -1 pour le null terminator si nécessaire
    int total_fragments = (data_len + max_payload - 1) / max_payload; // Arrondi supérieur
    
    ESP_LOGI(TAG, "📦 Envoi MULTI-FRAGMENTS %d bytes en %d fragments via ESP-NOW", data_len, total_fragments);
    
    // Envoyer chaque fragment
    for (int frag_num = 1; frag_num <= total_fragments; frag_num++) {
        fragment_packet_t fragment = {0};
        fragment.message_id = current_message_id;
        fragment.fragment_num = frag_num;
        fragment.total_fragments = total_fragments;
        
        // Calculer la position et la taille des données pour ce fragment
        int offset = (frag_num - 1) * max_payload;
        int remaining = data_len - offset;
        int payload_size = (remaining < max_payload) ? remaining : max_payload;
        
        fragment.data_len = payload_size;
        memcpy(fragment.data, data + offset, payload_size);
        
        ESP_LOGI(TAG, "📤 Fragment %d/%d (offset:%d, size:%d bytes)", 
                frag_num, total_fragments, offset, payload_size);
        
        esp_err_t ret = esp_now_send(slave_mac, (uint8_t*)&fragment, sizeof(fragment_packet_t));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ Erreur envoi fragment %d/%d", frag_num, total_fragments);
            return false;
        }
        
        // Petit délai entre les fragments pour éviter la congestion
        vTaskDelay(pdMS_TO_TICKS(100));
        ESP_LOGI(TAG, "✅ Fragment %d/%d envoyé avec succès", frag_num, total_fragments);
    }
    
    current_message_id++;
    ESP_LOGI(TAG, "🎯 Message complet envoyé avec succès (%d fragments)", total_fragments);
    return true;
}

// Tâche de récupération périodique des données du serveur
static void server_fetch_task(void *arg) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000)); // Attendre 1 minute
        
        ESP_LOGI(TAG, "⏰ RÉCUPÉRATION PÉRIODIQUE DES DONNÉES SERVEUR");
        
        if (http_get_news()) {
            update_news_data();
            ESP_LOGI(TAG, "✅ Données serveur mises à jour");
        } else {
            ESP_LOGE(TAG, "❌ Échec récupération périodique");
        }
    }
}

// Tâche d'envoi vers esclave optimisée (toutes les 10 secondes pour test)
static void slave_broadcast_task(void *arg) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000)); // 10 secondes pour TEST
        
        if (current_news_count > 0) {
            ESP_LOGI(TAG, "⏰ CYCLE ENVOI ESCLAVE - Diffusion de %d article(s)", current_news_count);
            
            for (int i = 0; i < current_news_count; i++) {
                ESP_LOGI(TAG, "📰 Envoi article %d/%d: ID=%d", 
                         i+1, current_news_count, news_database[i].id);
                
                char* article_json = create_article_json(&news_database[i]);
                if (article_json) {
                    if (send_multi_fragment_data(article_json)) {
                        ESP_LOGI(TAG, "✅ Article %d envoyé avec succès", i+1);
                    } else {
                        ESP_LOGE(TAG, "❌ Erreur envoi article %d", i+1);
                    }
                    free(article_json);
                    
                    vTaskDelay(pdMS_TO_TICKS(3000)); // Plus de délai pour les fragments multiples
                }
            }
            
            ESP_LOGI(TAG, "🎯 DIFFUSION TERMINÉE - %d articles envoyés", current_news_count);
        } else {
            ESP_LOGI(TAG, "⚠️ Aucun article à envoyer");
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "🚀 DÉMARRAGE MASTER ESP-NOW AVEC SERVEUR HTTP");
    ESP_LOGI(TAG, "📋 MODE: Récupération données depuis serveur Node.js");
    ESP_LOGI(TAG, "🎯 OBJECTIF: Envoi données serveur vers esclave");
    
    // Init NVS
    ESP_ERROR_CHECK(nvs_flash_init());
    
    // Init réseau
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    // Config WiFi et connexion au hotspot
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Connexion au hotspot TestCo
    wifi_init_sta();
    
    // ESP-NOW utilise automatiquement le canal du WiFi connecté (canal 9)
    // Pas besoin de changer le canal manuellement
    ESP_LOGI(TAG, "📡 WiFi connecté - ESP-NOW utilisera le canal du hotspot");
    
    // Init ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    
    // Ajouter peer slave
    esp_now_peer_info_t peer;
    memcpy(peer.peer_addr, slave_mac, 6);
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx = ESPNOW_WIFI_IF;
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
    
    ESP_LOGI(TAG, "✅ ESP-NOW initialisé");
    
    // Récupération initiale des données serveur
    init_server_data();
    
    // Lancer les tâches
    xTaskCreate(server_fetch_task, "server_fetch", 8192, NULL, 4, NULL);
    xTaskCreate(slave_broadcast_task, "slave_broadcast", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "🎯 MASTER AVEC SERVEUR HTTP PRÊT");
}
idf_component_register(SRCS "mesh_master_final.c"
                    PRIV_REQUIRES nvs_flash esp_event esp_netif esp_wifi json esp_timer esp_http_client
                    INCLUDE_DIRS ".")

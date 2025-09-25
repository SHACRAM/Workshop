idf_component_register(SRCS "mesh_slave_main_fixed.c"
                    PRIV_REQUIRES nvs_flash esp_event esp_netif esp_wifi esp_http_server json
                    INCLUDE_DIRS "."
                    EMBED_FILES "../../web_files/index.html"
                                "../../web_files/login.html"
                                "../../web_files/news.html"
                                "../../web_files/main.css"
                                "../../web_files/hidden.css"
                                "../../web_files/login.css"
                                "../../web_files/script.js"
                                "../../web_files/assets/dembele.png"
                                "../../web_files/assets/favicon.ico"
                                "../../web_files/assets/logo.png")
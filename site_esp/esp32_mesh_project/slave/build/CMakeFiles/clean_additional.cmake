# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "bootloader/bootloader.bin"
  "bootloader/bootloader.elf"
  "bootloader/bootloader.map"
  "config/sdkconfig.cmake"
  "config/sdkconfig.h"
  "dembele.png.S"
  "esp-idf/esptool_py/flasher_args.json.in"
  "esp-idf/mbedtls/x509_crt_bundle"
  "favicon.ico.S"
  "flash_app_args"
  "flash_bootloader_args"
  "flash_project_args"
  "flasher_args.json"
  "hidden.css.S"
  "index.html.S"
  "ldgen_libraries"
  "ldgen_libraries.in"
  "login.css.S"
  "login.html.S"
  "logo.png.S"
  "main.css.S"
  "mesh_slave.bin"
  "mesh_slave.map"
  "news.html.S"
  "project_elf_src_esp32s3.c"
  "script.js.S"
  "x509_crt_bundle.S"
  )
endif()

#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint32_t TickType_t;
typedef void *QueueHandle_t;
typedef void *SemaphoreHandle_t;
typedef void *TaskHandle_t;
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_NO_MEM 1
#define ESP_BT_MODE_CLASSIC_BT 1
#define ESP_BT_MODE_BLE 2
#define pdTRUE 1
#define pdPASS 1
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(x) (x)
#define ESP_ERROR_CHECK(x) assert((x) == ESP_OK)
#define ESP_LOGE(tag, ...) do { (void)(tag); if (0) printf(__VA_ARGS__); } while (0)
#define ESP_LOGI(tag, ...) ESP_LOGE(tag, __VA_ARGS__)
#define ESP_LOGW(tag, ...) ESP_LOGE(tag, __VA_ARGS__)
#define HCIT_TYPE_COMMAND 1
#define HCIT_TYPE_ACL_DATA 2
#define HCIT_TYPE_EVENT 4
typedef struct { int dummy; } esp_bt_controller_config_t;
#define BT_CONTROLLER_INIT_CONFIG_DEFAULT() {0}
typedef struct { void (*ready)(void); int (*recv)(uint8_t *, uint16_t); } esp_vhci_host_callback_t;
QueueHandle_t xQueueCreate(unsigned depth, unsigned item_size);
int xQueueSend(QueueHandle_t, const void *, TickType_t);
int xQueueReceive(QueueHandle_t, void *, TickType_t);
SemaphoreHandle_t xSemaphoreCreateBinary(void);
int xSemaphoreTake(SemaphoreHandle_t, TickType_t);
int xSemaphoreGive(SemaphoreHandle_t);
int xTaskCreate(void (*fn)(void *), const char *, unsigned, void *, unsigned, TaskHandle_t *);
TickType_t xTaskGetTickCount(void);
void vTaskDelay(TickType_t);
void esp_restart(void);
bool esp_vhci_host_check_send_available(void);
void esp_vhci_host_send_packet(uint8_t *, uint16_t);
int esp_bt_controller_mem_release(int);
int esp_bt_controller_init(esp_bt_controller_config_t *);
int esp_bt_controller_enable(int);
int esp_vhci_host_register_callback(esp_vhci_host_callback_t *);
bool tud_mounted(void);
bool tud_bt_event_send(void *, uint16_t);
bool tud_bt_acl_data_send(void *, uint16_t);

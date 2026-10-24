/*
 * SPDX-FileCopyrightText: 2020-2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 * WIP Windows POC: separate USB/VHCI callbacks with copied, queued packets.
 * No fabricated HCI command responses or Classic Bluetooth emulation.
 */
#include <string.h>
#include <stddef.h>
#include "esp_check.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_bt.h"
#include "tinyusb.h"
#include "tusb_bth.h"

#define ACL_PAYLOAD_MAX 1021
#define PACKET_MAX (1 + 4 + ACL_PAYLOAD_MAX)
#define QUEUE_DEPTH 12
#define IO_TIMEOUT_MS 3000
typedef struct {
    uint16_t len; // Includes H4 type byte; USB transfers omit it.
    uint8_t reserved; // data + 1 (USB payload) must be 4-byte aligned for DMA.
    uint8_t data[PACKET_MAX];
} hci_packet_t;
_Static_assert((offsetof(hci_packet_t, data) + 1) % 4 == 0, "USB DMA alignment");
static const char *TAG = "bth_poc";
static QueueHandle_t to_usb, to_controller;
static SemaphoreHandle_t usb_done;
static TaskHandle_t usb_worker, controller_worker;
static hci_packet_t acl_partial;
static size_t acl_expected;

// Reset also stops USB DMA. Never silently lose credits or recycle an in-flight buffer.
static void transport_fault(const char *reason)
{
    ESP_LOGE(TAG, "WIP transport reset: %s", reason);
    esp_restart();
}
static void enqueue(QueueHandle_t queue, const hci_packet_t *packet)
{
    if (xQueueSend(queue, packet, 0) != pdTRUE) {
        transport_fault("packet queue full");
    }
}
static void controller_rcv_pkt_ready(void) {}

static int host_rcv_pkt(uint8_t *data, uint16_t len)
{
    if (!data || len < 3 || len > PACKET_MAX) {
        transport_fault("invalid controller packet length");
        return 0;
    }
    if (data[0] == HCIT_TYPE_EVENT) {
        if (len != (uint16_t)(3 + data[2])) {
            transport_fault("invalid event length");
            return 0;
        }
        // Opcode/status only; never log keys, addresses or packet payloads.
        if (data[1] == 0x0e && len >= 7) {
            ESP_LOGI(TAG, "HCI complete opcode=%04x status=%02x",
                     data[4] | (data[5] << 8), data[6]);
        } else if (data[1] == 0x0f && len == 7) {
            ESP_LOGI(TAG, "HCI status opcode=%04x status=%02x",
                     data[5] | (data[6] << 8), data[3]);
        }
    } else if (data[0] == HCIT_TYPE_ACL_DATA) {
        if (len < 5 || (size_t)len != 5u + (data[3] | (data[4] << 8))) {
            transport_fault("invalid controller ACL length");
            return 0;
        }
    } else {
        transport_fault("unsupported controller packet type");
        return 0;
    }
    hci_packet_t packet = { .len = len };
    memcpy(packet.data, data, len);
    enqueue(to_usb, &packet); // Copy and return; never wait for USB here.
    return 0;
}

void tud_bt_hci_cmd_cb(void *hci_cmd, size_t cmd_len)
{
    const uint8_t *cmd = hci_cmd;
    if (!cmd || cmd_len < 3 || cmd_len > 258 || cmd_len != (size_t)(3 + cmd[2])) {
        transport_fault("invalid host command length");
        return;
    }
    hci_packet_t packet = { .len = cmd_len + 1, .data = { HCIT_TYPE_COMMAND } };
    memcpy(packet.data + 1, cmd, cmd_len);
    ESP_LOGI(TAG, "HCI command opcode=%04x bytes=%u", cmd[0] | (cmd[1] << 8), (unsigned)cmd_len);
    // Return so TinyUSB can process the control status stage and IN completions.
    enqueue(to_controller, &packet);
}

void tud_bt_acl_data_received_cb(void *acl_data, uint16_t data_len)
{
    const uint8_t *data = acl_data;
    if (!data || !data_len) {
        transport_fault("empty ACL fragment");
        return;
    }
    if (!acl_expected) {
        // The first full-speed USB transaction contains the four-byte HCI header.
        if (data_len < 4) {
            transport_fault("short ACL header");
            return;
        }
        acl_expected = 5 + (data[2] | (data[3] << 8));
        acl_partial.len = 1;
        acl_partial.data[0] = HCIT_TYPE_ACL_DATA;
    }
    if (acl_expected > PACKET_MAX || (size_t)acl_partial.len + data_len > acl_expected) {
        acl_expected = 0;
        acl_partial.len = 0;
        transport_fault("ACL fragment exceeds packet boundary");
        return;
    }
    memcpy(acl_partial.data + acl_partial.len, data, data_len);
    acl_partial.len += data_len;
    if (acl_partial.len == acl_expected) {
        enqueue(to_controller, &acl_partial);
        acl_expected = 0;
        acl_partial.len = 0;
    }
}

void tud_bt_event_sent_cb(uint16_t sent_bytes)
{
    (void)sent_bytes;
    xSemaphoreGive(usb_done);
}
void tud_bt_acl_data_sent_cb(uint16_t sent_bytes)
{
    (void)sent_bytes;
    xSemaphoreGive(usb_done);
}

static void send_to_controller(hci_packet_t *packet)
{
    TickType_t started = xTaskGetTickCount();
    while (!esp_vhci_host_check_send_available()) {
        if ((TickType_t)(xTaskGetTickCount() - started) >= pdMS_TO_TICKS(IO_TIMEOUT_MS)) {
            transport_fault("controller not accepting packets");
            return;
        }
        vTaskDelay(1);
    }
    esp_vhci_host_send_packet(packet->data, packet->len);
}
static void send_to_usb(hci_packet_t *packet)
{
    // One outstanding transfer across event/ACL endpoints; keep the worker buffer
    // alive until completion, including TinyUSB's optional ACL zero-length packet.
    TickType_t started = xTaskGetTickCount();
    while (true) {
        bool sent = false;
        if (tud_mounted()) {
            sent = packet->data[0] == HCIT_TYPE_EVENT
                ? tud_bt_event_send(packet->data + 1, packet->len - 1)
                : tud_bt_acl_data_send(packet->data + 1, packet->len - 1);
        }
        if (sent) {
            break;
        }
        if ((TickType_t)(xTaskGetTickCount() - started) >= pdMS_TO_TICKS(IO_TIMEOUT_MS)) {
            transport_fault("USB not mounted or endpoint busy");
            return;
        }
        vTaskDelay(1);
    }
    if (xSemaphoreTake(usb_done, pdMS_TO_TICKS(IO_TIMEOUT_MS)) != pdTRUE) {
        transport_fault("USB completion timeout");
    }
}
static void controller_task(void *arg)
{
    (void)arg;
    hci_packet_t packet;
    for (;;) {
        if (xQueueReceive(to_controller, &packet, portMAX_DELAY) == pdTRUE) {
            send_to_controller(&packet);
        }
    }
}
static void usb_task(void *arg)
{
    (void)arg;
    // Internal task stack on S3: PSRAM task stacks and USB D-cache are disabled.
    hci_packet_t packet __attribute__((aligned(4)));
    for (;;) {
        if (xQueueReceive(to_usb, &packet, portMAX_DELAY) == pdTRUE) {
            send_to_usb(&packet);
        }
    }
}
static esp_vhci_host_callback_t vhci_host_cb = {
    controller_rcv_pkt_ready, host_rcv_pkt
};
void tusb_bth_init(void)
{
    to_usb = xQueueCreate(QUEUE_DEPTH, sizeof(hci_packet_t));
    to_controller = xQueueCreate(QUEUE_DEPTH, sizeof(hci_packet_t));
    usb_done = xSemaphoreCreateBinary();
    ESP_ERROR_CHECK(to_usb && to_controller && usb_done ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreate(controller_task, "hci_to_ctrl", 4096, NULL, 5,
                               &controller_worker) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreate(usb_task, "hci_to_usb", 4096, NULL, 5,
                               &usb_worker) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    ESP_ERROR_CHECK(esp_bt_controller_init(&cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_vhci_host_register_callback(&vhci_host_cb));
    ESP_LOGW(TAG, "WIP Windows startup POC: BLE only; not hardware validated");
}
void tusb_bth_deinit(void)
{
    // Reset prevents USB DMA from outliving buffers. No live uninstall in this POC.
    esp_restart();
}

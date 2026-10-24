// Compile the actual firmware transport against deterministic FreeRTOS/USB mocks.
#include <setjmp.h>
#include "stubs/mock.h"
#include "../firmware/components/tinyusb_dongle/tusb_bth.c"

typedef struct { hci_packet_t packet; int count; } mock_queue_t;
static mock_queue_t queues[2];
static int next_queue, sem, sends, busy, fail_completion, vhci_busy, restarts;
static TickType_t ticks;
static hci_packet_t last_controller;
static jmp_buf fault;
QueueHandle_t xQueueCreate(unsigned depth, unsigned item_size) {
    assert(depth == QUEUE_DEPTH && item_size == sizeof(hci_packet_t));
    return &queues[next_queue++];
}
int xQueueSend(QueueHandle_t q, const void *p, TickType_t wait) {
    mock_queue_t *queue = q;
    assert(wait == 0); // Both callbacks must be nonblocking.
    if (queue->count) return 0;
    memcpy(&queue->packet, p, sizeof(hci_packet_t)); queue->count = 1; return 1;
}
int xQueueReceive(QueueHandle_t q, void *p, TickType_t wait) {
    (void)wait; mock_queue_t *queue = q;
    if (!queue->count) return 0;
    memcpy(p, &queue->packet, sizeof(hci_packet_t)); queue->count = 0; return 1;
}
SemaphoreHandle_t xSemaphoreCreateBinary(void) { return &sem; }
int xSemaphoreTake(SemaphoreHandle_t s, TickType_t wait) {
    assert(s == &sem && wait == IO_TIMEOUT_MS);
    if (fail_completion) return 0;
    int was = sem; sem = 0; return was;
}
int xSemaphoreGive(SemaphoreHandle_t s) { assert(s == &sem); sem = 1; return 1; }
int xTaskCreate(void (*fn)(void *), const char *n, unsigned stack, void *arg,
                unsigned priority, TaskHandle_t *handle) {
    (void)n; (void)stack; (void)arg; (void)priority; *handle = (void *)fn; return 1;
}
TickType_t xTaskGetTickCount(void) { return ticks; }
void vTaskDelay(TickType_t delay) { ticks += delay; }
void esp_restart(void) { restarts++; longjmp(fault, 1); }
bool esp_vhci_host_check_send_available(void) { return !vhci_busy; }
void esp_vhci_host_send_packet(uint8_t *p, uint16_t n) {
    last_controller.len = n; memcpy(last_controller.data, p, n);
}
int esp_bt_controller_mem_release(int mode) { assert(mode == ESP_BT_MODE_CLASSIC_BT); return 0; }
int esp_bt_controller_init(esp_bt_controller_config_t *cfg) { (void)cfg; return 0; }
int esp_bt_controller_enable(int mode) { assert(mode == ESP_BT_MODE_BLE); return 0; }
int esp_vhci_host_register_callback(esp_vhci_host_callback_t *cb) { assert(cb->recv == host_rcv_pkt); return 0; }
bool tud_mounted(void) { return true; }
bool tud_bt_event_send(void *p, uint16_t n) {
    assert((uintptr_t)p % 4 == 0); assert(n > 0);
    sends++;
    if (busy > 0) { busy--; return false; }
    if (busy < 0) return false;
    if (!fail_completion) tud_bt_event_sent_cb(n);
    return true;
}
bool tud_bt_acl_data_send(void *p, uint16_t n) { return tud_bt_event_send(p, n); }

#define EXPECT_FAULT(call) do { int before = restarts; if (!setjmp(fault)) { call; assert(!"expected restart"); } assert(restarts == before + 1); } while (0)
int main(void) {
    tusb_bth_init();
    uint8_t reset[] = {0x03, 0x0c, 0x00};
    tud_bt_hci_cmd_cb(reset, sizeof(reset));
    reset[0] = 0xff; // Queue must own a copy of TinyUSB's reusable buffer.
    hci_packet_t p __attribute__((aligned(4)));
    assert(xQueueReceive(to_controller, &p, 0));
    assert(p.len == 4 && p.data[0] == 1 && p.data[1] == 3);
    send_to_controller(&p);
    assert(last_controller.len == 4 && last_controller.data[1] == 3);

    uint8_t complete[] = {4, 0x0e, 4, 1, 3, 0x0c, 0};
    host_rcv_pkt(complete, sizeof(complete));
    complete[6] = 0xff;
    assert(sends == 0 && xQueueReceive(to_usb, &p, 0));
    assert(p.data[6] == 0); // VHCI callback also returns without waiting.
    busy = 2; send_to_usb(&p); assert(sends == 3 && ticks == 2 && sem == 0);

    uint8_t acl[104] = {1, 0x20, 100, 0};
    memset(acl + 4, 0x5a, 100);
    tud_bt_acl_data_received_cb(acl, 64);
    assert(!queues[1].count);
    tud_bt_acl_data_received_cb(acl + 64, 40);
    assert(xQueueReceive(to_controller, &p, 0) && p.len == 105);
    assert(!memcmp(p.data + 1, acl, sizeof(acl)));
    uint8_t zero_acl[] = {1, 0x20, 0, 0};
    tud_bt_acl_data_received_cb(zero_acl, 4);
    assert(xQueueReceive(to_controller, &p, 0) && p.len == 5);

    uint8_t bad_cmd[] = {3, 0x0c, 1};
    EXPECT_FAULT(tud_bt_hci_cmd_cb(bad_cmd, 3));
    EXPECT_FAULT(host_rcv_pkt(complete, 2));
    uint8_t oversized[] = {1, 0x20, 0xff, 0xff};
    EXPECT_FAULT(tud_bt_acl_data_received_cb(oversized, 4));
    EXPECT_FAULT(tud_bt_acl_data_received_cb(zero_acl, 3));
    uint8_t overflow[] = {1, 0x20, 0, 0, 0xaa};
    EXPECT_FAULT(tud_bt_acl_data_received_cb(overflow, 5));

    queues[0].count = 1;
    EXPECT_FAULT(host_rcv_pkt(complete, sizeof(complete)));
    queues[0].count = 0;
    p.len = 7; memcpy(p.data, complete, 7);
    busy = -1; EXPECT_FAULT(send_to_usb(&p)); // Failed send cannot wait on a nonexistent completion.
    busy = 0; fail_completion = 1; EXPECT_FAULT(send_to_usb(&p));
    fail_completion = 0; vhci_busy = 1; EXPECT_FAULT(send_to_controller(&p));
    puts("PASS: queued ownership, H4 forwarding, USB busy retry, DMA alignment, ACL fragmentation, malformed input, queue overflow, USB/controller timeouts");
    return 0;
}

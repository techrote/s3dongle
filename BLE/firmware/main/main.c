#include "esp_bt.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "tinyusb.h"
#include "tusb_bth.h"

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    // Prepare VHCI before exposing endpoints to the PC.
    tusb_bth_init();
    ESP_ERROR_CHECK(esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED
                    ? ESP_OK : ESP_FAIL);
    const tinyusb_config_t usb = { .external_phy = false };
    ESP_ERROR_CHECK(tinyusb_driver_install(&usb));
}

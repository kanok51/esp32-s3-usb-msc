#include <Arduino.h>
#include <SD.h>

#include "sd_card.h"
#include "wifi_ftp_bridge.h"
#include "config.h"

static void print_card_type(uint8_t card_type)
{
    Serial.print("Card type: ");
    switch (card_type) {
        case CARD_MMC:
            Serial.println("MMC");
            break;
        case CARD_SD:
            Serial.println("SDSC");
            break;
        case CARD_SDHC:
            Serial.println("SDHC/SDXC");
            break;
        default:
            Serial.println("UNKNOWN");
            break;
    }
}

void setup()
{
    Serial.begin(115200);
    delay(3000);

    Serial.println();
    Serial.println("ESP32-S3 SD + USB MSC + FTP");

    sd_card_config_t sd_cfg = {
        .sck = APP_SD_SCK,
        .miso = APP_SD_MISO,
        .mosi = APP_SD_MOSI,
        .cs = APP_SD_CS,
        .freq_hz = APP_SD_FREQ_HZ,
        .mount_point = APP_SD_MOUNT_POINT
    };

    if (!sd_card_init(&sd_cfg)) {
        Serial.println("SD init failed");
        return;
    }

    print_card_type(sd_card_type());
    Serial.printf("Card size: %llu MB\n", sd_card_size_bytes() / (1024ULL * 1024ULL));
    Serial.printf("Sector size: %u\n", static_cast<unsigned>(sd_card_sector_size()));
    Serial.printf("Sector count: %u\n", static_cast<unsigned>(sd_card_num_sectors()));

    File f = SD.open("/hello.txt", FILE_WRITE);
    if (f) {
        f.println("Hello from ESP32-S3");
        f.close();
    }

    wifi_ftp_bridge_config_t bridge_cfg = {
        .wifi_ssid = APP_WIFI_SSID,
        .wifi_password = APP_WIFI_PASSWORD,
        .ftp_user = APP_FTP_USER,
        .ftp_password = APP_FTP_PASSWORD
    };

    if (!wifi_ftp_bridge_begin(&bridge_cfg)) {
        Serial.println("WiFi/FTP bridge init failed");
        return;
    }

    Serial.print("FTP ready at: ftp://");
    Serial.println(wifi_ftp_bridge_ip());
    Serial.println("USB MSC is enabled while FTP is idle.");
    Serial.println("When an FTP client connects, MSC is disabled.");
    Serial.println("When the FTP client disconnects, MSC is enabled again.");
}

void loop()
{
    wifi_ftp_bridge_poll();
    delay(2);
}

#include <Arduino.h>
#include "USB.h"
#include "USBMSC.h"

extern "C" {
#include "esp_partition.h"
}

USBMSC MSC;

static constexpr const char *PART_LABEL = "mscraw";
static constexpr uint32_t BLOCK_SIZE = 512;
static constexpr uint32_t ERASE_SIZE = 4096;

static const esp_partition_t *g_part = nullptr;
static uint8_t *g_erase_buf = nullptr;

// FAT16 layout
static uint32_t g_total_sectors = 0;
static uint32_t g_part_start_lba = 1;
static uint32_t g_part_sectors = 0;

static uint16_t g_reserved_sectors = 1;
static uint8_t  g_num_fats = 2;
static uint16_t g_root_entries = 128;
static uint16_t g_root_dir_sectors = 0;
static uint16_t g_sectors_per_fat = 0;
static uint8_t  g_sectors_per_cluster = 1;

static uint32_t g_boot_lba = 0;
static uint32_t g_fat1_lba = 0;
static uint32_t g_fat2_lba = 0;
static uint32_t g_root_lba = 0;
static uint32_t g_data_lba = 0;

static constexpr const char README_TEXT[] =
    "ESP32-S3 USB MSC\r\n"
    "Raw onboard flash partition\r\n"
    "MBR + FAT16\r\n";

static uint32_t ceil_div_u32(uint32_t a, uint32_t b) {
  return (a + b - 1) / b;
}

static void wr16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
}

static void wr32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
  p[2] = (uint8_t)((v >> 16) & 0xFF);
  p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static esp_err_t part_read(uint32_t lba, uint32_t offset, void *buf, uint32_t len) {
  uint32_t flash_off = lba * BLOCK_SIZE + offset;
  return esp_partition_read(g_part, flash_off, buf, len);
}

static esp_err_t part_write_any(uint32_t abs_off, const uint8_t *src, uint32_t len) {
  if (!g_erase_buf) return ESP_ERR_NO_MEM;

  while (len > 0) {
    uint32_t sector_base = (abs_off / ERASE_SIZE) * ERASE_SIZE;
    uint32_t in_sector   = abs_off - sector_base;
    uint32_t chunk       = min((uint32_t)(ERASE_SIZE - in_sector), len);

    esp_err_t err = esp_partition_read(g_part, sector_base, g_erase_buf, ERASE_SIZE);
    if (err != ESP_OK) return err;

    memcpy(&g_erase_buf[in_sector], src, chunk);

    err = esp_partition_erase_range(g_part, sector_base, ERASE_SIZE);
    if (err != ESP_OK) return err;

    err = esp_partition_write(g_part, sector_base, g_erase_buf, ERASE_SIZE);
    if (err != ESP_OK) return err;

    abs_off += chunk;
    src += chunk;
    len -= chunk;
  }

  return ESP_OK;
}

static esp_err_t part_write_lba(uint32_t lba, uint32_t offset, const uint8_t *src, uint32_t len) {
  return part_write_any(lba * BLOCK_SIZE + offset, src, len);
}

static void fat16_set_entry(uint8_t *fat, uint16_t cluster, uint16_t value) {
  uint32_t off = cluster * 2;
  fat[off + 0] = (uint8_t)(value & 0xFF);
  fat[off + 1] = (uint8_t)((value >> 8) & 0xFF);
}

static void make_name83(char out[11], const char *name, const char *ext) {
  memset(out, ' ', 11);
  for (int i = 0; i < 8 && name[i]; i++) out[i] = (char)toupper((unsigned char)name[i]);
  for (int i = 0; i < 3 && ext[i]; i++) out[8 + i] = (char)toupper((unsigned char)ext[i]);
}

static void write_dir_entry(uint8_t *e, const char name83[11], uint8_t attr, uint16_t first_cluster, uint32_t size) {
  memset(e, 0, 32);
  memcpy(e + 0, name83, 11);
  e[11] = attr;

  wr16(e + 14, 0x6000);
  wr16(e + 16, 0x5A88);
  wr16(e + 22, 0x6000);
  wr16(e + 24, 0x5A88);

  wr16(e + 26, first_cluster);
  wr32(e + 28, size);
}

static void chooseFatGeometry() {
  g_total_sectors = g_part->size / BLOCK_SIZE;
  g_part_sectors = g_total_sectors - g_part_start_lba;
  g_root_dir_sectors = (g_root_entries * 32 + BLOCK_SIZE - 1) / BLOCK_SIZE;
  g_sectors_per_cluster = 1;

  uint32_t old_spf = 0;
  uint32_t spf = 1;

  while (spf != old_spf) {
    old_spf = spf;
    uint32_t data_sectors = g_part_sectors - g_reserved_sectors - (g_num_fats * spf) - g_root_dir_sectors;
    uint32_t cluster_count = data_sectors / g_sectors_per_cluster;
    spf = ceil_div_u32((cluster_count + 2) * 2, BLOCK_SIZE);
  }

  g_sectors_per_fat = (uint16_t)spf;

  g_boot_lba = g_part_start_lba;
  g_fat1_lba = g_boot_lba + g_reserved_sectors;
  g_fat2_lba = g_fat1_lba + g_sectors_per_fat;
  g_root_lba = g_fat2_lba + g_sectors_per_fat;
  g_data_lba = g_root_lba + g_root_dir_sectors;
}

static esp_err_t buildDiskImage() {
  chooseFatGeometry();

  esp_err_t err = esp_partition_erase_range(g_part, 0, g_part->size);
  if (err != ESP_OK) return err;

  uint8_t sec[BLOCK_SIZE];

  memset(sec, 0, sizeof(sec));
  uint8_t *p = &sec[446];
  p[0] = 0x00;
  p[1] = 0x01; p[2] = 0x01; p[3] = 0x00;
  p[4] = 0x06;
  p[5] = 0xFE; p[6] = 0xFF; p[7] = 0xFF;
  wr32(&p[8], g_part_start_lba);
  wr32(&p[12], g_part_sectors);
  sec[510] = 0x55;
  sec[511] = 0xAA;
  err = part_write_lba(0, 0, sec, BLOCK_SIZE);
  if (err != ESP_OK) return err;

  memset(sec, 0, sizeof(sec));
  sec[0] = 0xEB; sec[1] = 0x3C; sec[2] = 0x90;
  memcpy(&sec[3], "MSDOS5.0", 8);
  wr16(&sec[11], BLOCK_SIZE);
  sec[13] = g_sectors_per_cluster;
  wr16(&sec[14], g_reserved_sectors);
  sec[16] = g_num_fats;
  wr16(&sec[17], g_root_entries);

  if (g_part_sectors < 65536) {
    wr16(&sec[19], (uint16_t)g_part_sectors);
    wr32(&sec[32], 0);
  } else {
    wr16(&sec[19], 0);
    wr32(&sec[32], g_part_sectors);
  }

  sec[21] = 0xF8;
  wr16(&sec[22], g_sectors_per_fat);
  wr16(&sec[24], 63);
  wr16(&sec[26], 255);
  wr32(&sec[28], g_part_start_lba);
  sec[36] = 0x80;
  sec[38] = 0x29;
  wr32(&sec[39], 0x20260408);
  memcpy(&sec[43], "ESP32S3MSC ", 11);
  memcpy(&sec[54], "FAT16   ", 8);
  sec[510] = 0x55;
  sec[511] = 0xAA;
  err = part_write_lba(g_boot_lba, 0, sec, BLOCK_SIZE);
  if (err != ESP_OK) return err;

  uint8_t fat_sector[BLOCK_SIZE];
  memset(fat_sector, 0, sizeof(fat_sector));
  fat_sector[0] = 0xF8;
  fat_sector[1] = 0xFF;
  fat_sector[2] = 0xFF;
  fat_sector[3] = 0xFF;
  fat16_set_entry(fat_sector, 2, 0xFFFF);

  err = part_write_lba(g_fat1_lba, 0, fat_sector, BLOCK_SIZE);
  if (err != ESP_OK) return err;
  err = part_write_lba(g_fat2_lba, 0, fat_sector, BLOCK_SIZE);
  if (err != ESP_OK) return err;

  for (uint16_t i = 1; i < g_sectors_per_fat; i++) {
    memset(fat_sector, 0, sizeof(fat_sector));
    err = part_write_lba(g_fat1_lba + i, 0, fat_sector, BLOCK_SIZE);
    if (err != ESP_OK) return err;
    err = part_write_lba(g_fat2_lba + i, 0, fat_sector, BLOCK_SIZE);
    if (err != ESP_OK) return err;
  }

  memset(sec, 0, sizeof(sec));
  char vol83[11] = {'E','S','P','3','2','S','3','M','S','C',' '};
  memcpy(&sec[0], vol83, 11);
  sec[11] = 0x08;
  err = part_write_lba(g_root_lba, 0, sec, BLOCK_SIZE);
  if (err != ESP_OK) return err;

  memset(sec, 0, sizeof(sec));
  char name83[11];
  make_name83(name83, "README", "TXT");
  write_dir_entry(&sec[0], name83, 0x20, 2, sizeof(README_TEXT) - 1);
  err = part_write_lba(g_root_lba, 0, sec, BLOCK_SIZE);
  if (err != ESP_OK) return err;

  memset(sec, 0, sizeof(sec));
  memcpy(sec, README_TEXT, sizeof(README_TEXT) - 1);
  err = part_write_lba(g_data_lba, 0, sec, BLOCK_SIZE);
  if (err != ESP_OK) return err;

  return ESP_OK;
}

static bool diskLooksFormatted() {
  uint8_t sec[BLOCK_SIZE];
  if (part_read(0, 0, sec, BLOCK_SIZE) != ESP_OK) return false;
  if (sec[510] != 0x55 || sec[511] != 0xAA) return false;

  uint8_t bs[BLOCK_SIZE];
  if (part_read(g_part_start_lba, 0, bs, BLOCK_SIZE) != ESP_OK) return false;
  if (bs[510] != 0x55 || bs[511] != 0xAA) return false;
  if (memcmp(&bs[54], "FAT16   ", 8) != 0) return false;

  return true;
}

static int32_t onRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
  if (!g_part) return -1;
  uint32_t addr = lba * BLOCK_SIZE + offset;
  if (addr + bufsize > g_part->size) return -1;
  return (part_read(lba, offset, buffer, bufsize) == ESP_OK) ? (int32_t)bufsize : -1;
}

static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
  if (!g_part) return -1;
  uint32_t addr = lba * BLOCK_SIZE + offset;
  if (addr + bufsize > g_part->size) return -1;
  return (part_write_lba(lba, offset, buffer, bufsize) == ESP_OK) ? (int32_t)bufsize : -1;
}

static bool onStartStop(uint8_t power_condition, bool start, bool load_eject) {
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.println();
  Serial.println("ESP32-S3 raw-flash MSC");

  g_erase_buf = (uint8_t *)malloc(ERASE_SIZE);
  if (!g_erase_buf) {
    Serial.println("Failed to allocate erase buffer");
    while (true) delay(1000);
  }

  g_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, PART_LABEL);
  if (!g_part) {
    Serial.println("Partition 'mscraw' not found");
    while (true) delay(1000);
  }

  chooseFatGeometry();

  if (!diskLooksFormatted()) {
    Serial.println("Formatting raw partition as MBR + FAT16...");
    esp_err_t err = buildDiskImage();
    if (err != ESP_OK) {
      Serial.printf("buildDiskImage failed: 0x%08x\n", (unsigned)err);
      while (true) delay(1000);
    }
  }

  uint32_t block_count = g_part->size / BLOCK_SIZE;

  MSC.vendorID("ESP32");
  MSC.productID("RAW_FLASH");
  MSC.productRevision("1.0");
  MSC.onRead(onRead);
  MSC.onWrite(onWrite);
  MSC.onStartStop(onStartStop);
  MSC.mediaPresent(true);

  if (!MSC.begin(block_count, BLOCK_SIZE)) {
    Serial.println("MSC.begin failed");
    while (true) delay(1000);
  }

  USB.begin();

  Serial.printf("MSC ready: %u blocks x %u bytes\n", (unsigned)block_count, (unsigned)BLOCK_SIZE);
  Serial.println("Use the board port labeled USB.");
}

void loop() {
  delay(1000);
  Serial.println("S3-OTG: Hardware-backed FAT16 is LIVE! 🚀");
}
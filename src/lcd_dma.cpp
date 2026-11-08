#include "lcd_dma.h"
#include <Arduino.h>
#include <atomic>
#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>

namespace {
esp_lcd_panel_io_handle_t io = nullptr;
std::atomic<int> inflight{0};

bool IRAM_ATTR transferDone(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void* ctx) {
    inflight.fetch_sub(1);
    lv_display_flush_ready(static_cast<lv_display_t*>(ctx));
    return false;
}
}

bool lcdDmaBegin(lv_display_t* display, size_t maxTransferBytes) {
    spi_bus_config_t bus{};
    bus.mosi_io_num = TFT_MOSI;
    bus.miso_io_num = -1;
    bus.sclk_io_num = TFT_SCLK;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = int(maxTransferBytes) + 64;
    // SPI2 (FSPI) is the native IOMUX host for GPIO 10-13 on the ESP32-S3.
    if (spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) return false;
    esp_lcd_panel_io_spi_config_t config{};
    config.cs_gpio_num = TFT_CS;
    config.dc_gpio_num = TFT_DC;
    config.spi_mode = 0;
    config.pclk_hz = SPI_FREQUENCY;
    config.trans_queue_depth = 4;
    config.on_color_trans_done = transferDone;
    config.user_ctx = display;
    config.lcd_cmd_bits = 8;
    config.lcd_param_bits = 8;
    if (esp_lcd_new_panel_io_spi(reinterpret_cast<esp_lcd_spi_bus_handle_t>(SPI2_HOST), &config, &io) != ESP_OK) {
        spi_bus_free(SPI2_HOST);
        io = nullptr;
        return false;
    }
    return true;
}

void lcdDmaFlush(lv_display_t*, const lv_area_t* area, uint8_t* pixels) {
    const uint32_t count = uint32_t(area->x2 - area->x1 + 1) * uint32_t(area->y2 - area->y1 + 1);
    // LVGL renders RGB565_SWAPPED (big-endian), exactly what the panel expects.
    // tx_param waits for the previous strip's DMA, so the window never changes mid-transfer.
    const uint8_t columns[] = {uint8_t(area->x1 >> 8), uint8_t(area->x1), uint8_t(area->x2 >> 8), uint8_t(area->x2)};
    const uint8_t rows[] = {uint8_t(area->y1 >> 8), uint8_t(area->y1), uint8_t(area->y2 >> 8), uint8_t(area->y2)};
    esp_lcd_panel_io_tx_param(io, 0x2A, columns, 4);
    esp_lcd_panel_io_tx_param(io, 0x2B, rows, 4);
    inflight.fetch_add(1);
    // Returns once queued; transferDone() reports the buffer free to LVGL.
    esp_lcd_panel_io_tx_color(io, 0x2C, pixels, count * 2);
}

void lcdDmaSetMadctl(uint8_t value) {
    if (!io) return;
    esp_lcd_panel_io_tx_param(io, 0x36, &value, 1);
}

void lcdDmaWait() {
    while (inflight.load() > 0) delay(1);
}

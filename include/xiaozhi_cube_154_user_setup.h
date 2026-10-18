// PotsSquad-CYD -- TFT_eSPI user setup for the 1.54" Xiaozhi-style cube
// (the board ComPiXi ships as main/boards/compixi/compixi-cube-154).
//
// ESP32-S3 (N16R8: 16 MB flash, 8 MB OPI PSRAM), a 1.54" 240x240 ST7789 on
// SPI, no touch, three keys (BOOT 0, VOL+ 40, VOL- 39) and a power rail
// (GPIO21) the board keeps high itself. The pins match ComPiXi's board
// config.h so one device can be flashed by either firmware without a pin map
// living in two places.
//
// No audio here yet: this build is the button UI (SQW_SMALL's cursor), the
// same screens the StickS3 drives.
#pragma once

#define USER_SETUP_INFO    "PotsSquad-CYD / Xiaozhi cube 1.54 / ST7789 240x240"
#define ST7789_DRIVER
#define TFT_WIDTH   240
#define TFT_HEIGHT  240

#define TFT_MISO  -1
#define TFT_MOSI  10
#define TFT_SCLK  9
#define TFT_CS    14
#define TFT_DC    8
#define TFT_RST   18
// GPIO13 is the backlight, run by main.cpp's LEDC dimmer.

#define TFT_INVERSION_ON

#ifndef SPI_FREQUENCY
#define SPI_FREQUENCY         40000000
#endif
#define SPI_READ_FREQUENCY    16000000

#define LOAD_GLCD
#define LOAD_FONT2
// PotSquad: the GFX free-font renderer, for the Russian face (RuCyr8 in
// include/ru_font.h, drawn via Theme::printRU). Types only (gfxfont.h)
// plus the renderer -- no bundled font data, ~1 KB of our own glyphs.
// The font-1 path with no free font set is byte-identical with or without
// this, so EN rendering does not move a pixel.
#define LOAD_GFXFF
#define LOAD_FONT4
#define SMOOTH_FONT

// Audio (POTS_VOICE): the same NoAudioCodecSimplex wiring as ComPiXi's board —
// the mic on one I2S and the speaker on another, no codec chip. Kept here so a
// second firmware flashing this hardware reads the pins from one place.
#define VOICE_MIC_SCK   5
#define VOICE_MIC_WS    4
#define VOICE_MIC_DIN   6
#define VOICE_SPK_BCLK  15
#define VOICE_SPK_WS    16
#define VOICE_SPK_DOUT  7

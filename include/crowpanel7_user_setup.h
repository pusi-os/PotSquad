// PotsSquad-CYD — TFT_eSPI "user setup" for the CrowPanel 7, which describes a
// display that does not exist.
//
// This board has no SPI display bus. TFT_eSPI is kept on this board purely as
// a TYPE — the app upcasts its off-screen sprite to TFT_eSPI* (src/main.cpp's
// `canvas`), so the sprite and the device must share a base class, and only
// TFT_eSPI's hierarchy gives that. CrowPanelTFT (crowpanel7_display.h) hides
// every method that would reach a bus, and ESP-IDF's esp_lcd RGB driver drives
// the actual panel underneath. tft.init() is never allowed to run, so no initialisation
// sequence is ever emitted and none of the pins below are ever touched.
//
// It exists only to satisfy USER_SETUP_LOADED and to pick a processor header.
#pragma once

#define USER_SETUP_INFO "PotsSquad / CrowPanel 7 / RGB parallel via esp_lcd"

// A driver has to be named for the library to compile; nothing is sent to it.
#define ST7789_DRIVER
#define TFT_WIDTH   800
#define TFT_HEIGHT  480

#define TFT_MISO  -1
#define TFT_MOSI  -1
#define TFT_SCLK  -1
#define TFT_CS    -1
#define TFT_DC    -1
#define TFT_RST   -1

// The fonts the app actually loads. Keep in step with the other boards'
// setups: the GLCD 6x8 cell and font 2 are what theme.cpp measures against.
#define LOAD_GLCD
#define LOAD_FONT2
// PotSquad: the GFX free-font renderer, for the Russian face (RuCyr8 in
// include/ru_font.h, drawn via Theme::printRU). Types only (gfxfont.h)
// plus the renderer -- no bundled font data, ~1 KB of our own glyphs.
// The font-1 path with no free font set is byte-identical with or without
// this, so EN rendering does not move a pixel.
#define LOAD_GFXFF

#ifndef SPI_FREQUENCY
#define SPI_FREQUENCY 20000000
#endif

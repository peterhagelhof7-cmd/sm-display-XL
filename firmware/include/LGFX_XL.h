#pragma once
// LovyanGFX-Konfiguration fuer das ESP32-8048S070C (Sunton 7" 800x480 RGB-Parallel
// + GT911 Kapazitiv-Touch). Bus-Pins/Timings/Touch = LovyanGFX-Referenzconfig
// LGFX_Sunton_ESP32-8048S070.h (die LovyanGFX-Bus-Bit-Reihenfolge d0..d15 ist
// bewusst anders als die ESP-IDF-data_gpio-Reihenfolge - NICHT von Hand
// "korrigieren"). Hintergrundbeleuchtung (GPIO2) macht der BacklightManager per
// LEDC-PWM selbst -> hier KEINE LovyanGFX-Light (kein Doppelzugriff). Touch nutzt
// KEIN INT-Pin (GPIO_NUM_NC) -> GPIO18 bleibt frei fuer den DHT.
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <driver/i2c.h>

class LGFX_XL : public lgfx::LGFX_Device {
	lgfx::Bus_RGB     _bus;
	lgfx::Panel_RGB   _panel;
	lgfx::Touch_GT911 _touch;

public:
	LGFX_XL() {
		{
			auto cfg = _panel.config();
			cfg.memory_width  = 800; cfg.memory_height = 480;
			cfg.panel_width   = 800; cfg.panel_height  = 480;
			cfg.offset_x = 0;        cfg.offset_y = 0;
			_panel.config(cfg);
		}
		{
			auto cfg = _panel.config_detail();
			cfg.use_psram = 1;   // Framebuffer ins PSRAM
			_panel.config_detail(cfg);
		}
		{
			auto cfg = _bus.config();
			cfg.panel = &_panel;
			cfg.pin_d0  = GPIO_NUM_15; cfg.pin_d1  = GPIO_NUM_7;  cfg.pin_d2  = GPIO_NUM_6;  cfg.pin_d3  = GPIO_NUM_5;  cfg.pin_d4  = GPIO_NUM_4;   // B
			cfg.pin_d5  = GPIO_NUM_9;  cfg.pin_d6  = GPIO_NUM_46; cfg.pin_d7  = GPIO_NUM_3;  cfg.pin_d8  = GPIO_NUM_8;  cfg.pin_d9  = GPIO_NUM_16; cfg.pin_d10 = GPIO_NUM_1;   // G
			cfg.pin_d11 = GPIO_NUM_14; cfg.pin_d12 = GPIO_NUM_21; cfg.pin_d13 = GPIO_NUM_47; cfg.pin_d14 = GPIO_NUM_48; cfg.pin_d15 = GPIO_NUM_45;  // R
			cfg.pin_henable = GPIO_NUM_41;
			cfg.pin_vsync   = GPIO_NUM_40;
			cfg.pin_hsync   = GPIO_NUM_39;
			cfg.pin_pclk    = GPIO_NUM_42;
			cfg.freq_write  = 14000000;
			cfg.hsync_polarity = 0; cfg.hsync_front_porch = 80; cfg.hsync_pulse_width = 4; cfg.hsync_back_porch = 16;
			cfg.vsync_polarity = 0; cfg.vsync_front_porch = 22; cfg.vsync_pulse_width = 4; cfg.vsync_back_porch = 4;
			cfg.pclk_idle_high = 1;
			_bus.config(cfg);
		}
		_panel.setBus(&_bus);
		{
			auto cfg = _touch.config();
			cfg.x_min = 0; cfg.x_max = 800;
			cfg.y_min = 0; cfg.y_max = 480;
			cfg.pin_int  = GPIO_NUM_NC;   // Polling -> GPIO18 bleibt frei fuer DHT
			cfg.pin_rst  = GPIO_NUM_38;
			cfg.bus_shared = false;
			cfg.offset_rotation = 0;
			cfg.i2c_port = I2C_NUM_1;
			cfg.pin_sda  = GPIO_NUM_19;
			cfg.pin_scl  = GPIO_NUM_20;
			cfg.freq     = 400000;
			cfg.i2c_addr = 0x14;   // GT911; alternativ 0x5D
			_touch.config(cfg);
			_panel.setTouch(&_touch);
		}
		setPanel(&_panel);
	}
};

// Kompat-Alias: die portierten Views nutzen weiterhin `TFT_eSPI &tft = ...`.
using TFT_eSPI = LGFX_XL;

#pragma once

// ========= LCD (LovyanGFX) =========
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// ========= Touch (bb_captouch) =========
#include <Wire.h>
#include <bb_captouch.h>

class LGFX_2432S022 : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789  _panel;
  lgfx::Bus_Parallel8 _bus;
  lgfx::Light_PWM     _light;

public:
  LGFX_2432S022() {
    { // bus parallel 8-bit
      auto cfg = _bus.config();
      cfg.freq_write = 25000000;

      cfg.pin_wr = 4;
      cfg.pin_rd = 2;
      cfg.pin_rs = 16;      // DC/RS

      cfg.pin_d0 = 15;
      cfg.pin_d1 = 13;
      cfg.pin_d2 = 12;
      cfg.pin_d3 = 14;
      cfg.pin_d4 = 27;
      cfg.pin_d5 = 25;
      cfg.pin_d6 = 33;
      cfg.pin_d7 = 32;

      _bus.config(cfg);
      _panel.setBus(&_bus);
    }

    { // panel ST7789 240x320
      auto cfg = _panel.config();
      cfg.pin_cs   = 17;
      cfg.pin_rst  = -1;   // reset global
      cfg.pin_busy = -1;

      cfg.panel_width  = 240;
      cfg.panel_height = 320;

      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;

      cfg.readable   = false;
      cfg.invert     = false;
      cfg.rgb_order  = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = true;

      _panel.config(cfg);
    }

    { // backlight IO0
      auto cfg = _light.config();
      cfg.pin_bl = 0;        // IO0
      cfg.invert = false;
      cfg.freq = 44100;
      cfg.pwm_channel = 7;
      _light.config(cfg);
      _panel.light(&_light);
    }

    setPanel(&_panel);
  }
};

// ---- Instances globales (à utiliser dans ton .ino) ----
static LGFX_2432S022 lcd;

static BBCapTouch g_touch;
static TOUCHINFO  g_ti;

// Réglages validés chez toi :
static constexpr int ROT = 1;
static constexpr bool FLIP_X = true;
static constexpr bool FLIP_Y = true;

// Dimensions "brutes" habituelles du touch
static constexpr int RAW_W = 240;
static constexpr int RAW_H = 320;

static inline uint16_t clampU16(int v, int lo, int hi) {
  if (v < lo) return (uint16_t)lo;
  if (v > hi) return (uint16_t)hi;
  return (uint16_t)v;
}

static inline void mapTouchToScreen(uint16_t rx, uint16_t ry, uint16_t &x, uint16_t &y) {
  int sx = 0, sy = 0;

  switch (ROT & 3) {
    case 0: sx = rx;                 sy = ry;                 break;
    case 1: sx = (RAW_H - 1) - ry;   sy = rx;                 break;
    case 2: sx = (RAW_W - 1) - rx;   sy = (RAW_H - 1) - ry;   break;
    case 3: sx = ry;                 sy = (RAW_W - 1) - rx;   break;
  }

  int w = lcd.width();
  int h = lcd.height();

  uint16_t xx = clampU16(sx, 0, w - 1);
  uint16_t yy = clampU16(sy, 0, h - 1);

  if (FLIP_X) xx = (uint16_t)((w - 1) - xx);
  if (FLIP_Y) yy = (uint16_t)((h - 1) - yy);

  x = xx;
  y = yy;
}

// Call au démarrage
static inline void boardInit_2432S022() {
  lcd.init();
  lcd.setRotation(ROT);
  lcd.setBrightness(255);

  Wire.begin(21, 22);
  Wire.setClock(400000);

  // INT/RST souvent non câblés => -1
  g_touch.init(21, 22, -1, -1);
}

// Lecture touch “simple”
static inline bool boardTouch_2432S022(uint16_t &x, uint16_t &y) {
  int n = g_touch.getSamples(&g_ti);
  if (n > 0 && g_ti.count > 0) {
    mapTouchToScreen(g_ti.x[0], g_ti.y[0], x, y);
    return true;
  }
  return false;
}

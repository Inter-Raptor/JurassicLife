// tamadinov4.ino - ESP32 WROOM + CYD 2432S028 (ILI9341) + LovyanGFX
#include <stdint.h>

// ================== CONFIG RAPIDE (à modifier en premier) ==================
// --- Choix carte/écran ---
#define DISPLAY_PROFILE_2432S022 1   // ST7789 + cap touch I2C
#define DISPLAY_PROFILE_2432S028 2   // ILI9341 + XPT2046 (soft-SPI)
#define DISPLAY_PROFILE_ILI9341_320x240 3 // ILI9341 320x240 + XPT2046 (soft-SPI)

// >>> Réglage simple : modifier la carte ici <<<
#define DISPLAY_PROFILE DISPLAY_PROFILE_2432S022   // DISPLAY_PROFILE_2432S022   ou   DISPLAY_PROFILE_2432S028   ou   DISPLAY_PROFILE_ILI9341_320x240

// >>> Audio : 1 = ON, 0 = OFF <<<
#define ENABLE_AUDIO 1

// >>> Calibration tactile XPT2046 : 1 = ON, 0 = OFF <<<
// Par défaut : 2432S022 = OFF (garde le réglage de base), autres = ON.
#if DISPLAY_PROFILE == DISPLAY_PROFILE_2432S022
  #define ENABLE_TOUCH_CALIBRATION 0
#else
  #define ENABLE_TOUCH_CALIBRATION 1
#endif

// --- Options entrée (2432S028 / ILI9341 uniquement - pas assez de pins sur 2432S022) ---
// Encoder rotatif cliquable (A/B + bouton), ou 3 boutons (gauche/droite/OK).
// Laisser à -1 pour désactiver.
#if DISPLAY_PROFILE == DISPLAY_PROFILE_2432S022
  static const int ENC_A   = -1;
  static const int ENC_B   = -1;
  static const int ENC_BTN = -1;

  static const int BTN_LEFT  = -1;
  static const int BTN_RIGHT = -1;
  static const int BTN_OK    = -1;
#elif DISPLAY_PROFILE == DISPLAY_PROFILE_2432S028
  static const int ENC_A   = 22;   // A=22
  static const int ENC_B   = 27;   // B=27
  static const int ENC_BTN = 35;   // BTN=35 (ou -1 pour désactiver)

  static const int BTN_LEFT  = -1;
  static const int BTN_RIGHT = -1;
  static const int BTN_OK    = -1;
#else // DISPLAY_PROFILE_ILI9341_320x240 (ESP32 classic avec écran)
  static const int ENC_A   = 22;   // pins de base encoder
  static const int ENC_B   = 27;
  static const int ENC_BTN = 35;

  // Option 3 boutons : mettre ENC_* à -1 et définir BTN_* (pins de base à adapter)
  static const int BTN_LEFT  = -1;
  static const int BTN_RIGHT = -1;
  static const int BTN_OK    = -1;
#endif

// --- Pins utilisés (référence rapide) ---
// 2432S022 (ST7789 + cap touch I2C)
//   LCD parallèle 8-bit: WR=4, RD=2, RS=16, D0=15, D1=13, D2=12, D3=14, D4=27, D5=25, D6=33, D7=32
//   LCD CS=17, RST=-1, BL=0 (PWM)
//   Touch I2C: SDA=21, SCL=22
// 2432S028 (ILI9341 + XPT2046)
//   LCD SPI: SCLK=14, MOSI=13, MISO=12, DC=2, CS=15, BL=21
//   Touch XPT2046 (soft-SPI): CLK=25, MOSI=32, MISO=39, CS=33, IRQ=36
// ILI9341_320x240 (TFT classique + XPT2046)
//   LCD SPI: SCLK=14, MOSI=13, MISO=12, DC=2, CS=15, BL=21
//   Touch XPT2046 (soft-SPI): CLK=25, MOSI=32, MISO=39, CS=33, IRQ=36
// Entrées (optionnelles): encoder = ENC_A/ENC_B/ENC_BTN, boutons = BTN_LEFT/BTN_RIGHT/BTN_OK
// SD (toutes cartes): SCK=18, MISO=19, MOSI=23, CS=5// SD (par carte)
// 2432S022: SCK=18, MISO=19, MOSI=23, CS=5
// 2432S028: SCK=18, MISO=19, MOSI=23, CS=5
// ILI9341_320x240: à adapter si besoin
// ================== ENUMS (Arduino prototype fix) ==================
enum AgeStage : uint8_t { AGE_JUNIOR, AGE_ADULTE, AGE_SENIOR };

enum UiAction : uint8_t {
  UI_REPOS, UI_MANGER, UI_BOIRE, UI_LAVER, UI_JOUER, UI_CACA, UI_CALIN, UI_AUDIO,
  UI_COUNT
};

enum TriState : uint8_t { ST_WALK, ST_JUMP, ST_SIT, ST_BLINK, ST_EAT, ST_SLEEP, ST_DEAD };

enum TaskKind  : uint8_t { TASK_NONE, TASK_SLEEP, TASK_EAT, TASK_DRINK, TASK_WASH, TASK_PLAY, TASK_POOP, TASK_HUG };
enum TaskPhase : uint8_t { PH_GO, PH_DO, PH_RETURN };

enum GamePhase : uint8_t { PHASE_EGG, PHASE_HATCHING, PHASE_ALIVE, PHASE_RESTREADY, PHASE_TOMB };

enum AudioMode : uint8_t { AUDIO_OFF, AUDIO_LIMITED, AUDIO_TOTAL };
enum AudioPriority : uint8_t { AUDIO_PRIO_LOW, AUDIO_PRIO_MED, AUDIO_PRIO_HIGH };
struct AudioStep;

// ================== APP MODE (gestion vs mini-jeux) ==================
enum AppMode : uint8_t { MODE_PET, MODE_MG_WASH, MODE_MG_PLAY };
static AppMode appMode = MODE_PET;

struct MiniGameCtx {
  bool active = false;
  TaskKind kind = TASK_NONE;   // TASK_WASH ou TASK_PLAY
  uint32_t startedAt = 0;
  bool success = true;
  int score = 0;
};
static MiniGameCtx mg;

struct RainDrop {
  float x = 0;
  float y = 0;
  float vy = 0;
  bool active = false;
  float prevX = 0;
  float prevY = 0;
  bool prevActive = false;
};

struct Balloon {
  float x = 0;
  float y = 0;
  float vx = 0;
  bool active = false;
  float prevX = 0;
  float prevY = 0;
  bool prevActive = false;
};

static const int MG_RAIN_MAX = 28;
static const int MG_BALLOON_MAX = 18;
static RainDrop mgRain[MG_RAIN_MAX];
static Balloon mgBalloons[MG_BALLOON_MAX];

static float mgCloudX = 0.0f;
static float mgCloudV = 0.9f;
static float mgDinoX = 0.0f;
static float mgDinoV = 1.2f;
static int mgDropsHit = 0;

static float mgPlayDinoY = 0.0f;
static float mgPlayDinoVy = 0.0f;
static int mgBalloonsCaught = 0;
static int mgBalloonsSpawned = 0;
static uint32_t mgNextBalloonAt = 0;
static bool mgJumpRequested = false;
static uint32_t mgNextDropAt = 0;   // spawn pluie basé sur le temps (wash)
static int32_t mgLastDetent = 0;
static bool mgNeedsFullRedraw = true;
static float mgPrevCloudX = 0.0f;
static float mgPrevDinoX = 0.0f;
static int mgPrevWashDinoY = 0;
static int mgPrevPlayDinoTop = 0;
static uint8_t mgAnimIdx = 0;
static uint32_t mgAnimNextTick = 0;
static const uint16_t MG_SKY    = 0x6D9F; // bleu ciel (plus visible)
static const uint16_t MG_GROUND = 0x07E0; // vert herbe (bien vert)
static const uint16_t MG_GLINE  = 0x05E0; // vert plus foncé (ligne du sol)

// Protos (si besoin)
static inline uint16_t btnColorForAction(UiAction a);
static inline const char* btnLabel(UiAction a);
static inline const char* stageLabel(AgeStage s);
static void enterState(TriState st, uint32_t now);
static bool startTask(TaskKind k, uint32_t now);
static void resetToEgg(uint32_t now);
static void handleDeath(uint32_t now);
static void eraseSavesAndRestart();

// AJOUT (tactile) : prototypes pour éviter tout souci d'auto-prototypes Arduino
static void uiPressAction(uint32_t now);
static void handleTouchUI(uint32_t now);

// Mini-jeux
static void mgBegin(TaskKind k, uint32_t now);
static bool mgUpdate(uint32_t now);
static void mgDraw(uint32_t now);

// ================== TYPES (tactile soft-SPI) ==================
struct TouchSample { uint16_t x, y, z; bool valid; };
struct TouchAffine {
  float a,b,c;
  float d,e,f;
  bool ok;
  bool skipped; // true si l'utilisateur a choisi de passer la calibration
};

struct ButtonDeb;
extern bool sdReady;

// ================== INCLUDES ==================
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <ctype.h>
#include <math.h>
#include <string.h>

#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>

#include "DinoNames.h"
#include "JurassicMusicRTTTL.h"
#include "pageaccueil.h"

// ================== ASSETS (namespaces rename) ==================
#define triceratops triJ
#include "annim_junior.h"
#undef triceratops

#define triceratops triA
#include "annim_adul.h"
#undef triceratops

#define triceratops triS
#include "annim_senior.h"
#undef triceratops

#define dino egg
#include "annim_oeuf.h"
#undef dino

#define dino poop
#include "annim_caca.h"
#undef dino

#include "tombe.h"

// Décor
#include "montagne.h"
#include "sapin.h"
#include "arbre.h"
#include "buissonbaie.h"
#include "buissonsansbaie.h"
#include "flaque_eau_60x25.h"
#include "nuage.h"
#include "ballon.h"

#ifndef TFT_BLACK
  #define TFT_BLACK 0x0000
  #define TFT_WHITE 0xFFFF
  #define TFT_RED   0xF800
  #define TFT_GREEN 0x07E0
  #define TFT_BLUE  0x001F
  #define TFT_YELLOW 0xFFE0
#endif

// ================== CONFIG ==================
// Affichage boutons type "tamadinov30test" (Manger/Boire en bas) ou boutons rapides en haut
#define USE_TOP_QUICK_BUTTONS (DISPLAY_PROFILE == DISPLAY_PROFILE_2432S022)

// Rotation écran (0..3). Pour un retournement 180°, ajouter 2 (ex: 1 -> 3).
static constexpr int ROT = 3;
static constexpr bool FLIP_X = true;
static constexpr bool FLIP_Y = true;

static const int   SIM_SPEED = 1;


// vitesse *1.5 (tu as demandé)
static const float SPEED_MULT = 1.5f;
static const int   SPEED_BASE_PX = 2;

static const int   HOME_X_MODE   = 0;

// dino “pied au sol”
static const int   TRI_FOOT_Y = 45;
static const bool  TRI_FACES_LEFT = true;

// décor
static int BUSH_Y_OFFSET   = 20;
static int PUDDLE_Y_OFFSET = 26;

// === Ajustement facile des “spots” (calibrage) ===
static const int EAT_SPOT_OFFSET   = 0;
static const int DRINK_SPOT_OFFSET = 0;  // <- valeur “par défaut” selon tes retours

// saut (encore présent mais peu utilisé)
static int JUMP_V0_MIN = 2;
static int JUMP_V0_MAX = 4;
static const float GRAVITY = 0.5f;

// évolution  base 80% pendant 80min pour augmenter d'age!
static const int EVOLVE_THR = 85;
static const int EVOLVE_JUNIOR_TO_ADULT_MIN = 1200;
static const int EVOLVE_ADULT_TO_SENIOR_MIN = 1200;
static const int EVOLVE_SENIOR_TO_REST_MIN  = 600;

// bonus âge
static const float DUR_MUL_JUNIOR = 1.00f;
static const float DUR_MUL_ADULTE = 0.85f;
static const float DUR_MUL_SENIOR = 0.75f;

static const float GAIN_MUL_JUNIOR = 1.00f;
static const float GAIN_MUL_ADULTE = 1.15f;
static const float GAIN_MUL_SENIOR = 1.10f;

// tick réveillé (par minute)
static const float AWAKE_HUNGER_D  = -2.0f;
static const float AWAKE_THIRST_D  = -3.0f;
static const float AWAKE_HYGIENE_D = -1.0f;
static const float AWAKE_MOOD_D    = -1.0f;
static const float AWAKE_ENERGY_D  = -2.0f;
static const float AWAKE_FATIGUE_D = -2.0f;
static const float AWAKE_LOVE_D    = -0.5f;

// caca
static const float AWAKE_POOP_D    = +1.0f;
static const float SLEEP_POOP_D    = +0.5f;
static const int   POOP_STRESS_THR = 80;

// durées base actions (toujours utiles pour balancing même si wash/play passent en mini-jeu)
static const uint32_t BASE_EAT_MS   = 20000;
static const uint32_t BASE_DRINK_MS = 15000;
static const uint32_t BASE_WASH_MS  = 15000;
static const uint32_t BASE_PLAY_MS  = 25000;
static const uint32_t BASE_POOP_MS  = 12000;
static const uint32_t BASE_HUG_MS   = 15000;

// cooldowns
static const uint32_t CD_EAT_MS   = 3UL  * 60UL * 1000UL;
static const uint32_t CD_DRINK_MS = 1UL  * 60UL * 1000UL;
static const uint32_t CD_WASH_MS  = 90UL * 1000UL;
static const uint32_t CD_PLAY_MS  = 120UL * 1000UL;
static const uint32_t CD_POOP_MS  = 90UL * 1000UL;
static const uint32_t CD_HUG_MS   = 1UL  * 60UL * 1000UL;

// effets actions (avant gain mul)
static const float EAT_HUNGER   = +30;
static const float EAT_ENERGY   = +5;
static const float EAT_FATIGUE  = -5;
static const float EAT_THIRST   = -5;
static const float EAT_POOP     = +25;

static const float DRINK_THIRST = +35;
static const float DRINK_ENERGY = +2;
static const float DRINK_FATIGUE= 0;
static const float DRINK_POOP   = +5;

static const float WASH_HYGIENE = +40;
static const float WASH_MOOD    = +5;
static const float WASH_ENERGY  = -5;
static const float WASH_FATIGUE = -5;

static const float PLAY_MOOD    = +25;
static const float PLAY_ENERGY  = -20;
static const float PLAY_FATIGUE = -25;
static const float PLAY_HUNGER  = -10;
static const float PLAY_THIRST  = -10;

static const float POOP_SET     = 0;
static const float POOP_HYGIENE = -15;
static const float POOP_MOOD    = +2;
static const float POOP_ENERGY  = -3;
static const float POOP_FATIGUE = -3;

static const float HUG_LOVE     = +25;
static const float HUG_MOOD     = +5;
static const float HUG_FATIGUE  = +2;
static const float HUG_ENERGY   = -2;

static const float SLEEP_GAIN_PER_SEC = 0.1f;
static const int   POOP_ACCIDENT_AT = 100;

// ================== TRANSPARENCE KEY ==================
static inline uint16_t swap16(uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); }
static const uint16_t KEY      = 0xF81F;
static const uint16_t KEY_SWAP = 0x1FF8;

// ================== ALIAS décor ==================
#define MONT_W    montagne_W
#define MONT_H    montagne_H
#define MONT_IMG  montagne

#define SAPIN_W   sapin_W
#define SAPIN_H   sapin_H
#define SAPIN_IMG sapin

#define ARBRE_W   arbre_W
#define ARBRE_H   arbre_H
#define ARBRE_IMG arbre

#define BBAIE_W   buissonbaie_W
#define BBAIE_H   buissonbaie_H
#define BBAIE_IMG buissonbaie

#define BSANS_W   buissonsansbaie_W
#define BSANS_H   buissonsansbaie_H
#define BSANS_IMG buissonsansbaie

#define FLAQUE_W   flaque_eau_60x25_W
#define FLAQUE_H   flaque_eau_60x25_H
#define FLAQUE_IMG flaque_eau_60x25

// ================== ÉCRAN ==================
#if DISPLAY_PROFILE == DISPLAY_PROFILE_2432S022
#include <Wire.h>
#include <bb_captouch.h>

static constexpr int TOUCH_SDA = 21;
static constexpr int TOUCH_SCL = 22;

// --- LovyanGFX : ST7789 + bus parallèle 8-bit ---
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789  _panel;
  lgfx::Bus_Parallel8 _bus;
  lgfx::Light_PWM     _light;

public:
  LGFX() {
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

    { // panel
      auto cfg = _panel.config();
      cfg.pin_cs   = 17;
      cfg.pin_rst  = -1;
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

    { // backlight : IO0
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

LGFX tft;

// --- Touch I2C (cap touch) ---
static BBCapTouch g_touch;
static TOUCHINFO  g_ti;

// dimensions RAW habituelles touch (avant rotation)
static constexpr int RAW_W = 240;
static constexpr int RAW_H = 320;

#else
// --- ILI9341 SPI (2432S028 ou TFT classique 320x240) ---
static constexpr int TOUCH_IRQ  = 36; // input-only
static constexpr int TOUCH_MOSI = 32;
static constexpr int TOUCH_MISO = 39;
static constexpr int TOUCH_CLK  = 25;
static constexpr int TOUCH_CS   = 33;

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 _panel;
  lgfx::Bus_SPI _bus;
public:
  LGFX() {
    { auto cfg = _bus.config();
      cfg.spi_host   = VSPI_HOST;
      cfg.spi_mode   = 0;
      cfg.freq_write = 20000000;
      cfg.freq_read  = 16000000;
      cfg.pin_sclk   = 14;
      cfg.pin_mosi   = 13;
      cfg.pin_miso   = 12;
      cfg.pin_dc     = 2;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    { auto cfg = _panel.config();
      cfg.pin_cs   = 15;
      cfg.pin_rst  = -1;
      cfg.pin_busy = -1;
#if DISPLAY_PROFILE == DISPLAY_PROFILE_ILI9341_320x240
      cfg.panel_width  = 320;
      cfg.panel_height = 240;
#else
      cfg.panel_width  = 240;
      cfg.panel_height = 320;
#endif
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.invert     = false;
      cfg.rgb_order  = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = true;
      _panel.config(cfg);
    }
    setPanel(&_panel);
  }
};

LGFX tft;
static const int PIN_BL = 21;

#if DISPLAY_PROFILE == DISPLAY_PROFILE_ILI9341_320x240
static constexpr int RAW_W = 320;
static constexpr int RAW_H = 240;
#else
static constexpr int RAW_W = 240;
static constexpr int RAW_H = 320;
#endif
#endif

static inline uint16_t clampU16(int v, int lo, int hi) {
  if (v < lo) return (uint16_t)lo;
  if (v > hi) return (uint16_t)hi;
  return (uint16_t)v;
}

static inline void mapTouchToScreen(uint16_t rx, uint16_t ry, uint16_t &x, uint16_t &y) {
  int sx = 0, sy = 0;

  // rotation d’abord
  switch (ROT & 3) {
    case 0: sx = rx;                 sy = ry;                 break;
    case 1: sx = (RAW_H - 1) - ry;   sy = rx;                 break;
    case 2: sx = (RAW_W - 1) - rx;   sy = (RAW_H - 1) - ry;   break;
    case 3: sx = ry;                 sy = (RAW_W - 1) - rx;   break;
  }

  int w = tft.width();
  int h = tft.height();

  uint16_t xx = clampU16(sx, 0, w - 1);
  uint16_t yy = clampU16(sy, 0, h - 1);

  // flips ensuite
  if (FLIP_X) xx = (uint16_t)((w - 1) - xx);
  if (FLIP_Y) yy = (uint16_t)((h - 1) - yy);

  x = xx;
  y = yy;
}

#if DISPLAY_PROFILE == DISPLAY_PROFILE_2432S022
// Lecture tactile "prête UI" (cap touch)
static inline bool readTouchScreen(int16_t &sx, int16_t &sy) {
  int n = g_touch.getSamples(&g_ti);
  if (n > 0 && g_ti.count > 0) {
    uint16_t x, y;
    mapTouchToScreen(g_ti.x[0], g_ti.y[0], x, y);
    sx = (int16_t)x;
    sy = (int16_t)y;
    return true;
  }
  return false;
}
#else
// --- Touch XPT2046 (soft-SPI + mapping simple) ---
static inline void spiDelayTouch() { /* petit délai si besoin */ }

static inline void softSPIBeginTouch() {
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);
  pinMode(TOUCH_CLK, OUTPUT);
  digitalWrite(TOUCH_CLK, HIGH);
  pinMode(TOUCH_MOSI, OUTPUT);
  digitalWrite(TOUCH_MOSI, LOW);
  pinMode(TOUCH_MISO, INPUT);
  pinMode(TOUCH_IRQ, INPUT);
}

static inline uint16_t xptRead12_soft(uint8_t cmd) {
  digitalWrite(TOUCH_CS, LOW);
  spiDelayTouch();

  for (int i = 7; i >= 0; --i) {
    digitalWrite(TOUCH_CLK, LOW);
    digitalWrite(TOUCH_MOSI, (cmd >> i) & 1);
    spiDelayTouch();
    digitalWrite(TOUCH_CLK, HIGH);
    spiDelayTouch();
  }

  uint16_t v = 0;
  for (int i = 15; i >= 0; --i) {
    digitalWrite(TOUCH_CLK, LOW);
    spiDelayTouch();
    digitalWrite(TOUCH_CLK, HIGH);
    v = (uint16_t)((v << 1) | (uint16_t)(digitalRead(TOUCH_MISO) & 1));
    spiDelayTouch();
  }

  digitalWrite(TOUCH_CS, HIGH);
  v >>= 4;
  return v & 0x0FFF;
}

static TouchSample readTouchOnce() {
  TouchSample s{};
  uint16_t ry = xptRead12_soft(0x90);
  uint16_t rx = xptRead12_soft(0xD0);
  uint16_t z1 = xptRead12_soft(0xB0);
  uint16_t z2 = xptRead12_soft(0xC0);

  int z = (int)z1 + (4095 - (int)z2);
  if (z < 0) z = 0;
  if (z > 4095) z = 4095;

  s.x = rx; s.y = ry; s.z = (uint16_t)z;
  s.valid = (s.z > 2400) && (s.x > 10) && (s.y > 10);
  return s;
}

// ================== TACTILE (XPT2046 SOFT-SPI + CALIB AFFINE) ==================
static TouchAffine touchCal = { 0,0,0, 0,0,0, false, false };
static const char* TOUCH_CAL_FILE = "/touch_affine.json";

static uint16_t medianSmall(uint16_t* a, int n) {
  for (int i = 1; i < n; i++) {
    uint16_t k = a[i]; int j = i - 1;
    while (j >= 0 && a[j] > k) { a[j + 1] = a[j]; j--; }
    a[j + 1] = k;
  }
  return a[n / 2];
}

static TouchSample readTouchFiltered() {
  const int N = 9;
  uint16_t xs[N], ys[N], zs[N];
  int k = 0;
  for (int i = 0; i < N; i++) {
    TouchSample s = readTouchOnce();
    if (!s.valid) { delay(2); continue; }
    xs[k] = s.x; ys[k] = s.y; zs[k] = s.z; k++;
    delay(2);
  }
  TouchSample out{};
  if (k < 5) { out.valid = false; return out; }
  out.x = medianSmall(xs, k);
  out.y = medianSmall(ys, k);
  uint16_t zmax = 0; for (int i = 0; i < k; i++) if (zs[i] > zmax) zmax = zs[i];
  out.z = zmax;
  out.valid = true;
  return out;
}

static bool waitPressStable(TouchSample &out, uint32_t timeoutMs = 25000) {
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    TouchSample a = readTouchFiltered();
    if (!a.valid) { delay(10); continue; }
    delay(40);
    TouchSample b = readTouchFiltered();
    delay(40);
    TouchSample c = readTouchFiltered();
    if (!b.valid || !c.valid) { delay(10); continue; }
    int dx1 = abs((int)a.x - (int)b.x), dy1 = abs((int)a.y - (int)b.y);
    int dx2 = abs((int)b.x - (int)c.x), dy2 = abs((int)b.y - (int)c.y);
    if (dx1 < 45 && dy1 < 45 && dx2 < 45 && dy2 < 45) { out = c; return true; }
    delay(10);
  }
  return false;
}

static void waitRelease(uint32_t timeoutMs = 8000) {
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    if (!readTouchOnce().valid) return;
    delay(15);
  }
}

// ---- Solve 3x3 (Gaussian) ----
static bool solve3x3(float A[3][3], float B[3], float X[3]) {
  float M[3][4];
  for (int r = 0; r < 3; r++) {
    for (int c = 0; c < 3; c++) M[r][c] = A[r][c];
    M[r][3] = B[r];
  }

  for (int i = 0; i < 3; i++) {
    int piv = i;
    float best = fabsf(M[i][i]);
    for (int r = i + 1; r < 3; r++) {
      float v = fabsf(M[r][i]);
      if (v > best) { best = v; piv = r; }
    }
    if (best < 1e-6f) return false;
    if (piv != i) {
      for (int c = i; c < 4; c++) { float tmp = M[i][c]; M[i][c] = M[piv][c]; M[piv][c] = tmp; }
    }

    float div = M[i][i];
    for (int c = i; c < 4; c++) M[i][c] /= div;

    for (int r = 0; r < 3; r++) {
      if (r == i) continue;
      float f = M[r][i];
      for (int c = i; c < 4; c++) M[r][c] -= f * M[i][c];
    }
  }
  X[0] = M[0][3]; X[1] = M[1][3]; X[2] = M[2][3];
  return true;
}

static bool fitAffineLSQ(const TouchSample* raw, const int* S, int n, float &a, float &b, float &c) {
  float XtX[3][3] = {{0,0,0},{0,0,0},{0,0,0}};
  float XtY[3] = {0,0,0};

  for (int i = 0; i < n; i++) {
    float x = (float)raw[i].x;
    float y = (float)raw[i].y;
    float yy = (float)S[i];
    float v[3] = {x, y, 1.0f};
    for (int r = 0; r < 3; r++) {
      for (int cc = 0; cc < 3; cc++) {
        XtX[r][cc] += v[r] * v[cc];
      }
      XtY[r] += v[r] * yy;
    }
  }

  float p[3];
  if (!solve3x3(XtX, XtY, p)) return false;
  a = p[0]; b = p[1]; c = p[2];
  return true;
}

static bool rawToScreenAffine(const TouchSample& r, int &sx, int &sy) {
  if (!touchCal.ok || !r.valid) return false;
  float fx = touchCal.a * (float)r.x + touchCal.b * (float)r.y + touchCal.c;
  float fy = touchCal.d * (float)r.x + touchCal.e * (float)r.y + touchCal.f;
  int W = tft.width(), H = tft.height();
  int ix = (int)lroundf(fx), iy = (int)lroundf(fy);
  if (ix < 0) ix = 0; if (ix > W - 1) ix = W - 1;
  if (iy < 0) iy = 0; if (iy > H - 1) iy = H - 1;
  sx = ix; sy = iy;
  return true;
}

static void touchBanner(const char* s) {
  tft.fillRect(0, 0, tft.width(), 22, 0x0000);
  tft.setTextSize(1);
  tft.setTextColor(0xFFFF, 0x0000);
  tft.setCursor(6, 6);
  tft.print(s);
}

static void touchCross(int x, int y, uint16_t col) {
  tft.drawLine(x - 14, y, x + 14, y, col);
  tft.drawLine(x, y - 14, x, y + 14, col);
  tft.drawRect(x - 18, y - 18, 36, 36, col);
}

static bool touchLoadFromSD() {
  touchCal.ok = false;
  touchCal.skipped = false;

  if (!sdReady) return false;
  if (!SD.exists(TOUCH_CAL_FILE)) return false;

  File f = SD.open(TOUCH_CAL_FILE, FILE_READ);
  if (!f) return false;

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;

  if ((doc["ver"] | 0) != 1) return false;

  touchCal.skipped = (doc["skip"] | 0) ? true : false;

  if (!touchCal.skipped) {
    touchCal.a = doc["a"] | 0.0f;
    touchCal.b = doc["b"] | 0.0f;
    touchCal.c = doc["c"] | 0.0f;
    touchCal.d = doc["d"] | 0.0f;
    touchCal.e = doc["e"] | 0.0f;
    touchCal.f = doc["f"] | 0.0f;
    touchCal.ok = (doc["ok"] | 0) ? true : false;
  } else {
    touchCal.a = touchCal.b = touchCal.c = 0.0f;
    touchCal.d = touchCal.e = touchCal.f = 0.0f;
    touchCal.ok = false;
  }

  return (touchCal.ok || touchCal.skipped);
}

static bool touchSaveToSD() {
  if (!sdReady) return false;

  touchCal.skipped = false;

  StaticJsonDocument<256> doc;
  doc["ver"] = 1;
  doc["ok"]  = touchCal.ok ? 1 : 0;
  doc["skip"] = 0;
  doc["a"] = touchCal.a; doc["b"] = touchCal.b; doc["c"] = touchCal.c;
  doc["d"] = touchCal.d; doc["e"] = touchCal.e; doc["f"] = touchCal.f;

  const char* TMP = "/touch_affine.tmp";
  if (SD.exists(TMP)) SD.remove(TMP);
  File f = SD.open(TMP, FILE_WRITE);
  if (!f) return false;
  if (serializeJson(doc, f) == 0) { f.close(); SD.remove(TMP); return false; }
  f.flush(); f.close();

  if (SD.exists(TOUCH_CAL_FILE)) SD.remove(TOUCH_CAL_FILE);
  if (!SD.rename(TMP, TOUCH_CAL_FILE)) { SD.remove(TMP); return false; }
  return true;
}

static bool touchSaveSkipToSD() {
  if (!sdReady) return false;

  touchCal.ok = false;
  touchCal.skipped = true;

  StaticJsonDocument<128> doc;
  doc["ver"] = 1;
  doc["ok"]  = 0;
  doc["skip"] = 1;

  const char* TMP = "/touch_affine.tmp";
  if (SD.exists(TMP)) SD.remove(TMP);
  File f = SD.open(TMP, FILE_WRITE);
  if (!f) return false;
  if (serializeJson(doc, f) == 0) { f.close(); SD.remove(TMP); return false; }
  f.flush(); f.close();

  if (SD.exists(TOUCH_CAL_FILE)) SD.remove(TOUCH_CAL_FILE);
  if (!SD.rename(TMP, TOUCH_CAL_FILE)) { SD.remove(TMP); return false; }
  return true;
}

static bool touchRunCalibrationWizard() {
  int W = tft.width(), H = tft.height();
  const int M = 28;

  const int TLx = M,     TLy = M;
  const int TRx = W - M, TRy = M;
  const int BRx = W - M, BRy = H - M;
  const int BLx = M,     BLy = H - M;

  TouchSample Praw[4]{};
  int Sx[4] = {TLx, TRx, BRx, BLx};
  int Sy[4] = {TLy, TRy, BRy, BLy};

  tft.fillScreen(0x0000);
  touchBanner("Calibration tactile (1/4) : HAUT GAUCHE");
  touchCross(TLx, TLy, 0xFFFF);
  if (!waitPressStable(Praw[0])) return false; waitRelease();

  tft.fillScreen(0x0000);
  touchBanner("Calibration tactile (2/4) : HAUT DROIT");
  touchCross(TRx, TRy, 0xFFFF);
  if (!waitPressStable(Praw[1])) return false; waitRelease();

  tft.fillScreen(0x0000);
  touchBanner("Calibration tactile (3/4) : BAS DROIT");
  touchCross(BRx, BRy, 0xFFFF);
  if (!waitPressStable(Praw[2])) return false; waitRelease();

  tft.fillScreen(0x0000);
  touchBanner("Calibration tactile (4/4) : BAS GAUCHE");
  touchCross(BLx, BLy, 0xFFFF);
  if (!waitPressStable(Praw[3])) return false; waitRelease();

  bool okX = fitAffineLSQ(Praw, Sx, 4, touchCal.a, touchCal.b, touchCal.c);
  bool okY = fitAffineLSQ(Praw, Sy, 4, touchCal.d, touchCal.e, touchCal.f);
  touchCal.ok = okX && okY;

  if (touchCal.ok) touchSaveToSD();

  tft.fillScreen(0x0000);
  touchBanner(touchCal.ok ? "OK - demarrage du jeu..." : "ECHEC calib - demarrage");
  delay(600);

  return touchCal.ok;
}

// Lecture tactile "prête UI"
static inline bool readTouchScreen(int16_t &sx, int16_t &sy) {
  if (touchCal.skipped) return false;
  TouchSample r = readTouchFiltered();
  if (!r.valid) return false;
  int x, y;
  if (!rawToScreenAffine(r, x, y)) return false;
  sx = (int16_t)x;
  sy = (int16_t)y;
  return true;
}
#endif


// ================== ENCODEUR / BOUTONS ==================

static inline bool encoderEnabled() { return (ENC_A >= 0 && ENC_B >= 0); }
static inline bool encoderButtonEnabled() { return (ENC_BTN >= 0); }
static inline bool buttonsEnabled() { return (BTN_LEFT >= 0 || BTN_RIGHT >= 0 || BTN_OK >= 0); }

static inline bool readButtonRaw(int pin) {
  if (pin < 0) return false;
  return digitalRead(pin) == LOW;
}

volatile int32_t encPos = 0;
volatile uint8_t lastAB = 0;
static const int8_t QDEC[16] = { 0,-1,+1,0, +1,0,0,-1, -1,0,0,+1, 0,+1,-1,0 };

static inline uint8_t readAB() {
  uint8_t a = (uint8_t)digitalRead(ENC_A);
  uint8_t b = (uint8_t)digitalRead(ENC_B);
  return (a << 1) | b;
}
void IRAM_ATTR isrEnc() {
  uint8_t ab = readAB();
  uint8_t idx = (lastAB << 2) | ab;
  encPos += QDEC[idx];
  lastAB = ab;
}

static bool btnState = false;
static bool lastBtnRaw = false;
static uint32_t btnChangeAt = 0;
static const uint32_t BTN_DEBOUNCE_MS = 25;
static inline bool readBtnPressedRaw() {
  if (ENC_BTN < 0) return false;
  return digitalRead(ENC_BTN) == LOW;
}

struct ButtonDeb {
  bool raw = false;
  bool stable = false;
  bool lastStable = false;
  uint32_t changeAt = 0;
};
static ButtonDeb btnLeftDeb;
static ButtonDeb btnRightDeb;
static ButtonDeb btnOkDeb;

static bool btnLeftEdge = false;
static bool btnRightEdge = false;
static bool btnOkEdge = false;
static bool btnLeftHeld = false;
static bool btnRightHeld = false;
static bool btnOkHeld = false;

static inline void updateButtonDeb(ButtonDeb &b, bool raw, uint32_t now) {
  if (raw != b.raw) {
    b.raw = raw;
    b.changeAt = now;
  }
  if ((now - b.changeAt) > BTN_DEBOUNCE_MS) {
    b.stable = b.raw;
  }
}

static inline bool consumePressedEdge(ButtonDeb &b) {
  bool edge = (b.stable && !b.lastStable);
  b.lastStable = b.stable;
  return edge;
}

static void updateButtons(uint32_t now) {
  btnLeftEdge = btnRightEdge = btnOkEdge = false;
  btnLeftHeld = btnRightHeld = btnOkHeld = false;
  if (!buttonsEnabled()) return;

  updateButtonDeb(btnLeftDeb, readButtonRaw(BTN_LEFT), now);
  updateButtonDeb(btnRightDeb, readButtonRaw(BTN_RIGHT), now);
  updateButtonDeb(btnOkDeb, readButtonRaw(BTN_OK), now);

  btnLeftEdge = consumePressedEdge(btnLeftDeb);
  btnRightEdge = consumePressedEdge(btnRightDeb);
  btnOkEdge = consumePressedEdge(btnOkDeb);

  btnLeftHeld = btnLeftDeb.stable;
  btnRightHeld = btnRightDeb.stable;
  btnOkHeld = btnOkDeb.stable;
}

static const int ENC_DIV = 4;
static int32_t lastDetent = 0;
static inline int32_t detentFromEnc(int32_t p) {
  if (p >= 0) return p / ENC_DIV;
  return - (int32_t)(((-p) + ENC_DIV - 1) / ENC_DIV);
}

// ================== RENDU BAND ==================
static const int BAND_H = 30;
LGFX_Sprite band(&tft);

// ================== MONDE ==================
static int SW = 320, SH = 240;
static const int GROUND_Y = 170;

static const uint16_t C_SKY     = 0x7D7C;
static const uint16_t C_GROUND  = 0x23E7;
static const uint16_t C_GROUND2 = 0x1B45;
static const uint16_t C_GLINE   = 0x2C48;

static inline int imod(int a, int m) { int r = a % m; return (r < 0) ? r + m : r; }
static inline int clampi(int v, int lo, int hi) { if (v < lo) return lo; if (v > hi) return hi; return v; }
static inline float clampf(float v, float lo, float hi) { if (v < lo) return lo; if (v > hi) return hi; return v; }
static inline float clamp01f(float v) { return clampf(v, 0.0f, 100.0f); }

static inline uint32_t hash32(int32_t x) {
  uint32_t v = (uint32_t)x;
  v ^= v >> 16; v *= 0x7feb352d;
  v ^= v >> 15; v *= 0x846ca68b;
  v ^= v >> 16;
  return v;
}

// ================== PET ==================
struct PetStats {
  float faim    = 60;
  float soif    = 60;
  float hygiene = 80;
  float humeur  = 60;
  float energie = 100;
  float fatigue = 100;
  float amour   = 60;
  float caca    = 0;

  float sante   = 80;
  uint32_t ageMin = 0;
  bool vivant = true;

  AgeStage stage = AGE_JUNIOR;

  // progression d'évolution (minutes validées NON consécutives)
  uint32_t evolveProgressMin = 0;

  bool poopAccidentLatched = false;
} pet;

static GamePhase phase = PHASE_EGG;

// ================== AUDIO ==================
static const int SPEAK_PIN = 26;
static const int AUDIO_PWM_CHANNEL = 6;
static const int AUDIO_PWM_BITS = 10;
static const uint16_t AUDIO_DUTY_NORMAL = 320;
static const uint16_t AUDIO_DUTY_QUIET  = 150;
static uint8_t audioVolumePercent = 1; // 0-100 pour reduire le volume global

struct AudioStep {
  uint16_t freq = 0;
  uint16_t durMs = 0;
  uint16_t duty = 0;
};

static void audioPwmSetup() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(SPEAK_PIN, 2000, AUDIO_PWM_BITS);
#else
  ledcSetup(AUDIO_PWM_CHANNEL, 2000, AUDIO_PWM_BITS);
  ledcAttachPin(SPEAK_PIN, AUDIO_PWM_CHANNEL);
#endif
}

static void audioWriteTone(uint16_t freq) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWriteTone(SPEAK_PIN, freq);
#else
  ledcWriteTone(AUDIO_PWM_CHANNEL, freq);
#endif
}

static void audioWriteDuty(uint16_t duty) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(SPEAK_PIN, duty);
#else
  ledcWrite(AUDIO_PWM_CHANNEL, duty);
#endif
}

static uint16_t audioDutyScaled(uint16_t duty) {
  if (audioVolumePercent >= 100) return duty;
  uint32_t scaled = (uint32_t)duty * (uint32_t)audioVolumePercent;
  return (uint16_t)(scaled / 100U);
}

#if ENABLE_AUDIO
static AudioMode audioMode = AUDIO_TOTAL;
#else
static AudioMode audioMode = AUDIO_OFF;
#endif
static const AudioStep* audioSeq = nullptr;
static uint8_t audioSeqLen = 0;
static uint8_t audioSeqIndex = 0;
static uint32_t audioStepUntil = 0;
static AudioPriority audioPriority = AUDIO_PRIO_LOW;
static uint32_t audioNextAlertAt = 0;
static bool audioActive = false;

static AudioStep audioDynSeq[128];
static const char* audioLoopRtttl = nullptr;
static AudioPriority audioLoopPriority = AUDIO_PRIO_LOW;

static void audioSetTone(uint16_t freq, uint16_t duty) {
  if (freq == 0 || duty == 0) {
    audioWriteTone(0);
    audioWriteDuty(0);
    return;
  }
  audioWriteTone(freq);
  audioWriteDuty(audioDutyScaled(duty));
}

static void audioStop() {
  audioActive = false;
  audioSeq = nullptr;
  audioSeqLen = 0;
  audioSeqIndex = 0;
  audioStepUntil = 0;
  audioPriority = AUDIO_PRIO_LOW;
  audioSetTone(0, 0);
}

static void stopAudio() {
  audioLoopRtttl = nullptr;
  audioStop();
}

static void audioStartSequence(const AudioStep* seq, uint8_t len, AudioPriority priority) {
  if (audioMode == AUDIO_OFF || seq == nullptr || len == 0) return;
  if (audioActive && priority < audioPriority) return;

  audioSeq = seq;
  audioSeqLen = len;
  audioSeqIndex = 0;
  audioPriority = priority;
  audioActive = true;
  audioStepUntil = 0;
}

static void playRTTTLOnce(const char* rtttl, AudioPriority priority);
static void audioTickMusic(uint32_t now);

static void audioUpdate(uint32_t now) {
  if (!audioActive) return;
  if (audioSeq == nullptr || audioSeqLen == 0) { audioStop(); return; }

  if (audioStepUntil == 0 || (int32_t)(now - audioStepUntil) >= 0) {
    if (audioSeqIndex >= audioSeqLen) {
    if (audioLoopRtttl != nullptr) {
        audioStop();
        playRTTTLOnce(audioLoopRtttl, audioLoopPriority);
        return;
      }
      audioStop();
      return;
    }
    const AudioStep& step = audioSeq[audioSeqIndex++];
    audioSetTone(step.freq, step.duty);
    audioStepUntil = now + step.durMs;
  }
}

static uint16_t rtttlNoteFrequency(char note, bool sharp, uint8_t octave) {
  if (note == 'p') return 0;
  int semitone = 0;
  switch (note) {
    case 'c': semitone = 0; break;
    case 'd': semitone = 2; break;
    case 'e': semitone = 4; break;
    case 'f': semitone = 5; break;
    case 'g': semitone = 7; break;
    case 'a': semitone = 9; break;
    case 'b': semitone = 11; break;
    default: return 0;
  }
  if (sharp) semitone++;
  int midi = ((int)octave + 1) * 12 + semitone;
  float freq = 440.0f * powf(2.0f, ((float)midi - 69.0f) / 12.0f);
  return (uint16_t)max(0.0f, freq);
}

static uint16_t rtttlParseNumber(const char*& p) {
  uint16_t val = 0;
  while (*p && isdigit(static_cast<unsigned char>(*p))) {
    val = (uint16_t)(val * 10 + (*p - '0'));
    p++;
  }
  return val;
}

static uint8_t rtttlBuildSequence(const char* rtttl, AudioStep* out, uint8_t maxSteps) {
  if (rtttl == nullptr || out == nullptr || maxSteps == 0) return 0;

  const char* p = strchr(rtttl, ':');
  if (!p) return 0;
  p++;

  uint16_t defaultDur = 4;
  uint8_t defaultOct = 6;
  uint16_t bpm = 63;

  const char* settingsEnd = strchr(p, ':');
  if (!settingsEnd) return 0;

  while (p < settingsEnd) {
    if (*p == 'd' && *(p + 1) == '=') {
      p += 2;
      uint16_t v = rtttlParseNumber(p);
      if (v > 0) defaultDur = v;
    } else if (*p == 'o' && *(p + 1) == '=') {
      p += 2;
      uint16_t v = rtttlParseNumber(p);
      if (v >= 3 && v <= 7) defaultOct = (uint8_t)v;
    } else if (*p == 'b' && *(p + 1) == '=') {
      p += 2;
      uint16_t v = rtttlParseNumber(p);
      if (v > 0) bpm = v;
    }
    while (p < settingsEnd && *p != ',') p++;
    if (*p == ',') p++;
  }

  p = settingsEnd + 1;
  if (bpm == 0) bpm = 63;
  uint32_t wholeNoteMs = (uint32_t)((60000UL * 4UL) / bpm);

  uint8_t outIndex = 0;
  while (*p && outIndex < maxSteps) {
    if (*p == ',') { p++; continue; }

    uint16_t dur = 0;
    if (isdigit(static_cast<unsigned char>(*p))) {
      dur = rtttlParseNumber(p);
    } else {
      dur = defaultDur;
    }
    if (dur == 0) dur = defaultDur;

    char note = (char)tolower(static_cast<unsigned char>(*p));
    if (note == 0) break;
    if (note < 'a' || note > 'g') {
      if (note != 'p') {
        while (*p && *p != ',') p++;
        continue;
      }
    }
    p++;

    bool sharp = false;
    if (*p == '#') { sharp = true; p++; }

    bool dotted = false;
    if (*p == '.') { dotted = true; p++; }

    uint8_t octave = defaultOct;
    if (isdigit(static_cast<unsigned char>(*p))) {
      octave = (uint8_t)rtttlParseNumber(p);
    }

    if (*p == '.') { dotted = true; p++; }

    uint32_t noteDur = wholeNoteMs / dur;
    if (dotted) noteDur += noteDur / 2;

    uint16_t freq = rtttlNoteFrequency(note, sharp, octave);
    uint16_t duty = (note == 'p' || freq == 0) ? 0 : AUDIO_DUTY_NORMAL;
    out[outIndex++] = {freq, (uint16_t)noteDur, duty};
  }
  return outIndex;
}

static void playRTTTLOnce(const char* rtttl, AudioPriority priority) {
  uint8_t len = rtttlBuildSequence(rtttl, audioDynSeq, (uint8_t)(sizeof(audioDynSeq) / sizeof(audioDynSeq[0])));
  audioStartSequence(audioDynSeq, len, priority);
}

static void playRTTTLLoop(const char* rtttl, AudioPriority priority) {
  if (audioMode == AUDIO_OFF || rtttl == nullptr) return;
  audioLoopRtttl = rtttl;
  audioLoopPriority = priority;
  if (!audioActive || priority >= audioPriority) {
    playRTTTLOnce(rtttl, priority);
  }
}

static void audioPlayRTTTLRepeat(const char* rtttl, uint8_t count, AudioPriority priority) {
  if (count == 0) return;
  if (count > 10) count = 10;

  AudioStep unit[16];
  uint8_t unitLen = rtttlBuildSequence(rtttl, unit, (uint8_t)(sizeof(unit) / sizeof(unit[0])));
  if (unitLen == 0) return;

  uint8_t idx = 0;
  uint8_t maxSteps = (uint8_t)(sizeof(audioDynSeq) / sizeof(audioDynSeq[0]));
  for (uint8_t i = 0; i < count && idx < maxSteps; i++) {
    for (uint8_t j = 0; j < unitLen && idx < maxSteps; j++) {
      audioDynSeq[idx++] = unit[j];
    }
  }
  audioStartSequence(audioDynSeq, idx, priority);
}

static uint8_t healthCriticalCount() {
  if (!pet.vivant || phase != PHASE_ALIVE) return 0;
  uint8_t count = 0;
  if (pet.faim    < 15) count++;
  if (pet.soif    < 15) count++;
  if (pet.hygiene < 15) count++;
  if (pet.humeur  < 10) count++;
  if (pet.energie < 10) count++;
  if (pet.fatigue < 10) count++;
  if (pet.amour   < 10) count++;
  if (pet.caca >= 95) count++;
  return count;
}

static inline bool criticalBlinkOn(uint32_t now) {
  if (healthCriticalCount() == 0) return false;
  return ((now / 500UL) % 2U) == 0U;
}

static void audioTickAlerts(uint32_t now) {
  if (audioMode == AUDIO_OFF) return;
  uint32_t interval = (audioMode == AUDIO_TOTAL) ? 10000UL : 20000UL;
  if (audioNextAlertAt == 0) audioNextAlertAt = now + interval;
  if ((int32_t)(now - audioNextAlertAt) < 0) return;

  uint8_t count = healthCriticalCount();
  if (count > 0) {
    audioPlayRTTTLRepeat(RTTTL_ALERT_UNIT, count, AUDIO_PRIO_LOW);
  }
  audioNextAlertAt = now + interval;
}

// nom
static char petName[20] = "???";

// ================== UI ==================
static uint8_t uiSel = 0;

static const uint16_t COL_FAIM    = 0xFD20;
static const uint16_t COL_SOIF    = 0x03FF;
static const uint16_t COL_HYGIENE = 0x07FF;
static const uint16_t COL_HUMEUR  = 0xFFE0;
static const uint16_t COL_ENERGIE = 0x001F;
static const uint16_t COL_FATIGUE = 0x8410;
static const uint16_t COL_AMOUR2  = 0xF8B2;
static const uint16_t COL_CACA    = 0xA145;

static inline uint16_t btnColorForAction(UiAction a) {
  switch (a) {
    case UI_REPOS:  return COL_ENERGIE;
    case UI_MANGER: return COL_FAIM;
    case UI_BOIRE:  return COL_SOIF;
    case UI_LAVER:  return COL_HYGIENE;
    case UI_JOUER:  return COL_HUMEUR;
    case UI_CACA:   return COL_CACA;
    case UI_CALIN:  return COL_AMOUR2;
    case UI_AUDIO:  return 0x7BEF;
    default:        return TFT_WHITE;
  }
}
static inline const char* btnLabel(UiAction a) {
  switch (a) {
    case UI_REPOS:  return "Repos";
    case UI_MANGER: return "Manger";
    case UI_BOIRE:  return "Boire";
    case UI_LAVER:  return "Laver";
    case UI_JOUER:  return "Jouer";
    case UI_CACA:   return "Caca";
    case UI_CALIN:  return "Calin";
    case UI_AUDIO:  return "Son";
    default:        return "?";
  }
}
static inline const char* stageLabel(AgeStage s) {
  switch (s) {
    case AGE_JUNIOR: return "Junior";
    case AGE_ADULTE: return "Adulte";
    case AGE_SENIOR: return "Senior";
    default:         return "?";
  }
}

static void drawAudioIcon(LGFX_Sprite& g, int x, int y, int w, int h, uint16_t color, AudioMode mode) {
  int cx = x + w / 2;
  int cy = y + h / 2;
  int noteR = 3;
  int stemH = 10;

  auto drawNote = [&](int nx, int ny) {
    g.fillCircle(nx, ny, noteR, color);
    g.drawFastVLine(nx + noteR, ny - stemH, stemH, color);
    g.drawFastHLine(nx + noteR, ny - stemH, 6, color);
  };

  if (mode == AUDIO_TOTAL) {
    drawNote(cx - 6, cy + 2);
    drawNote(cx + 4, cy - 1);
  } else {
    drawNote(cx - 2, cy + 1);
  }

  if (mode == AUDIO_OFF) {
    g.drawLine(x + 6, y + 6, x + w - 6, y + h - 6, color);
  }
}
// ===== Ordre des boutons (barre du bas) =====
// Change juste l'ordre ici pour déplacer Manger/Boire où tu veux :
#if USE_TOP_QUICK_BUTTONS
static const UiAction UI_ORDER_ALIVE[] = {
  UI_REPOS, UI_LAVER, UI_JOUER, UI_CACA, UI_CALIN
#if ENABLE_AUDIO
  , UI_AUDIO
#endif
};
#else
static const UiAction UI_ORDER_ALIVE[] = {
  UI_REPOS, UI_MANGER, UI_BOIRE, UI_LAVER, UI_JOUER, UI_CACA, UI_CALIN
#if ENABLE_AUDIO
  , UI_AUDIO
#endif
};
#endif

static inline uint8_t uiAliveCount() {
  return (uint8_t)(sizeof(UI_ORDER_ALIVE) / sizeof(UI_ORDER_ALIVE[0]));
}

static inline UiAction uiAliveActionAt(uint8_t idx) {
  if (idx >= uiAliveCount()) return UI_REPOS;
  return UI_ORDER_ALIVE[idx];
}
// barre activité (affichée si occupé OU message)
static bool activityVisible = false;
static uint32_t activityStart = 0;
static uint32_t activityEnd   = 0;
static char activityText[64]  = {0};

// IMPORTANT: pour que la barre se remplisse progressivement,
// on force une reconstruction UI régulière tant que activityVisible==true.
static uint32_t lastActivityUiRefresh = 0;
static const uint32_t ACTIVITY_UI_REFRESH_MS = 120; // ~8 fps

// message court -> utilise la barre activité
static bool showMsg = false;
static uint32_t msgUntil = 0;
static char msgText[64] = {0};

static void setMsg(const char* s, uint32_t now, uint32_t dur=1500) {
  strncpy(msgText, s, sizeof(msgText)-1);
  msgText[sizeof(msgText)-1] = 0;
  showMsg = true;
  msgUntil = now + dur;

  // si pas occupé : on affiche la barre activité juste pour le message
  if (!activityVisible) {
    activityVisible = true;
    activityStart = now;
    activityEnd   = now + dur;
    strncpy(activityText, s, sizeof(activityText)-1);
    activityText[sizeof(activityText)-1] = 0;
  }
}

// sprites UI
static const int UI_TOP_H = 95;   // + grand pour remonter bars + barre activité
static const int UI_BOT_H = 52;
LGFX_Sprite uiTop(&tft);
LGFX_Sprite uiBot(&tft);

static bool uiSpriteDirty = true;
static bool uiForceBands  = true;

// ================== MONDE 2× écran + objets ==================
static float worldW = 640.0f;
static float worldMin = 0.0f;
static float worldMax = 640.0f;

static float homeX = 320.0f;

static float bushLeftX  = 20.0f;
static float puddleX    = 0.0f;

static bool  berriesLeftAvailable = true;
static uint32_t berriesRespawnAt = 0;

static bool  puddleVisible = true;
static uint32_t puddleRespawnAt = 0;

// ================== DINO position / camera ==================
static float worldX = 0.0f;
static float camX   = 0.0f;
static bool movingRight = true;

static int DZ_L = 0;
static int DZ_R = 0;

static inline bool flipForMovingRight(bool right) {
  return TRI_FACES_LEFT ? right : !right;
}

// ================== STATE / ANIM ==================
static TriState state = ST_SIT;

static uint8_t animIdx = 0;
static uint32_t nextAnimTick = 0;

// caca visible
static bool poopVisible = false;
static uint32_t poopUntil = 0;
static float poopWorldX = 0.0f;

// idle autonome
static float idleTargetX = 0.0f;
static bool  idleWalking = false;
static uint32_t idleUntil = 0;
static uint32_t nextIdleDecisionAt = 0;

// hatch
static uint8_t hatchIdx = 0;
static uint32_t hatchNext = 0;

// ================== Durée / gain par âge ==================
static inline float durMulForStage(AgeStage s) {
  switch (s) {
    case AGE_ADULTE: return DUR_MUL_ADULTE;
    case AGE_SENIOR: return DUR_MUL_SENIOR;
    default:         return DUR_MUL_JUNIOR;
  }
}
static inline float gainMulForStage(AgeStage s) {
  switch (s) {
    case AGE_ADULTE: return GAIN_MUL_ADULTE;
    case AGE_SENIOR: return GAIN_MUL_SENIOR;
    default:         return GAIN_MUL_JUNIOR;
  }
}

static inline float fatigueFactor() {
  float f = clamp01f(pet.fatigue);
  return 1.0f + (1.0f - (f / 100.0f)) * 2.0f; //vitesse de ralentisement si fatiguer!
}
static inline uint32_t scaleByFatigueAndAge(uint32_t baseMs) {
  float k = fatigueFactor();
  float a = durMulForStage(pet.stage);
  return (uint32_t)max(1.0f, (float)baseMs * k * a);
}
static inline float moveSpeedPxPerFrame() {
  float k = fatigueFactor();
  float a = durMulForStage(pet.stage);
  float sp = ((float)SPEED_BASE_PX * SPEED_MULT) / k;
  sp = sp / a;
  if (sp < 0.8f) sp = 0.8f;
  return sp;
}

// ================== triceratops frames (3 namespaces) ==================
static inline const uint16_t* triGetFrame_J(triJ::AnimId anim, uint8_t frameIndex) {
  triJ::AnimDesc ad; memcpy_P(&ad, &triJ::ANIMS[anim], sizeof(ad));
  if (ad.count == 0) return nullptr;
  frameIndex %= ad.count;
  const uint16_t* framePtr = nullptr;
  memcpy_P(&framePtr, &ad.frames[frameIndex], sizeof(framePtr));
  return framePtr;
}
static inline uint8_t triAnimCount_J(triJ::AnimId anim) {
  triJ::AnimDesc ad; memcpy_P(&ad, &triJ::ANIMS[anim], sizeof(ad));
  return ad.count;
}
static inline const uint16_t* triGetFrame_A(triA::AnimId anim, uint8_t frameIndex) {
  triA::AnimDesc ad; memcpy_P(&ad, &triA::ANIMS[anim], sizeof(ad));
  if (ad.count == 0) return nullptr;
  frameIndex %= ad.count;
  const uint16_t* framePtr = nullptr;
  memcpy_P(&framePtr, &ad.frames[frameIndex], sizeof(framePtr));
  return framePtr;
}
static inline uint8_t triAnimCount_A(triA::AnimId anim) {
  triA::AnimDesc ad; memcpy_P(&ad, &triA::ANIMS[anim], sizeof(ad));
  return ad.count;
}
static inline const uint16_t* triGetFrame_S(triS::AnimId anim, uint8_t frameIndex) {
  triS::AnimDesc ad; memcpy_P(&ad, &triS::ANIMS[anim], sizeof(ad));
  if (ad.count == 0) return nullptr;
  frameIndex %= ad.count;
  const uint16_t* framePtr = nullptr;
  memcpy_P(&framePtr, &ad.frames[frameIndex], sizeof(framePtr));
  return framePtr;
}
static inline uint8_t triAnimCount_S(triS::AnimId anim) {
  triS::AnimDesc ad; memcpy_P(&ad, &triS::ANIMS[anim], sizeof(ad));
  return ad.count;
}

static inline uint8_t animIdForState(AgeStage stg, TriState st) {
  if (stg == AGE_JUNIOR) {
    switch (st) {
      case ST_SIT:   return (uint8_t)triJ::ANIM_JUNIOR_ASSIE;
      case ST_BLINK: return (uint8_t)triJ::ANIM_JUNIOR_CLIGNE;
      case ST_EAT:   return (uint8_t)triJ::ANIM_JUNIOR_MANGE;
      case ST_SLEEP: return (uint8_t)triJ::ANIM_JUNIOR_DODO;
      default:       return (uint8_t)triJ::ANIM_JUNIOR_MARCHE;
    }
  } else if (stg == AGE_ADULTE) {
    switch (st) {
      case ST_SIT:   return (uint8_t)triA::ANIM_ADULTE_ASSIE;
      case ST_BLINK: return (uint8_t)triA::ANIM_ADULTE_CLIGNE;
      case ST_EAT:   return (uint8_t)triA::ANIM_ADULTE_MANGE;
      case ST_SLEEP: return (uint8_t)triA::ANIM_ADULTE_DODO;
      default:       return (uint8_t)triA::ANIM_ADULTE_MARCHE;
    }
  } else {
    switch (st) {
      case ST_SIT:   return (uint8_t)triS::ANIM_SENIOR_ASSIE;
      case ST_BLINK: return (uint8_t)triS::ANIM_SENIOR_CLIGNE;
      case ST_EAT:   return (uint8_t)triS::ANIM_SENIOR_MANGE;
      case ST_SLEEP: return (uint8_t)triS::ANIM_SENIOR_DODO;
      default:       return (uint8_t)triS::ANIM_SENIOR_MARCHE;
    }
  }
}
static inline uint8_t triAnimCount(AgeStage stg, uint8_t animId) {
  if (stg == AGE_JUNIOR) return triAnimCount_J((triJ::AnimId)animId);
  if (stg == AGE_ADULTE) return triAnimCount_A((triA::AnimId)animId);
  return triAnimCount_S((triS::AnimId)animId);
}
static inline const uint16_t* triGetFrame(AgeStage stg, uint8_t animId, uint8_t idx) {
  if (stg == AGE_JUNIOR) return triGetFrame_J((triJ::AnimId)animId, idx);
  if (stg == AGE_ADULTE) return triGetFrame_A((triA::AnimId)animId, idx);
  return triGetFrame_S((triS::AnimId)animId, idx);
}
static inline int triW(AgeStage stg) {
  if (stg == AGE_JUNIOR) return (int)triJ::W;
  if (stg == AGE_ADULTE) return (int)triA::W;
  return (int)triS::W;
}
static inline int triH(AgeStage stg) {
  if (stg == AGE_JUNIOR) return (int)triJ::H;
  if (stg == AGE_ADULTE) return (int)triA::H;
  return (int)triS::H;
}

// ================== DRAW IMAGE KEYED ==================
static inline uint16_t darken565(uint16_t c, uint8_t level) {
  if (level == 0) return c;
  uint8_t r = (c >> 11) & 0x1F;
  uint8_t g = (c >>  5) & 0x3F;
  uint8_t b = (c >>  0) & 0x1F;
  float k = (level == 1) ? 0.70f : 0.40f;
  r = (uint8_t)roundf((float)r * k);
  g = (uint8_t)roundf((float)g * k);
  b = (uint8_t)roundf((float)b * k);
  return (uint16_t)((r << 11) | (g << 5) | (b));
}

static void drawImageKeyedOnBand(const uint16_t* img565, int w, int h, int x, int y, bool flipX=false, uint8_t shade=0) {
  if (!img565) return;
  for (int j = 0; j < h; j++) {
    int yy = y + j;
    if (yy < 0 || yy >= band.height()) continue;
    for (int i = 0; i < w; i++) {
      int xx = x + i;
      if (xx < 0 || xx >= band.width()) continue;
      int sx = flipX ? (w - 1 - i) : i;
      uint16_t c = pgm_read_word(&img565[j * w + sx]);
      if (c == KEY || c == KEY_SWAP) continue;
      if (swap16(c) == KEY) continue;
      if (shade) c = darken565(c, shade);
      band.drawPixel(xx, yy, c);
    }
  }
}

static void drawImageKeyedOnTFT(const uint16_t* img565, int w, int h, int x, int y, bool flipX=false, uint8_t shade=0) {
  if (!img565) return;
  for (int j = 0; j < h; j++) {
    int yy = y + j;
    if (yy < 0 || yy >= tft.height()) continue;
    for (int i = 0; i < w; i++) {
      int xx = x + i;
      if (xx < 0 || xx >= tft.width()) continue;
      int sx = flipX ? (w - 1 - i) : i;
      uint16_t c = pgm_read_word(&img565[j * w + sx]);
      if (c == KEY || c == KEY_SWAP) continue;
      if (swap16(c) == KEY) continue;
      if (shade) c = darken565(c, shade);
      tft.drawPixel(xx, yy, c);
    }
  }
}

// ================== UI helpers ==================
static inline void uiTextShadow(LGFX_Sprite& g, int x, int y, const char* s, uint16_t fg, uint16_t bg=KEY) {
  g.setTextSize(1);
  g.setTextColor(TFT_BLACK, bg);
  g.setCursor(x+1, y+1);
  g.print(s);
  g.setTextColor(fg, bg);
  g.setCursor(x, y);
  g.print(s);
}

static void drawBarRound(LGFX_Sprite& g, int x, int y, int w, int h, float value, const char* label, uint16_t col) {
  value = clamp01f(value);
  int r = 4;
  g.drawRoundRect(x, y, w, h, r, col);
  g.fillRoundRect(x+1, y+1, w-2, h-2, r-1, TFT_WHITE);
  int innerW = w - 2;
  int fillW = (int)roundf((innerW * value) / 100.0f);
  if (fillW > 0) {
    if (fillW >= innerW - 2) g.fillRoundRect(x+1, y+1, innerW, h-2, r-1, col);
    else g.fillRect(x+1, y+1, fillW, h-2, col);
  }
  uiTextShadow(g, x, y-10, label, TFT_WHITE, KEY);
}

// barre activité (140 px, +50% hauteur, texte centré, SANS fond derrière le texte)
static void drawActivityBar(LGFX_Sprite& g, int centerX, int y, int w, int h, float value, const char* text) {
  int x = centerX - w/2;
  value = clamp01f(value);
  int r = 8;

  uint16_t border = 0x7BEF;
  uint16_t bg     = 0xFFFF;
  uint16_t fill   = 0xC618;

  g.drawRoundRect(x, y, w, h, r, border);
  g.fillRoundRect(x+1, y+1, w-2, h-2, r-1, bg);

  int innerW = w - 2;
  int fillW = (int)roundf((innerW * value) / 100.0f);
  if (fillW > 0) g.fillRoundRect(x+1, y+1, fillW, h-2, r-1, fill);

  // TEXTE: sans fond (pas de carré)
  g.setTextDatum(middle_center);
  g.setTextSize(1);

  // petite ombre
  g.setTextColor(TFT_WHITE);
  g.drawString(text ? text : "", x + w/2 + 1, y + h/2 + 1);

  g.setTextColor(TFT_BLACK);
  g.drawString(text ? text : "", x + w/2,     y + h/2);

  g.setTextDatum(top_left);
}

static inline uint8_t uiButtonCount() {
  if (phase == PHASE_EGG || phase == PHASE_HATCHING) return 1;
  if (phase == PHASE_RESTREADY) return 1;
  if (phase == PHASE_TOMB) return 1;
  if (state == ST_SLEEP) return 1;
  return uiAliveCount();

}
static inline int uiButtonWidth(uint8_t nbtn) {
  const int GAP = 6;
  int totalGap = ((int)nbtn - 1) * GAP;
  int bw = (SW - totalGap) / (int)nbtn;
  if (bw > 56) bw = 56;
  if (bw < 42) bw = max(1, bw);
  return bw;
}
static inline const char* uiSingleLabel() {
  if (phase == PHASE_EGG) return "Eclore";
  if (phase == PHASE_HATCHING) return "...";
  if (phase == PHASE_RESTREADY) return "Recommencer";
  if (phase == PHASE_TOMB) return "Recommencer";
  if (state == ST_SLEEP) return "Reveiller";
  return "?";
}
static inline uint16_t uiSingleColor() {
  if (phase == PHASE_EGG) return 0x07E0;
  if (phase == PHASE_RESTREADY) return 0x07E0;
  if (phase == PHASE_TOMB) return TFT_RED;
  if (state == ST_SLEEP) return COL_ENERGIE;
  return 0xC618;
}

// ================== TASKS ==================
struct Task {
  bool active = false;
  TaskKind kind = TASK_NONE;
  TaskPhase ph  = PH_GO;
  float targetX = 0;
  float returnX = 0;
  uint32_t doUntil = 0;
  uint32_t startedAt = 0;
  uint32_t plannedTotal = 0;

  uint32_t doMs = 0; // duree "sur place" pour manger/boire
} task;

static void audioTickMusic(uint32_t now) {
  (void)now;
  static bool initialized = false;
  static GamePhase prevPhase = PHASE_EGG;
  static TaskKind prevTaskKind = TASK_NONE;
  static TaskPhase prevTaskPhase = PH_GO;
  static bool prevTaskActive = false;
  static AgeStage prevAgeStage = AGE_JUNIOR;

  GamePhase curPhase = phase;
  bool curTaskActive = task.active;
  TaskKind curTaskKind = curTaskActive ? task.kind : TASK_NONE;
  TaskPhase curTaskPhase = curTaskActive ? task.ph : PH_GO;
  AgeStage curAgeStage = pet.stage;

  if (!initialized) {
    prevPhase = curPhase;
    prevTaskKind = curTaskKind;
    prevTaskPhase = curTaskPhase;
    prevTaskActive = curTaskActive;
    prevAgeStage = curAgeStage;
    initialized = true;
  } else {
    if (prevPhase != curPhase) {
      if (curPhase == PHASE_TOMB) {
        playRTTTLOnce(RTTTL_DEATH_INTRO, AUDIO_PRIO_HIGH);
      } else if (curPhase == PHASE_RESTREADY) {
        playRTTTLOnce(RTTTL_RIP_INTRO, AUDIO_PRIO_HIGH);
      }
    }

    if (prevAgeStage != curAgeStage) {
      if (prevAgeStage == AGE_JUNIOR && curAgeStage == AGE_ADULTE) {
        playRTTTLOnce(RTTTL_AGEUP_JUNIOR_TO_ADULT, AUDIO_PRIO_MED);
      } else if (prevAgeStage == AGE_ADULTE && curAgeStage == AGE_SENIOR) {
        playRTTTLOnce(RTTTL_AGEUP_ADULT_TO_SENIOR, AUDIO_PRIO_MED);
      }
    }

    if (curTaskActive && curTaskPhase == PH_DO &&
        (!prevTaskActive || prevTaskPhase != PH_DO || prevTaskKind != curTaskKind)) {
      if (curTaskKind == TASK_SLEEP) {
        playRTTTLOnce(RTTTL_SLEEP_INTRO, AUDIO_PRIO_MED);
      }
    }
  }

  const char* desiredLoop = nullptr;
  AudioPriority desiredPriority = AUDIO_PRIO_LOW;
  if (curPhase == PHASE_HATCHING) {
    desiredLoop = RTTTL_HATCH_INTRO;
    desiredPriority = AUDIO_PRIO_LOW;
  }

  if (desiredLoop != audioLoopRtttl) {
    if (desiredLoop == nullptr) {
      if (audioLoopRtttl != nullptr) {
        stopAudio();
      }
    } else {
      playRTTTLLoop(desiredLoop, desiredPriority);
    }
  }

  prevPhase = curPhase;
  prevTaskKind = curTaskKind;
  prevTaskPhase = curTaskPhase;
  prevTaskActive = curTaskActive;
  prevAgeStage = curAgeStage;
}

static uint32_t cdUntil[UI_COUNT] = {0};

// ================== Positions “stop” (manger/boire) ==================
static float eatSpotX() {
  // dino doit être à DROITE du buisson et regarder à gauche
  return bushLeftX + (float)(BBAIE_W - 18 + EAT_SPOT_OFFSET);
}
static float drinkSpotX() {
  // dino doit être à GAUCHE de la flaque et regarder à droite
  return puddleX - (float)(triW(pet.stage) - 22) + (float)DRINK_SPOT_OFFSET;
}

// ================== Activity bar control ==================
static void activityStartTask(uint32_t now, const char* text, uint32_t plannedTotalMs) {
  activityVisible = true;
  activityStart = now;
  activityEnd   = now + max(1UL, plannedTotalMs);
  strncpy(activityText, text, sizeof(activityText)-1);
  activityText[sizeof(activityText)-1] = 0;
}
static void activitySetText(const char* text) {
  strncpy(activityText, text, sizeof(activityText)-1);
  activityText[sizeof(activityText)-1] = 0;
}
static float activityProgress(uint32_t now) {
  if (!activityVisible) return 0;
  // indéterminé (ex: sommeil) => plein
  if (activityEnd <= activityStart) return 100;
  if ((int32_t)(now - activityEnd) >= 0) return 100;
  float t = (float)(now - activityStart) / (float)(activityEnd - activityStart);
  return clamp01f(t * 100.0f);
}
static void activityStopIfFree(uint32_t now) {
  if (task.active) return;
  if (showMsg && (int32_t)(now - msgUntil) < 0) return;
  activityVisible = false;
  activityText[0] = 0;
  (void)now;
}

// modes speciaux
static void activityShowFull(uint32_t now, const char* text) {
  activityVisible = true;
  activityStart = now;
  activityEnd   = now; // end<=start => progress=100
  strncpy(activityText, text, sizeof(activityText)-1);
  activityText[sizeof(activityText)-1] = 0;
}
static void activityShowProgress(uint32_t now, const char* text, uint32_t durMs) {
  activityVisible = true;
  activityStart = now;
  activityEnd   = now + max(1UL, durMs);
  strncpy(activityText, text, sizeof(activityText)-1);
  activityText[sizeof(activityText)-1] = 0;
}

// ================== SAVE SYSTEM (microSD JSON A/B) ==================
static const int SD_SCK  = 18;
static const int SD_MISO = 19;
static const int SD_MOSI = 23;
static const int SD_CS   = 5;

// Bus SD séparé (HSPI) pour éviter conflit avec l'écran (VSPI pins custom)
static SPIClass sdSPI(HSPI);
bool sdReady = false;

static const char* SAVE_A  = "/saveA.json";
static const char* SAVE_B  = "/saveB.json";
static const char* TMP_A   = "/saveA.tmp";
static const char* TMP_B   = "/saveB.tmp";

static const uint32_t SAVE_EVERY_MS = 60000UL;

static uint32_t saveSeq = 0;
static uint32_t lastSaveAt = 0;
static bool nextSlotIsA = true;  // alterne A/B

static inline int iClamp(int v, int lo, int hi){ if(v<lo) return lo; if(v>hi) return hi; return v; }
static inline int fToI100(float f){ return iClamp((int)lroundf(f), 0, 100); }

static bool sdInit() {
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdReady = SD.begin(SD_CS, sdSPI, 20000000);
  if (!sdReady) {
    Serial.println("[SD] init FAIL");
    return false;
  }
  Serial.println("[SD] init OK");
  return true;
}

static bool readJsonFile(const char* path, StaticJsonDocument<768>& doc, uint32_t& outSeq) {
  outSeq = 0;
  if (!sdReady) return false;
  if (!SD.exists(path)) return false;

  File f = SD.open(path, FILE_READ);
  if (!f) return false;

  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;

  int ver = doc["ver"] | 0;
  if (ver != 1) return false;

  outSeq = doc["seq"] | 0UL;
  if (!doc.containsKey("phase")) return false;
  if (!doc.containsKey("stage")) return false;
  if (!doc.containsKey("ageMin")) return false;
  if (!doc.containsKey("pet")) return false;

  return true;
}

static void applyLoadedToRuntime(uint32_t now) {
  task.active = false;
  task.kind = TASK_NONE;
  task.ph = PH_GO;
  task.doUntil = 0;
  task.startedAt = now;
  task.plannedTotal = 0;
  task.doMs = 0;

  // cooldowns annulés
  for (int i = 0; i < (int)UI_COUNT; i++) cdUntil[i] = 0;

  // monde recentré
  worldX = homeX;
  camX   = worldX - (float)(SW / 2);
  camX = clampf(camX, 0.0f, worldW - (float)SW);

  poopVisible = false; poopUntil = 0;
  berriesLeftAvailable = true; berriesRespawnAt = 0;
  puddleVisible = true; puddleRespawnAt = 0;

  activityVisible = false;
  activityText[0] = 0;
  showMsg = false;
  uiSel = 0;

  animIdx = 0;
  nextAnimTick = 0;
  enterState(ST_SIT, now);

  // mini-jeu reset
  appMode = MODE_PET;
  mg.active = false;
  mg.kind = TASK_NONE;

  uiSpriteDirty = true;
  uiForceBands  = true;
}

static bool loadLatestSave(uint32_t now) {
  if (!sdReady) return false;

  StaticJsonDocument<768> dA, dB;
  uint32_t sA=0, sB=0;
  bool okA = readJsonFile(SAVE_A, dA, sA);
  bool okB = readJsonFile(SAVE_B, dB, sB);

  if (!okA && !okB) return false;

  StaticJsonDocument<768>* bestDoc = nullptr;
  bool bestIsA = true;

  if (okA && (!okB || sA >= sB)) { bestDoc = &dA; bestIsA = true; }
  else                           { bestDoc = &dB; bestIsA = false; }

  auto& doc = *bestDoc;

  saveSeq = doc["seq"] | 0UL;

  int p  = doc["phase"] | (int)PHASE_EGG;
  int st = doc["stage"] | (int)AGE_JUNIOR;

  if (p == (int)PHASE_HATCHING) p = (int)PHASE_EGG;

  phase = (GamePhase)iClamp(p, (int)PHASE_EGG, (int)PHASE_TOMB);
  pet.stage = (AgeStage)iClamp(st, (int)AGE_JUNIOR, (int)AGE_SENIOR);

  pet.ageMin = doc["ageMin"] | 0UL;
  pet.evolveProgressMin = doc["evolveProgressMin"] | 0UL;
  pet.vivant = doc["vivant"] | true;

  const char* nm = doc["name"] | "???";
  strncpy(petName, nm, sizeof(petName)-1);
  petName[sizeof(petName)-1] = 0;

  JsonObject ps = doc["pet"].as<JsonObject>();
  pet.faim    = (float)iClamp(ps["faim"]    | 60, 0, 100);
  pet.soif    = (float)iClamp(ps["soif"]    | 60, 0, 100);
  pet.hygiene = (float)iClamp(ps["hygiene"] | 80, 0, 100);
  pet.humeur  = (float)iClamp(ps["humeur"]  | 60, 0, 100);
  pet.energie = (float)iClamp(ps["energie"] | 100,0, 100);
  pet.fatigue = (float)iClamp(ps["fatigue"] | 100,0, 100);
  pet.amour   = (float)iClamp(ps["amour"]   | 60, 0, 100);
  pet.caca    = (float)iClamp(ps["caca"]    | 0,  0, 100);
  pet.sante   = clampf((float)(ps["sante"] | 80.0f), 0.0f, 100.0f);

#if ENABLE_AUDIO
  audioMode = (AudioMode)iClamp(doc["audioMode"] | (int)AUDIO_TOTAL, (int)AUDIO_OFF, (int)AUDIO_TOTAL);
  audioVolumePercent = (uint8_t)iClamp(doc["audioVol"] | (int)audioVolumePercent, 1, 100);
#else
  audioMode = AUDIO_OFF;
#endif

  if (!pet.vivant || pet.sante <= 0.0f) {
    pet.vivant = false;
    phase = PHASE_TOMB;
  }

  nextSlotIsA = !bestIsA;

  applyLoadedToRuntime(now);
  Serial.printf("[SAVE] Loaded seq=%lu from %s\n",
                (unsigned long)saveSeq,
                bestIsA ? "A" : "B");
  return true;
}

static bool writeSlotFile(const char* tmpPath, const char* finalPath, const char* why) {
  StaticJsonDocument<768> doc;
  doc["ver"] = 1;
  doc["seq"] = saveSeq;
  doc["why"] = why;

  doc["phase"] = (int)phase;
  doc["stage"] = (int)pet.stage;
  doc["ageMin"] = (uint32_t)pet.ageMin;
  doc["evolveProgressMin"] = (uint32_t)pet.evolveProgressMin;
  doc["vivant"] = pet.vivant;
  doc["name"] = petName;

#if ENABLE_AUDIO
  doc["audioMode"] = (int)audioMode;
  doc["audioVol"] = (int)audioVolumePercent;
#endif

  JsonObject ps = doc.createNestedObject("pet");
  ps["faim"]    = fToI100(pet.faim);
  ps["soif"]    = fToI100(pet.soif);
  ps["hygiene"] = fToI100(pet.hygiene);
  ps["humeur"]  = fToI100(pet.humeur);
  ps["energie"] = fToI100(pet.energie);
  ps["fatigue"] = fToI100(pet.fatigue);
  ps["amour"]   = fToI100(pet.amour);
  ps["caca"]    = fToI100(pet.caca);
  ps["sante"]   = pet.sante;

  if (SD.exists(tmpPath)) SD.remove(tmpPath);
  File f = SD.open(tmpPath, FILE_WRITE);
  if (!f) return false;

  if (serializeJson(doc, f) == 0) { f.close(); return false; }
  f.flush();
  f.close();

  if (SD.exists(finalPath)) SD.remove(finalPath);
  if (!SD.rename(tmpPath, finalPath)) {
    SD.remove(tmpPath);
    return false;
  }
  return true;
}

static bool saveNow(uint32_t now, const char* why) {
  if (!sdReady) return false;

  saveSeq++;
  const bool useA = nextSlotIsA;
  nextSlotIsA = !nextSlotIsA;

  const char* tmp  = useA ? TMP_A  : TMP_B;
  const char* fin  = useA ? SAVE_A : SAVE_B;

  bool ok = writeSlotFile(tmp, fin, why);
  if (ok) {
    lastSaveAt = now;
  } else {
    Serial.println("[SAVE] FAIL write");
  }
  return ok;
}

static inline void saveMaybeEachMinute(uint32_t now) {
  if (!sdReady) return;
  if (lastSaveAt == 0) lastSaveAt = now;
  if ((uint32_t)(now - lastSaveAt) >= SAVE_EVERY_MS) {
    saveNow(now, "minute");
  }
}

// ================== UI rebuild ==================
static void rebuildUISprites(uint32_t now) {
  uiTop.fillSprite(KEY);

  char line[96];
  bool showStatusLine = false;
  uint16_t statusColor = criticalBlinkOn(now) ? TFT_RED : TFT_WHITE;
if (phase == PHASE_EGG || phase == PHASE_HATCHING) {

  // ⚠️ Astuce: si ton écran/typo gère mal "œ" et "…",
  // garde la version ASCII ci-dessous (oeuf / ...)
  const char* l1 = "Un oeuf de dinosaure...";
  const char* l2 = "Ce n'est pas grand-chose, pour l'instant.";
  const char* l3 = "Mais c'est une vie en devenir.";
  const char* l4 = "A toi d'en prendre soin.";

  // Positionnement
  int x = 10;
  int y = 6;

  // Ligne 1 un peu plus grande
  uiTop.setTextSize(2);
  uiTop.setTextColor(TFT_BLACK, KEY);
  uiTop.setCursor(x + 1, y + 1); uiTop.print(l1);
  uiTop.setTextColor(TFT_WHITE, KEY);
  uiTop.setCursor(x,     y);     uiTop.print(l1);

  // Lignes suivantes en taille normale (sinon ça risque de dépasser)
  uiTop.setTextSize(1);

  int y2 = y + 22;  // espace après la ligne taille 2
  uiTop.setTextColor(TFT_BLACK, KEY);
  uiTop.setCursor(x + 1, y2 + 1);        uiTop.print(l2);
  uiTop.setCursor(x + 1, y2 + 13 + 1);   uiTop.print(l3);
  uiTop.setCursor(x + 1, y2 + 26 + 1);   uiTop.print(l4);

  uiTop.setTextColor(TFT_WHITE, KEY);
  uiTop.setCursor(x,     y2);            uiTop.print(l2);
  uiTop.setCursor(x,     y2 + 13);       uiTop.print(l3);
  uiTop.setCursor(x,     y2 + 26);       uiTop.print(l4);

  showStatusLine = false;  // IMPORTANT: sinon il ré-affiche "line" en haut
}
 else if (phase == PHASE_RESTREADY) {
    const char* l1 = "Felicitations !";
    const char* l2 = "Grace a vous,";
    const char* l3 = "votre dinosaure";
    const char* l4 = "a eu une belle vie.";
    const char* l5 = "Il repose desormais";
    const char* l6 = "au paradis des dinos.";

    uiTop.setTextSize(1);
    uiTop.setTextColor(TFT_BLACK, KEY);
    uiTop.setCursor(11,  9);  uiTop.print(l1);
    uiTop.setCursor(11, 23);  uiTop.print(l2);
    uiTop.setCursor(11, 37);  uiTop.print(l3);
    uiTop.setCursor(11, 51);  uiTop.print(l4);
    uiTop.setCursor(11, 65);  uiTop.print(l5);
    uiTop.setCursor(11, 79);  uiTop.print(l6);

    uiTop.setTextColor(statusColor, KEY);
    uiTop.setCursor(10,  8);  uiTop.print(l1);
    uiTop.setCursor(10, 22);  uiTop.print(l2);
    uiTop.setCursor(10, 36);  uiTop.print(l3);
    uiTop.setCursor(10, 50);  uiTop.print(l4);
    uiTop.setCursor(10, 64);  uiTop.print(l5);
    uiTop.setCursor(10, 78);  uiTop.print(l6);
  } else if (phase == PHASE_TOMB || phase == PHASE_RESTREADY) {

  // 4 lignes (évite les accents si ton font les gère mal)
  const char* l1 = "Vous l'avez neglige.";
  const char* l2 = "Vous aviez la charge";
  const char* l3 = "de veiller sur lui...";
  const char* l4 = "et vous l'avez perdu.";

  // Texte plus gros + ombre
  uiTop.setTextSize(2);

  // Ombre (noir) puis texte (blanc)
  uiTop.setTextColor(TFT_BLACK, KEY);
  uiTop.setCursor(7,  6);  uiTop.print(l1);
  uiTop.setCursor(7,  26); uiTop.print(l2);
  uiTop.setCursor(7,  46); uiTop.print(l3);
  uiTop.setCursor(7,  66); uiTop.print(l4);

  uiTop.setTextColor(TFT_WHITE, KEY);
  uiTop.setCursor(6,  5);  uiTop.print(l1);
  uiTop.setCursor(6,  25); uiTop.print(l2);
  uiTop.setCursor(6,  45); uiTop.print(l3);
  uiTop.setCursor(6,  65); uiTop.print(l4);

  // IMPORTANT: on sort ici sinon le code plus bas ré-affiche autre chose

  } else {
    snprintf(line, sizeof(line), "Sante:%d  Age:%lum  %s",
             (int)roundf(pet.sante),
             (unsigned long)pet.ageMin,
             stageLabel(pet.stage));
    showStatusLine = true;
  }

  if (showStatusLine) {
    uiTop.setTextSize(1);
    uiTextShadow(uiTop, 6, 2, line, statusColor, KEY);
  }

  int nameW = 0;
if (phase == PHASE_ALIVE) {
    uiTop.setTextDatum(top_right);
    uiTop.setTextSize(1);
    uiTop.setTextColor(TFT_WHITE, KEY);
    uiTop.drawString(petName, SW - 6, 2);
    uiTop.setTextDatum(top_left);
    nameW = uiTop.textWidth(petName);
  }

  if (phase == PHASE_ALIVE) {
    uint32_t need = (pet.stage == AGE_JUNIOR) ? EVOLVE_JUNIOR_TO_ADULT_MIN
                 : (pet.stage == AGE_ADULTE) ? EVOLVE_ADULT_TO_SENIOR_MIN
                 : EVOLVE_SENIOR_TO_REST_MIN;
    float p = (need > 0) ? (100.0f * (float)min(pet.evolveProgressMin, need) / (float)need) : 0;

    int leftW = uiTop.textWidth(line);
    int x0 = 6 + leftW + 6;
    int x1 = (SW - 6) - nameW - 8;
    int w = x1 - x0;
    int h = 6;
    int y = 3;

    if (w >= 40) {
      uiTop.drawRoundRect(x0, y, w, h, 3, 0xFFFF);
      uiTop.fillRoundRect(x0+1, y+1, w-2, h-2, 2, TFT_WHITE);
      int fw = (int)roundf(((w-2) * p) / 100.0f);
      if (fw > 0) uiTop.fillRoundRect(x0+1, y+1, fw, h-2, 2, 0x07E0);
    }
  }

    if (phase == PHASE_ALIVE) {
    int pad = 6;
    int cols = 4;
    int w = (SW - pad*(cols+1)) / cols;
    int h = 12;

    int y1 = 25;
    int y2 = 50;

    int x = pad;
    drawBarRound(uiTop, x, y1, w, h, pet.faim,    "Faim",    COL_FAIM);    x += w + pad;
    drawBarRound(uiTop, x, y1, w, h, pet.soif,    "Soif",    COL_SOIF);    x += w + pad;
    drawBarRound(uiTop, x, y1, w, h, pet.hygiene, "Hygiene", COL_HYGIENE); x += w + pad;
    drawBarRound(uiTop, x, y1, w, h, pet.humeur,  "Bonheur",  COL_HUMEUR);

    x = pad;
    drawBarRound(uiTop, x, y2, w, h, pet.energie, "Energie", COL_ENERGIE); x += w + pad;
    drawBarRound(uiTop, x, y2, w, h, (100.0f - pet.fatigue), "Fatigue", COL_FATIGUE); x += w + pad;
    drawBarRound(uiTop, x, y2, w, h, pet.amour,   "Amour",   COL_AMOUR2);  x += w + pad;
    drawBarRound(uiTop, x, y2, w, h, pet.caca,    "Caca",    COL_CACA);
  }

  // Zone sous les barres (à la place de l'activity bar quand on est libre)
const int TOP_BTN_Y = 77;
const int TOP_BTN_W = 120;
const int TOP_BTN_H = 18;
const int TOP_BTN_PAD = 8;

  bool canTopBtns = (phase == PHASE_ALIVE) && (appMode == MODE_PET) && (!task.active) && (state != ST_SLEEP);

  if (activityVisible) {
    // si une action est en cours -> on affiche la barre
    float p = activityProgress(now);
    drawActivityBar(uiTop, SW/2, TOP_BTN_Y, 140, TOP_BTN_H, p, activityText);
#if USE_TOP_QUICK_BUTTONS
  } else if (canTopBtns) {
    // sinon -> 2 gros boutons Manger/Boire sous les barres
    auto drawTopBtn = [&](int x, const char* lab, uint16_t baseCol, bool disabled) {
      uint16_t fill   = disabled ? 0xC618 : TFT_WHITE;
      uint16_t fg     = TFT_BLACK;
      uint16_t border = baseCol;

      if (!disabled) {
        // style "couleur quand actif" si tu veux, sinon laisse blanc
        // fill = TFT_WHITE;
        // fg = TFT_BLACK;
      }

      uiTop.fillRoundRect(x, TOP_BTN_Y, TOP_BTN_W, TOP_BTN_H, 8, fill);
      uiTop.drawRoundRect(x, TOP_BTN_Y, TOP_BTN_W, TOP_BTN_H, 8, border);

      uiTop.setTextDatum(middle_center);
      uiTop.setTextColor(fg, fill);
      uiTop.setTextSize(2);
      if (uiTop.textWidth(lab) > (TOP_BTN_W - 6)) uiTop.setTextSize(1);
      uiTop.drawString(lab, x + TOP_BTN_W/2, TOP_BTN_Y + TOP_BTN_H/2);
      uiTop.setTextDatum(top_left);
    };

    bool cdEat   = ((int32_t)(now - cdUntil[(int)UI_MANGER]) < 0);
    bool cdDrink = ((int32_t)(now - cdUntil[(int)UI_BOIRE ]) < 0);

    int xL = TOP_BTN_PAD;
    int xR = SW - TOP_BTN_PAD - TOP_BTN_W;

    drawTopBtn(xL, "Manger", btnColorForAction(UI_MANGER), cdEat);
    drawTopBtn(xR, "Boire",  btnColorForAction(UI_BOIRE),  cdDrink);
#endif
  }


  uiBot.fillSprite(KEY);
  uiBot.setTextDatum(middle_center);

  const uint8_t nbtn = uiButtonCount();
// --- Layout barre du bas (centrée, largeur fixe) ---
const int GAP = 6;     // espace entre boutons
const int yy  = 8;     // <-- plus grand => boutons moins hauts
const int r   = 8;

int bw = uiButtonWidth(nbtn);
int bh = UI_BOT_H - (yy * 2);

int totalW = (int)nbtn * bw + ((int)nbtn - 1) * GAP;
int startX = (SW - totalW) / 2;




  bool hardBusy = (phase == PHASE_HATCHING || phase == PHASE_TOMB);

  for (int i = 0; i < (int)nbtn; i++) {
    int xx = startX + i*(bw + GAP);
    bool sel = (i == (int)uiSel);

    uint16_t fill = TFT_WHITE;
    uint16_t fg   = TFT_BLACK;
    uint16_t border = 0x7BEF;

    const char* lab = "?";
    uint16_t baseCol = 0x7BEF;

    bool disabledLook = false;

    if (nbtn == 1) {
      lab = uiSingleLabel();
      baseCol = uiSingleColor();
      fill = sel ? baseCol : TFT_WHITE;
      fg   = sel ? TFT_WHITE : TFT_BLACK;
      border = baseCol;
      if (hardBusy) disabledLook = true;
    } else {
      UiAction a = uiAliveActionAt((uint8_t)i);
      baseCol = btnColorForAction(a);
      lab = (a == UI_AUDIO) ? "" : btnLabel(a);

      bool onCooldown = ((int32_t)(now - cdUntil[(int)a]) < 0);
      bool busy = task.active || (state == ST_SLEEP) || (appMode != MODE_PET);

      if ((busy || onCooldown) && a != UI_AUDIO) disabledLook = true;

      if (!disabledLook) {
        fill = sel ? baseCol : TFT_WHITE;
        fg   = sel ? TFT_WHITE : TFT_BLACK;
        border = baseCol;
      } else {
        fill = 0xC618;
        fg   = TFT_BLACK;
        border = baseCol;
      }
    }

    if (hardBusy) { fill = 0xC618; fg = TFT_BLACK; border = 0x7BEF; }

    uiBot.fillRoundRect(xx, yy, bw, bh, r, fill);
    uiBot.drawRoundRect(xx, yy, bw, bh, r, border);

    if (sel) {
      uiBot.drawRoundRect(xx + 2, yy + 2, bw - 4, bh - 4, max(1, r - 2), TFT_WHITE);
    }

    if (lab[0] != '\0') {
      uiBot.setTextColor(fg, fill);
      uiBot.setTextSize(2);
      if (uiBot.textWidth(lab) > (bw - 6)) uiBot.setTextSize(1);
      uiBot.drawString(lab, xx + bw/2, yy + bh/2);
    } else {
      drawAudioIcon(uiBot, xx, yy, bw, bh, fg, audioMode);
    }
  }
}

// ================== Overlay UI into band ==================
static inline void overlayKeyedSpriteIntoBand(int dstYInBand, const uint16_t* src, int srcW, int srcH, int srcY0, int copyH) {
  uint16_t* dst = (uint16_t*)band.getBuffer();
  for (int j = 0; j < copyH; j++) {
    int sy = srcY0 + j;
    int dy = dstYInBand + j;
    if (sy < 0 || sy >= srcH) continue;
    if (dy < 0 || dy >= band.height()) continue;

    const uint16_t* srow = src + sy * srcW;
    uint16_t* drow = dst + dy * SW;

    for (int x = 0; x < SW; x++) {
      uint16_t c = srow[x];
      if (c == KEY || c == KEY_SWAP) continue;
      if (swap16(c) == KEY) continue;
      drow[x] = c;
    }
  }
}
static void overlayUIIntoBand(int bandY, int bh) {
  int top0 = 0, top1 = UI_TOP_H;
  int a0 = max(bandY, top0);
  int a1 = min(bandY + bh, top1);
  if (a1 > a0) {
    int h = a1 - a0;
    int srcY = a0 - top0;
    int dstY = a0 - bandY;
    overlayKeyedSpriteIntoBand(dstY, (uint16_t*)uiTop.getBuffer(), SW, UI_TOP_H, srcY, h);
  }

  int bot0 = SH - UI_BOT_H, bot1 = SH;
  int b0 = max(bandY, bot0);
  int b1 = min(bandY + bh, bot1);
  if (b1 > b0) {
    int h = b1 - b0;
    int srcY = b0 - bot0;
    int dstY = b0 - bandY;
    overlayKeyedSpriteIntoBand(dstY, (uint16_t*)uiBot.getBuffer(), SW, UI_BOT_H, srcY, h);
  }
}

// ================== DECOR ==================
static void drawMountainImagesBand(float camX, int bandY) {
  const int w = (int)MONT_W;
  const int h = (int)MONT_H;
  const int yOnGround = GROUND_Y - h;

  float px = camX * 0.25f;
  const int spacing = 180;

  int first = (int)floor((px - SW) / spacing) - 2;
  int last  = (int)floor((px + 2 * SW) / spacing) + 2;

  for (int i = first; i <= last; i++) {
    uint32_t hh = hash32(i * 911);
    int jitter = (int)(hh % 50) - 25;
    int x = i * spacing - (int)px + jitter;
    int yLocal = yOnGround - bandY;
    if (yLocal >= band.height() || yLocal + h <= 0) continue;
    drawImageKeyedOnBand(MONT_IMG, w, h, x, yLocal);
  }
}
static void drawGroundBand(float camX, int bandY) {
  int gy = GROUND_Y - bandY;
  band.fillRect(0, gy, SW, SH - GROUND_Y, C_GROUND);
  for (int y = gy; y < band.height(); y += 8) {
    if (y >= 0) band.drawFastHLine(0, y, SW, C_GLINE);
  }
  int shift = (int)camX;
  for (int x = -40; x < SW + 40; x += 32) {
    int xx = x - imod(shift, 32);
    int yy = (GROUND_Y + 18 + (imod(x + shift, 64) / 8)) - bandY;
    band.fillRect(xx, yy, 10, 3, C_GROUND2);
  }
}
static void drawTreesMixedBand(float camX, int bandY) {
  float px = camX * 0.60f;
  const int spacing = 120;
  int first = (int)floor((px - SW) / spacing) - 3;
  int last  = (int)floor((px + 2 * SW) / spacing) + 3;

  for (int i = first; i <= last; i++) {
    uint32_t hh = hash32(i * 1337);
    if ((hh % 2) != 0) continue;
    int jitter = (int)((hh >> 8) % 50) - 25;
    int x = i * spacing - (int)px + jitter;

    bool useArbre = ((hh % 3) == 0);
    const uint16_t* img = useArbre ? ARBRE_IMG : SAPIN_IMG;
    int w = useArbre ? (int)ARBRE_W : (int)SAPIN_W;
    int h = useArbre ? (int)ARBRE_H : (int)SAPIN_H;

    int yOnGround = GROUND_Y - h;
    int yLocal = yOnGround - bandY;
    if (yLocal >= band.height() || yLocal + h <= 0) continue;
    drawImageKeyedOnBand(img, w, h, x, yLocal);
  }
}

static void drawFixedObjectsBand(float camX, int bandY) {
  {
    const uint16_t* img = berriesLeftAvailable ? BBAIE_IMG : BSANS_IMG;
    int w = berriesLeftAvailable ? (int)BBAIE_W : (int)BSANS_W;
    int h = berriesLeftAvailable ? (int)BBAIE_H : (int)BSANS_H;
    int x = (int)roundf(bushLeftX - camX);
    int yOnGround = GROUND_Y - h + BUSH_Y_OFFSET;
    int yLocal = yOnGround - bandY;
    if (!(yLocal >= band.height() || yLocal + h <= 0) && !(x > SW || x + w < 0)) {
      drawImageKeyedOnBand(img, w, h, x, yLocal);
    }
  }

  if (puddleVisible) {
    const uint16_t* img = FLAQUE_IMG;
    int w = (int)FLAQUE_W, h = (int)FLAQUE_H;
    int x = (int)roundf(puddleX - camX);
    int yOnGround = GROUND_Y - h + PUDDLE_Y_OFFSET;
    int yLocal = yOnGround - bandY;
    if (!(yLocal >= band.height() || yLocal + h <= 0) && !(x > SW || x + w < 0)) {
      drawImageKeyedOnBand(img, w, h, x, yLocal);
    }
  }

  if (poopVisible) {
    int w = (int)poop::W;
    int h = (int)poop::H;
    int x = (int)roundf(poopWorldX - camX);
    int yOnGround = GROUND_Y - h + 18;
    int yLocal = yOnGround - bandY;
    if (!(yLocal >= band.height() || yLocal + h <= 0) && !(x > SW || x + w < 0)) {
      drawImageKeyedOnBand(poop::dino_caca_003, w, h, x, yLocal);
    }
  }
}

static void drawWorldBand(float camX, int bandY, int bh) {
  band.fillRect(0, 0, SW, BAND_H, C_SKY);
  band.setClipRect(0, 0, SW, bh);
  drawMountainImagesBand(camX, bandY);
  drawGroundBand(camX, bandY);
  drawTreesMixedBand(camX, bandY);
  drawFixedObjectsBand(camX, bandY);
}

// ================== RENDER ==================
static float lastCamX = 0.0f;
static int lastBandMin = -1;
static int lastBandMax = -1;

static inline void pushBandToScreen(int y0, int bh) {
  uint16_t* buf = (uint16_t*)band.getBuffer();
  tft.pushImage(0, y0, SW, bh, buf);
}

static inline void renderOneBand(int y0, int bh, int dinoX, int dinoY, const uint16_t* frame, bool flipX, uint8_t shade) {
  drawWorldBand(camX, y0, bh);

  if (phase == PHASE_ALIVE) {
    int DW = triW(pet.stage);
    int DH = triH(pet.stage);
    if (dinoY < y0 + bh && dinoY + DH > y0) {
      drawImageKeyedOnBand(frame, DW, DH, dinoX, dinoY - y0, flipX, shade);
    }
  } else if (phase == PHASE_EGG || phase == PHASE_HATCHING) {
    const uint16_t* eggFrame = egg::dino_oeuf_001;
    if (phase == PHASE_HATCHING) {
      if (hatchIdx == 0) eggFrame = egg::dino_oeuf_001;
      else if (hatchIdx == 1) eggFrame = egg::dino_oeuf_002;
      else if (hatchIdx == 2) eggFrame = egg::dino_oeuf_003;
      else eggFrame = egg::dino_oeuf_004;
    }
    int w = (int)egg::W, h = (int)egg::H;
    float t = (float)millis() * 0.008f;
    int bob = (int)roundf(sinf(t) * 2.0f);
    int ex = dinoX;
    int ey = (GROUND_Y - 40) + bob;
    if (ey < y0 + bh && ey + h > y0) drawImageKeyedOnBand(eggFrame, w, h, ex, ey - y0);
  } else if (phase == PHASE_TOMB || phase == PHASE_RESTREADY) {
    int w = (int)tombe_W;
    int h = (int)tombe_H;
    int tx = (SW - w) / 2;
    int ty = (GROUND_Y - h + 10);
    if (ty < y0 + bh && ty + h > y0) drawImageKeyedOnBand(tombe, w, h, tx, ty - y0);
  }

  overlayUIIntoBand(y0, bh);
  pushBandToScreen(y0, bh);
}

static void renderFrameOptimized(int dinoX, int dinoY, const uint16_t* frame, bool flipX, uint8_t shade) {
  bool camMoved = (fabsf(camX - lastCamX) > 0.001f);
  int DH = (phase == PHASE_ALIVE) ? triH(pet.stage)
           : (phase == PHASE_TOMB || phase == PHASE_RESTREADY) ? (int)tombe_H
           : 60;

  if (camMoved) {
    for (int y = 0; y < SH; y += BAND_H) {
      int bh = (y + BAND_H <= SH) ? BAND_H : (SH - y);
      renderOneBand(y, bh, dinoX, dinoY, frame, flipX, shade);
    }
    lastBandMin = 0;
    lastBandMax = (SH - 1) / BAND_H;
  } else {
    int ymin = dinoY - 2;
    int ymax = dinoY + DH + 2;
    int bmin = clampi(ymin / BAND_H, 0, (SH - 1) / BAND_H);
    int bmax = clampi(ymax / BAND_H, 0, (SH - 1) / BAND_H);

    if (uiForceBands) {
      int uiTopBandMax = (UI_TOP_H - 1) / BAND_H;
      int botStart = SH - UI_BOT_H;
      int uiBotBandMin = botStart / BAND_H;
      int uiBotBandMax = (SH - 1) / BAND_H;
      bmin = min(bmin, 0);
      bmax = max(bmax, uiTopBandMax);
      bmin = min(bmin, uiBotBandMin);
      bmax = max(bmax, uiBotBandMax);
    }

    if (lastBandMin != -1) bmin = min(bmin, lastBandMin);
    if (lastBandMax != -1) bmax = max(bmax, lastBandMax);

    for (int bi = bmin; bi <= bmax; bi++) {
      int y = bi * BAND_H;
      int bh = (y + BAND_H <= SH) ? BAND_H : (SH - y);
      renderOneBand(y, bh, dinoX, dinoY, frame, flipX, shade);
    }
    lastBandMin = bmin;
    lastBandMax = bmax;
  }

  lastCamX = camX;
}

// ================== enterState / timing ==================
static inline uint16_t frameMsForState(TriState st) {
  switch (st) {
    case ST_BLINK: return 90;
    case ST_SLEEP: return 180;
    case ST_SIT:   return 140;
    case ST_EAT:   return 120;
    case ST_DEAD:  return 220;
    default:       return 120;
  }
}
static void enterState(TriState st, uint32_t now) {
  if (state == st) return;
  state = st;
  animIdx = 0;
  nextAnimTick = now + frameMsForState(st);
}

// ================== Effects ==================
static void applyTaskEffects(TaskKind k, uint32_t now) {
  if (!pet.vivant) return;

  float gm = gainMulForStage(pet.stage);
  auto add = [&](float &v, float dv){ v = clamp01f(v + dv * gm); };

  switch (k) {
    case TASK_EAT:
      add(pet.faim,    EAT_HUNGER);
      add(pet.energie, EAT_ENERGY);
      add(pet.fatigue, EAT_FATIGUE);
      add(pet.soif,    EAT_THIRST);
      add(pet.caca,    EAT_POOP);
      break;

    case TASK_DRINK:
      add(pet.soif,    DRINK_THIRST);
      add(pet.energie, DRINK_ENERGY);
      add(pet.fatigue, DRINK_FATIGUE);
      add(pet.caca,    DRINK_POOP);
      break;

    case TASK_WASH:
      add(pet.hygiene, WASH_HYGIENE);
      add(pet.humeur,  WASH_MOOD);
      add(pet.energie, WASH_ENERGY);
      add(pet.fatigue, WASH_FATIGUE);
      break;

    case TASK_PLAY:
      add(pet.humeur,  PLAY_MOOD);
      add(pet.energie, PLAY_ENERGY);
      add(pet.fatigue, PLAY_FATIGUE);
      add(pet.faim,    PLAY_HUNGER);
      add(pet.soif,    PLAY_THIRST);
      break;

    case TASK_POOP:
      pet.caca = clamp01f(POOP_SET);
      add(pet.hygiene, POOP_HYGIENE);
      add(pet.humeur,  POOP_MOOD);
      add(pet.energie, POOP_ENERGY);
      add(pet.fatigue, POOP_FATIGUE);

      poopVisible = true;
      poopUntil = now + 8000;
      {
        float off = movingRight ? -12.0f : +12.0f;
        poopWorldX = worldX + off;
      }
      break;

    case TASK_HUG:
      add(pet.amour,   HUG_LOVE);
      add(pet.humeur,  HUG_MOOD);
      add(pet.fatigue, HUG_FATIGUE);
      add(pet.energie, HUG_ENERGY);
      break;

    default: break;
  }

  uiSpriteDirty = true;
  uiForceBands  = true;

  // save après action importante
  saveNow(now, "action");
}

// ================== Start task ==================
static bool startTask(TaskKind k, uint32_t now) {
  if (!pet.vivant) { setMsg("Il est mort...", now, 2000); return false; }
  if (phase != PHASE_ALIVE && phase != PHASE_RESTREADY) return false;
  if (state == ST_SLEEP && k != TASK_SLEEP) return false;

  // IMPORTANT: LAVER / JOUER passent par mini-jeux -> on refuse ici
  if (k == TASK_WASH || k == TASK_PLAY) return false;

  task.active = true;
  task.kind   = k;
  task.ph     = PH_GO;
  task.doUntil = 0;
  task.startedAt = now;
  task.plannedTotal = 0;
  task.doMs = 0;

  if (k == TASK_EAT) {
    task.targetX = clampf(eatSpotX(), worldMin, worldMax);
    task.returnX = homeX;

    task.doMs = scaleByFatigueAndAge(BASE_EAT_MS);
    task.plannedTotal = task.doMs;

    activityShowFull(now, "En route pour manger...");
    enterState(ST_WALK, now);
  }
  else if (k == TASK_DRINK) {
    task.targetX = clampf(drinkSpotX(), worldMin, worldMax);
    task.returnX = homeX;

    task.doMs = scaleByFatigueAndAge(BASE_DRINK_MS);
    task.plannedTotal = task.doMs;

    activityShowFull(now, "En route pour boire...");
    enterState(ST_WALK, now);
  }
  else if (k == TASK_SLEEP) {
    task.targetX = worldX;
    task.returnX = homeX;
    task.plannedTotal = 1;
    activityVisible = true;
    activityStart = now;
    activityEnd   = now;
    strncpy(activityText, "Dort...", sizeof(activityText)-1);
    activityText[sizeof(activityText)-1] = 0;

    task.ph = PH_DO;
    enterState(ST_SLEEP, now);
  }
  else {
    task.targetX = worldX;
    task.returnX = homeX;
    uint32_t tDo = (k==TASK_POOP)? scaleByFatigueAndAge(BASE_POOP_MS)
                : (k==TASK_HUG )? scaleByFatigueAndAge(BASE_HUG_MS)
                : 1500;
    task.plannedTotal = tDo;
    task.ph = PH_DO;
    task.doUntil = now + tDo;

    if (k == TASK_POOP) {
      activityStartTask(now, "Fait caca...", task.plannedTotal);
    } else if (k == TASK_HUG) {
      activityStartTask(now, "Fait un calin...", task.plannedTotal);
    }

    enterState(ST_SIT, now);
  }

  uiSpriteDirty = true;
  uiForceBands  = true;
  return true;
}

// ================== Update task ==================
static uint32_t lastSleepGainAt = 0;

static void updateTask(uint32_t now) {
  if (!task.active) return;

  if (task.kind == TASK_SLEEP) {
    if (lastSleepGainAt == 0) lastSleepGainAt = now;
    uint32_t dt = now - lastSleepGainAt;
    if (dt >= 100) {
      float sec = (float)dt / 1000.0f;
      pet.fatigue = clamp01f(pet.fatigue + SLEEP_GAIN_PER_SEC * sec);
      pet.energie = clamp01f(pet.energie + SLEEP_GAIN_PER_SEC * sec);
      lastSleepGainAt = now;
      uiSpriteDirty = true; uiForceBands = true;
    }
    activitySetText("Dort...");
    return;
  }

  // GO / RETURN (déplacement)
  if (task.ph == PH_GO || task.ph == PH_RETURN) {
    float goal = (task.ph == PH_GO) ? task.targetX : task.returnX;

    if (fabsf(goal - worldX) < 1.2f) {
      worldX = goal;

      if (task.ph == PH_GO) {
        task.ph = PH_DO;

        if (task.kind == TASK_EAT) {
          if (!berriesLeftAvailable) {
            activityShowFull(now, "Plus de baies...");
            task.ph = PH_RETURN;
            enterState(ST_WALK, now);
            return;
          }
          activityShowProgress(now, "Mange...", task.doMs);
          enterState(ST_EAT, now);
          task.doUntil = now + task.doMs;
          return;
        }
        else if (task.kind == TASK_DRINK) {
          if (!puddleVisible) {
            activityShowFull(now, "Plus d'eau...");
            task.ph = PH_RETURN;
            enterState(ST_WALK, now);
            return;
          }
          activityShowProgress(now, "Boit...", task.doMs);
          enterState(ST_EAT, now);
          task.doUntil = now + task.doMs;
          return;
        }

      } else {
        task.active = false;
        enterState(ST_SIT, now);
        activityStopIfFree(now);
        uiSpriteDirty = true; uiForceBands = true;
        return;
      }
    } else {
      enterState(ST_WALK, now);
      movingRight = (goal >= worldX);
      float sp = moveSpeedPxPerFrame();
      float dir = movingRight ? 1.0f : -1.0f;
      worldX += dir * sp;
      worldX = clampf(worldX, worldMin, worldMax);
    }

    if (task.ph == PH_GO) {
      if (task.kind == TASK_EAT) activitySetText("En route pour manger...");
      else if (task.kind == TASK_DRINK) activitySetText("En route pour boire...");
    } else if (task.ph == PH_RETURN) {
      if (task.kind == TASK_EAT) activitySetText("Revient apres manger...");
      else if (task.kind == TASK_DRINK) activitySetText("Revient apres boire...");
    }

    return;
  }

  // DO (manger/boire)
  if (task.ph == PH_DO) {
    if (task.doUntil != 0 && (int32_t)(now - task.doUntil) >= 0) {

      if (task.kind == TASK_EAT) {
        berriesLeftAvailable = false;
        berriesRespawnAt = now + (uint32_t)random(6000, 14000);
      }
      if (task.kind == TASK_DRINK) {
        puddleVisible = false;
        puddleRespawnAt = now + 2000;
      }

      if (task.kind == TASK_EAT) {
        playRTTTLOnce(RTTTL_EAT_INTRO, AUDIO_PRIO_MED);
      } else if (task.kind == TASK_DRINK) {
        playRTTTLOnce(RTTTL_DRINK_INTRO, AUDIO_PRIO_MED);
      } else if (task.kind == TASK_POOP) {
        playRTTTLOnce(RTTTL_POOP_INTRO, AUDIO_PRIO_HIGH);
      } else if (task.kind == TASK_HUG) {
        playRTTTLOnce(RTTTL_HUG_INTRO, AUDIO_PRIO_MED);
      }

      applyTaskEffects(task.kind, now);

      task.ph = PH_RETURN;

      if (task.kind == TASK_EAT) activityShowFull(now, "Revient apres manger...");
      else if (task.kind == TASK_DRINK) activityShowFull(now, "Revient apres boire...");

      enterState(ST_WALK, now);
    }
    return;
  }
}

// ================== Evolution (NON consécutif) ==================
static inline bool goodEvolve80() {
  return (pet.faim   >= EVOLVE_THR &&
          pet.soif   >= EVOLVE_THR &&
          pet.humeur >= EVOLVE_THR &&
          pet.amour  >= EVOLVE_THR);
}
static void evolutionTick(uint32_t now) {
  if (!pet.vivant) return;
  if (phase != PHASE_ALIVE) return;

  if (goodEvolve80()) pet.evolveProgressMin++;

  uint32_t need = (pet.stage == AGE_JUNIOR) ? EVOLVE_JUNIOR_TO_ADULT_MIN
               : (pet.stage == AGE_ADULTE) ? EVOLVE_ADULT_TO_SENIOR_MIN
               : EVOLVE_SENIOR_TO_REST_MIN;

  if (need == 0) return;

  if (pet.evolveProgressMin >= need) {
    pet.evolveProgressMin = 0;

    if (pet.stage == AGE_JUNIOR) {
      pet.stage = AGE_ADULTE;
      setMsg("Evolution: Adulte !", now, 2000);
      saveNow(now, "evolve");
    } else if (pet.stage == AGE_ADULTE) {
      pet.stage = AGE_SENIOR;
      setMsg("Evolution: Senior !", now, 2000);
      saveNow(now, "evolve");
    } else {
      phase = PHASE_RESTREADY;
      uiSpriteDirty = true;
      uiForceBands = true;
      saveNow(now, "restready");
    }
  }
}

// ================== Santé / malus caca ==================
static void poopAccidentCheck(uint32_t now) {
  if (pet.caca >= (float)POOP_ACCIDENT_AT) {
    if (!pet.poopAccidentLatched) {
      pet.poopAccidentLatched = true;
      pet.hygiene = 0;
      pet.humeur  = 5;
      pet.sante   = clamp01f(pet.sante - 10);
      pet.caca    = 60;
      setMsg("Accident... Beurk !", now, 2200);
    }
  } else {
    pet.poopAccidentLatched = false;
  }

  if (pet.caca >= POOP_STRESS_THR) {
    pet.hygiene = clamp01f(pet.hygiene - 0.5f);
    pet.humeur  = clamp01f(pet.humeur  - 0.5f);
  }
}

static void updateHealthTick(uint32_t now) {
  (void)now;
  if (!pet.vivant) return;

  float ds = 0;
  if (pet.faim    < 15) ds -= 0.02f;
  if (pet.soif    < 15) ds -= 0.02f;
  if (pet.hygiene < 15) ds -= 0.01f;
  if (pet.humeur  < 10) ds -= 0.01f;
  if (pet.energie < 10) ds -= 0.01f;
  if (pet.fatigue < 10) ds -= 0.01f;
  if (pet.amour   < 10) ds -= 0.005f;
  if (pet.caca >= 95) ds -= 0.01f;

  if (ds < 0) pet.sante = clamp01f(pet.sante + ds);

  if (pet.sante <= 0) {
    handleDeath(now);
  }
}
static void handleDeath(uint32_t now) {
  pet.vivant = false;
  phase = PHASE_TOMB;
  task.active = false;
  task.kind = TASK_NONE;
  appMode = MODE_PET;
  enterState(ST_DEAD, now);
  activityShowFull(now, "Vous n'avez pas pris soin. Il a perdu la vie.");
  uiSel = 0;
  uiSpriteDirty = true;
  uiForceBands = true;
  saveNow(now, "dead");
}

static void eraseSavesAndRestart() {
  if (sdReady) {
    if (SD.exists(SAVE_A)) SD.remove(SAVE_A);
    if (SD.exists(SAVE_B)) SD.remove(SAVE_B);
    if (SD.exists(TMP_A)) SD.remove(TMP_A);
    if (SD.exists(TMP_B)) SD.remove(TMP_B);
  }
  delay(50);
  ESP.restart();
}

// ================== Tick pet (1 minute * SIM_SPEED) ==================
static uint32_t lastPetTick = 0;

static void updatePetTick(uint32_t now) {
  if (!pet.vivant) return;
  if (phase != PHASE_ALIVE && phase != PHASE_RESTREADY) return;

  bool sleeping = (state == ST_SLEEP);
  auto add = [](float &v, float dv){ v = clamp01f(v + dv); };

  if (sleeping) {
    add(pet.faim,    -1.0f);
    add(pet.soif,    -1.0f);
    add(pet.hygiene, -0.5f);
    add(pet.humeur,  +0.2f);
    add(pet.amour,   -0.2f);
    add(pet.sante,   + 0.1f);
    add(pet.caca,    SLEEP_POOP_D);
  } else {
    add(pet.faim,    AWAKE_HUNGER_D);
    add(pet.soif,    AWAKE_THIRST_D);
    add(pet.hygiene, AWAKE_HYGIENE_D);
    add(pet.humeur,  AWAKE_MOOD_D);
    add(pet.energie, AWAKE_ENERGY_D);
    add(pet.fatigue, AWAKE_FATIGUE_D);
    add(pet.amour,   AWAKE_LOVE_D);
    add(pet.caca,    AWAKE_POOP_D);
  }

  poopAccidentCheck(now);
  updateHealthTick(now);

  pet.ageMin++;
  evolutionTick(now);

  uiSpriteDirty = true;
  uiForceBands  = true;
}

// ================== Idle autonome (80% marche / 10% assis / 10% cligne) ==================
static void idleDecide(uint32_t now) {
  if (phase != PHASE_ALIVE) return;
  if (task.active) return;
  if (state == ST_SLEEP) return;
  if (appMode != MODE_PET) return;

  uint32_t r = (uint32_t)random(0, 100);

  if (r < 80) {
    float minX = worldMin + 10.0f;
    float maxX = worldMax - (float)triW(pet.stage) - 10.0f;
    float tx = (float)random((int)minX, (int)maxX);
    idleTargetX = tx;
    idleWalking = true;
    idleUntil = now + (uint32_t)random(3000, 8500); //marche
    enterState(ST_WALK, now);
  } else if (r < 90) {
    idleWalking = false;
    idleUntil = now + (uint32_t)random(900, 3000); // assie
    enterState(ST_SIT, now);
  } else {
    idleWalking = false;
    idleUntil = now + (uint32_t)random(900, 2000);  //cligne
    enterState(ST_BLINK, now);
  }

  nextIdleDecisionAt = now + (uint32_t)random(2000, 5000);
}

static void idleUpdate(uint32_t now) {
  if (phase != PHASE_ALIVE) return;
  if (task.active) return;
  if (state == ST_SLEEP) return;
  if (appMode != MODE_PET) return;

  if (nextIdleDecisionAt == 0 || (int32_t)(now - nextIdleDecisionAt) >= 0) {
    idleDecide(now);
  }

  if (idleUntil != 0 && (int32_t)(now - idleUntil) >= 0) {
    idleUntil = 0;
    enterState(ST_SIT, now);
  }

  if (idleWalking && state == ST_WALK) {
    float goal = idleTargetX;
    if (fabsf(goal - worldX) < 1.2f) {
      worldX = goal;
      idleWalking = false;
      enterState(ST_SIT, now);
      idleUntil = now + (uint32_t)random(400, 1200);
      return;
    }
    movingRight = (goal >= worldX);
    float sp = moveSpeedPxPerFrame();
    float dir = movingRight ? 1.0f : -1.0f;
    worldX += dir * sp;
    worldX = clampf(worldX, worldMin, worldMax);
  }
}



// ================== TACTILE -> UI (AJOUT) ==================
struct TouchDeb {
  bool rawDown = false;
  bool stableDown = false;
  uint32_t lastChange = 0;
  int16_t x = 0, y = 0;
  int8_t  lastBtn = -1;
    int8_t  pressBtn = -1;
  uint32_t pressStart = 0;
  bool longPressFired = false;
};
static TouchDeb touch;
static const uint32_t TOUCH_DEBOUNCE_MS = 25;
static const uint32_t AUDIO_LONG_PRESS_MS = 650;

static inline bool readTouchRaw(int16_t &x, int16_t &y) {
  return readTouchScreen(x, y);
}

// ================== Input UI ==================
static void handleEncoderUI(uint32_t now) {
  if (!encoderEnabled()) return;
  int32_t p;
  noInterrupts(); p = encPos; interrupts();
  int32_t det = detentFromEnc(p);
  int32_t dd  = det - lastDetent;

  uint8_t nbtn = uiButtonCount();

  if (dd != 0) {
    int s = (int)uiSel + (int)dd;
    s %= (int)nbtn; if (s < 0) s += (int)nbtn;
    uiSel = (uint8_t)s;
    lastDetent = det;
    uiSpriteDirty = true;
    uiForceBands  = true;
  }

  bool raw = readBtnPressedRaw();
  if (raw != lastBtnRaw) { lastBtnRaw = raw; btnChangeAt = now; }
  if ((now - btnChangeAt) > BTN_DEBOUNCE_MS) btnState = raw;

  static bool lastStable = false;
  bool pressedEdge = (btnState && !lastStable);
  lastStable = btnState;
  if (!pressedEdge) return;

  uiPressAction(now);
}

static void handleButtonsUI(uint32_t now) {
  if (!buttonsEnabled()) return;
  uint8_t nbtn = uiButtonCount();

  if (btnLeftEdge) {
    int s = (int)uiSel - 1;
    if (s < 0) s = (int)nbtn - 1;
    uiSel = (uint8_t)s;
    uiSpriteDirty = true;
    uiForceBands  = true;
  }

  if (btnRightEdge) {
    int s = (int)uiSel + 1;
    if (s >= (int)nbtn) s = 0;
    uiSel = (uint8_t)s;
    uiSpriteDirty = true;
    uiForceBands  = true;
  }

  if (btnOkEdge) {
    uiPressAction(now);
  }
}

// --- Action commune (encodeur + tactile) ---
static void uiPressAction(uint32_t now) {
  if (appMode != MODE_PET) return; // en mini-jeu, on ne clique pas l'UI gestion

  if (phase == PHASE_EGG) {
    phase = PHASE_HATCHING;
    hatchIdx = 0;
    hatchNext = now + 220;

    activityVisible = true;
    activityStart = now;
    activityEnd   = now + 1200;
    strcpy(activityText, "Eclosion...");
    uiSpriteDirty = true; uiForceBands = true;
    return;
  }

    if (phase == PHASE_TOMB) {
    eraseSavesAndRestart();
    return;
  }

  if (phase == PHASE_HATCHING) return;

  if (phase == PHASE_RESTREADY) {
    resetToEgg(now);
    setMsg("Un nouvel oeuf...", now, 1500);
    if (sdReady) saveNow(now, "rest_new");
    return;
  }

  if (state == ST_SLEEP) {
    task.active = false;
    task.kind = TASK_NONE;
    lastSleepGainAt = 0;

    activityVisible = false;
    activityText[0] = 0;

    enterState(ST_SIT, now);
    playRTTTLOnce(RTTTL_SLEEP_INTRO, AUDIO_PRIO_MED);
    setMsg("Reveille !", now, 1500);
    uiSpriteDirty = true; uiForceBands = true;
    return;
  }

  if (task.active) { setMsg("Occupe !", now, 1200); uiSpriteDirty = true; uiForceBands = true; return; }

uint8_t nbtn = uiButtonCount();
if (nbtn != uiAliveCount()) return;

  UiAction a = uiAliveActionAt(uiSel);

  if ((int32_t)(now - cdUntil[(int)a]) < 0) {
    uint32_t sec = (cdUntil[(int)a] - now) / 1000UL;
    char tmp[48];
    snprintf(tmp, sizeof(tmp), "En recharge (%lus)", (unsigned long)sec);
    setMsg(tmp, now, 1500);
    uiSpriteDirty = true; uiForceBands = true;
    return;
  }

  switch (a) {
    case UI_REPOS:
      cdUntil[a] = now + 1000;
      startTask(TASK_SLEEP, now);
      break;

    case UI_MANGER:
      cdUntil[a] = now + CD_EAT_MS;
      startTask(TASK_EAT, now);
      break;

    case UI_BOIRE:
      cdUntil[a] = now + CD_DRINK_MS;
      startTask(TASK_DRINK, now);
      break;

    case UI_LAVER:
      cdUntil[a] = now + CD_WASH_MS;
      mgBegin(TASK_WASH, now);  // <--- MINI-JEU
      break;

    case UI_JOUER:
      cdUntil[a] = now + CD_PLAY_MS;
      mgBegin(TASK_PLAY, now);  // <--- MINI-JEU
      break;

    case UI_CACA:
      cdUntil[a] = now + CD_POOP_MS;
      startTask(TASK_POOP, now);
      break;

    case UI_CALIN:
      cdUntil[a] = now + CD_HUG_MS;
      startTask(TASK_HUG, now);
      break;

    case UI_AUDIO: {
      if (audioMode == AUDIO_TOTAL) audioMode = AUDIO_LIMITED;
      else if (audioMode == AUDIO_LIMITED) audioMode = AUDIO_OFF;
      else audioMode = AUDIO_TOTAL;

      if (audioMode == AUDIO_OFF) {
        stopAudio();
        setMsg("Audio coupe", now, 1500);
      } else if (audioMode == AUDIO_LIMITED) {
        audioNextAlertAt = now + 20000UL;
        setMsg("Audio limite", now, 1500);
      } else {
        audioNextAlertAt = now + 10000UL;
        setMsg("Audio total", now, 1500);
      }
#if ENABLE_AUDIO
      if (sdReady) saveNow(now, "audio");
#endif
      break;
    }

    default: break;
  }

  uiSpriteDirty = true;
  uiForceBands  = true;
}

static void handleTouchUI(uint32_t now) {
  if (appMode != MODE_PET) return; // en mini-jeu, on gère le tactile dans le mini-jeu si besoin

  int16_t x, y;
  bool down = readTouchRaw(x, y);

  if (down != touch.rawDown) {
    touch.rawDown = down;
    touch.lastChange = now;
    if (down) { touch.x = x; touch.y = y; }
  } else if (down) {
    touch.x = x; touch.y = y;
  }

  bool stableDown = touch.rawDown && (now - touch.lastChange) >= TOUCH_DEBOUNCE_MS;
  bool stableUp   = (!touch.rawDown) && (now - touch.lastChange) >= TOUCH_DEBOUNCE_MS;

  bool pressedEdge  = false;
  bool releasedEdge = false;

  if (!touch.stableDown && stableDown) { touch.stableDown = true; pressedEdge = true; }
  if (touch.stableDown && stableUp) { touch.stableDown = false; releasedEdge = true; }

  if (touch.stableDown || pressedEdge) {
    int ty = touch.y;
    int tx = touch.x;
const int SAFE = 8;                 // évite les zones chiantes tout au bord
if (tx < SAFE) tx = SAFE;
if (tx > SW - 1 - SAFE) tx = SW - 1 - SAFE;
// --- Détection des 2 boutons du haut (Manger / Boire) ---
const int TOP_BTN_Y = 77;
const int TOP_BTN_W = 120;
const int TOP_BTN_H = 18;
const int TOP_BTN_PAD = 8;
const int TOP_BTN_TOUCH_PAD = TOP_BTN_H; // zone tactile agrandie

bool canTopBtns = (phase == PHASE_ALIVE) && (appMode == MODE_PET) && (!task.active) && (state != ST_SLEEP) && (!activityVisible);

#if USE_TOP_QUICK_BUTTONS
if (canTopBtns && ty >= (TOP_BTN_Y - TOP_BTN_TOUCH_PAD) && ty < (TOP_BTN_Y + TOP_BTN_H + TOP_BTN_TOUCH_PAD)) {
  int xL0 = TOP_BTN_PAD;
  int xL1 = xL0 + TOP_BTN_W;
  int xR1 = SW - TOP_BTN_PAD;
  int xR0 = xR1 - TOP_BTN_W;

  if (tx >= xL0 && tx < xL1) { touch.lastBtn = -10; }      // Manger
  else if (tx >= xR0 && tx < xR1) { touch.lastBtn = -11; } // Boire
  else { touch.lastBtn = -1; }
}
#endif

  if (touch.lastBtn != -10 && touch.lastBtn != -11) {
  const int TOUCH_PAD_Y = UI_BOT_H; // zone tactile agrandie (haut+bas)
  if (ty >= (SH - UI_BOT_H - TOUCH_PAD_Y) && ty < (SH + TOUCH_PAD_Y)) {
    uint8_t nbtn = uiButtonCount();

const int GAP = 6;
int bw = uiButtonWidth(nbtn); // <-- doit matcher rebuildUISprites()
int totalW = (int)nbtn * bw + ((int)nbtn - 1) * GAP;
int startX = (SW - totalW) / 2;

    int best = 0;
    int bestDist = 999999;
    for (int i = 0; i < (int)nbtn; i++) {
      int xx = startX + i * (bw + GAP);
      int cx = xx + bw / 2;
      int d = abs(tx - cx);
      if (d < bestDist) { bestDist = d; best = i; }
    }

    int idx = best;

    if (uiSel != (uint8_t)idx) {
      uiSel = (uint8_t)idx;
      uiSpriteDirty = true;
      uiForceBands  = true;
    }

    touch.lastBtn = (int8_t)idx;
  } else {
    touch.lastBtn = -1;
  }
}

if (pressedEdge) {
  touch.pressBtn = touch.lastBtn;
  touch.pressStart = now;
  touch.longPressFired = false;
}

if (touch.stableDown) {
  if (touch.lastBtn != touch.pressBtn) {
    touch.pressBtn = touch.lastBtn;
    touch.pressStart = now;
    touch.longPressFired = false;
  }

  if (!touch.longPressFired && touch.lastBtn >= 0 && uiButtonCount() == uiAliveCount()) {
    UiAction heldAction = uiAliveActionAt((uint8_t)touch.lastBtn);
    if (heldAction == UI_AUDIO && (int32_t)(now - touch.pressStart) >= (int32_t)AUDIO_LONG_PRESS_MS) {
      audioVolumePercent = (audioVolumePercent >= 10) ? 1 : 10;
      char tmp[32];
      snprintf(tmp, sizeof(tmp), "Volume %u%%", audioVolumePercent);
      setMsg(tmp, now, 1500);
      uiSpriteDirty = true;
      uiForceBands  = true;
      if (sdReady) saveNow(now, "audio_vol");
      touch.longPressFired = true;
    }
  }
}


  }

if (releasedEdge) {
  if (touch.longPressFired) {
    touch.lastBtn = -1;
    touch.pressBtn = -1;
    touch.longPressFired = false;
    return;
  }
  if (touch.lastBtn == -10) { // MANGER
    if ((int32_t)(now - cdUntil[(int)UI_MANGER]) >= 0) {
      cdUntil[(int)UI_MANGER] = now + CD_EAT_MS;
      startTask(TASK_EAT, now);
    }
    touch.lastBtn = -1;
    touch.pressBtn = -1;
    uiSpriteDirty = true; uiForceBands = true;
    return;
  }
  if (touch.lastBtn == -11) { // BOIRE
    if ((int32_t)(now - cdUntil[(int)UI_BOIRE]) >= 0) {
      cdUntil[(int)UI_BOIRE] = now + CD_DRINK_MS;
      startTask(TASK_DRINK, now);
    }
    touch.lastBtn = -1;
        touch.pressBtn = -1;
    uiSpriteDirty = true; uiForceBands = true;
    return;
  }

  if (touch.lastBtn >= 0) {
    uiPressAction(now);
    touch.lastBtn = -1;
    touch.pressBtn = -1;
  }
}

}

// ================== MINI-JEUX (stubs prêts à remplacer) ==================
static void mgBegin(TaskKind k, uint32_t now) {
  if (k != TASK_WASH && k != TASK_PLAY) return;

  mg.active = true;
  mg.kind = k;
  mg.startedAt = now;
  mg.success = true;
  mg.score = 0;

  appMode = (k == TASK_WASH) ? MODE_MG_WASH : MODE_MG_PLAY;

  // couper barre activité de la gestion
  activityVisible = false;
  activityText[0] = 0;
  showMsg = false;

  // stop idle anim pour être clean au retour
  enterState(ST_SIT, now);

  for (int i = 0; i < MG_RAIN_MAX; i++) mgRain[i] = RainDrop{};
  for (int i = 0; i < MG_BALLOON_MAX; i++) mgBalloons[i] = Balloon{};

  mgCloudX = (float)(SW / 2);
  mgCloudV = 0.9f;
  mgDinoX = (float)(SW / 2);
  mgDinoV = 2.2f;
  mgDropsHit = 0;

  mgPlayDinoY = 0.0f;
  mgPlayDinoVy = 0.0f;
  mgBalloonsCaught = 0;
  mgBalloonsSpawned = 0;
  mgNextBalloonAt = now + 600;
  mgJumpRequested = false;
  mgNextDropAt = now; // pluie active dès le début du mini-jeu laver
  mgNeedsFullRedraw = true;
  mgPrevCloudX = mgCloudX;
  mgPrevDinoX = mgDinoX;
  mgPrevWashDinoY = SH - UI_BOT_H - triH(pet.stage) - 4;
  mgPrevPlayDinoTop = SH - UI_BOT_H - 6 - triH(pet.stage);
  mgAnimIdx = 0;
  mgAnimNextTick = now + 120;
  noInterrupts();
  mgLastDetent = detentFromEnc(encPos);
  interrupts();

  tft.fillScreen(TFT_BLACK);
}

static bool mgUpdate(uint32_t now) {
  if (mg.startedAt == 0) { mg.success = false; mg.score = 0; return true; }

  if ((int32_t)(now - mgAnimNextTick) >= 0) {
    uint8_t animId = animIdForState(pet.stage, ST_WALK);
    uint8_t cnt = triAnimCount(pet.stage, animId);
    if (cnt == 0) cnt = 1;
    mgAnimIdx = (uint8_t)((mgAnimIdx + 1) % cnt);
    mgAnimNextTick = now + 120;
  }

  if (mg.kind == TASK_WASH) {
    const int CLOUD_W = (int)nuage_W;
    const int CLOUD_H = (int)nuage_H;
    const int DINO_DRAW_W = triW(pet.stage);
    const int DINO_HIT_W = triW(pet.stage);
    const int DINO_HIT_H = triH(pet.stage);
    const int DINO_Y_HIT = SH - UI_BOT_H - DINO_HIT_H - 4 + 25;

    int16_t tx = 0, ty = 0;
    if (readTouchScreen(tx, ty)) {
      (void)ty;
      mgCloudX = (float)tx;
    }

    int32_t encNow;
    noInterrupts();
    encNow = encPos;
    interrupts();
    int32_t det = detentFromEnc(encNow);
    int32_t dd = det - mgLastDetent;
    if (dd != 0) {
      mgCloudX += (float)dd * 6.0f;
      mgLastDetent = det;
    }

    if (btnLeftHeld) mgCloudX -= 4.0f;
    if (btnRightHeld) mgCloudX += 4.0f;

    mgCloudX += mgCloudV;
    float cloudHalf = (float)(CLOUD_W / 2);
    if (mgCloudX < cloudHalf) { mgCloudX = cloudHalf; mgCloudV = fabsf(mgCloudV); }
    if (mgCloudX > (float)(SW - cloudHalf)) { mgCloudX = (float)(SW - cloudHalf); mgCloudV = -fabsf(mgCloudV); }

    mgDinoX += mgDinoV;
    if (mgDinoX < -20) { mgDinoX = -20; mgDinoV = fabsf(mgDinoV); }
    if (mgDinoX > SW - DINO_DRAW_W + 20) { mgDinoX = (float)(SW - DINO_DRAW_W + 20); mgDinoV = -fabsf(mgDinoV); }

// --- spawn pluie basé sur le temps (NE S'ARRETE PAS) ---
if ((int32_t)(now - mgNextDropAt) >= 0) {

  // 1 ou 2 gouttes à chaque tick (pluie plus vivante)
  int toSpawn = 1 + (random(0, 100) < 35 ? 1 : 0);

  while (toSpawn-- > 0) {
    for (int i = 0; i < MG_RAIN_MAX; i++) {
      if (!mgRain[i].active) {
        mgRain[i].active = true;
        mgRain[i].x = mgCloudX + (float)random(-CLOUD_W / 2 + 6, CLOUD_W / 2 - 6);
        mgRain[i].y = (float)CLOUD_H + 4;
        mgRain[i].vy = 2.6f + (float)random(0, 10) * 0.1f;
        break;
      }
    }
  }

  // intervalle pluie : plus petit = plus de pluie
  mgNextDropAt = now + (uint32_t)random(35, 65);
}


    for (int i = 0; i < MG_RAIN_MAX; i++) {
      if (!mgRain[i].active) continue;
      mgRain[i].y += mgRain[i].vy;
      if (mgRain[i].y > (float)(SH - UI_BOT_H)) {
        mgRain[i].active = false;
        continue;
      }
      float rx = mgRain[i].x;
      float ry = mgRain[i].y;
      if (rx >= mgDinoX && rx <= (mgDinoX + DINO_HIT_W) &&
          ry >= (float)DINO_Y_HIT && ry <= (float)(DINO_Y_HIT + DINO_HIT_H)) {
        mgRain[i].active = false;
        mgDropsHit++;
        playRTTTLOnce(RTTTL_RAIN_HIT_SFX, AUDIO_PRIO_LOW);
      }
    }

    mg.score = mgDropsHit;
    if (mgDropsHit >= 50) {
      mg.success = true;
      return true;
    }
    return false;
  }

  if (mg.kind == TASK_PLAY) {
    const int DINO_X = 14;
    const int DINO_W = triW(pet.stage);
    const int DINO_H = triH(pet.stage);
    const int GROUND = SH - UI_BOT_H - 1;
    const int BALLOON_MIN_Y = 70;
    const int BALLOON_MAX_Y = max(BALLOON_MIN_Y, GROUND - DINO_H + 10);
    const float GRAV = 0.65f;
    const float JUMP_V0 = -9.5f;

    int16_t tx = 0, ty = 0;
    if (readTouchScreen(tx, ty)) {
      if (ty < (SH - UI_BOT_H) && mgPlayDinoY == 0.0f) {
        mgPlayDinoVy = JUMP_V0;
      }
    }

    if (mgJumpRequested) {
      if (mgPlayDinoY == 0.0f) {
        mgPlayDinoVy = JUMP_V0;
      }
      mgJumpRequested = false;
    }

    mgPlayDinoVy += GRAV;
    mgPlayDinoY += mgPlayDinoVy;
    if (mgPlayDinoY > 0.0f) {
      mgPlayDinoY = 0.0f;
      mgPlayDinoVy = 0.0f;
    }

    if (mgBalloonsCaught < 10 && (int32_t)(now - mgNextBalloonAt) >= 0) {
      for (int i = 0; i < MG_BALLOON_MAX; i++) {
        if (!mgBalloons[i].active) {
          mgBalloons[i].active = true;
          mgBalloons[i].x = (float)(SW + 10);
          mgBalloons[i].y = (float)random(BALLOON_MIN_Y, BALLOON_MAX_Y);
          mgBalloons[i].vx = -(2.6f + (float)random(0, 20) * 0.1f);
          mgBalloonsSpawned++;
          mgNextBalloonAt = now + (uint32_t)random(900, 1600
        );
          break;
        }
      }
    }

    int dinoTop = (int)(GROUND - DINO_H + mgPlayDinoY + 25);
    for (int i = 0; i < MG_BALLOON_MAX; i++) {
      if (!mgBalloons[i].active) continue;
      mgBalloons[i].x += mgBalloons[i].vx;
      if (mgBalloons[i].x < -20) {
        mgBalloons[i].active = false;
        continue;
      }
      float bx = mgBalloons[i].x;
      float by = mgBalloons[i].y;
      const float r = 8.0f;
      if (bx + r >= (float)DINO_X && bx - r <= (float)(DINO_X + DINO_W) &&
          by + r >= (float)dinoTop && by - r <= (float)(dinoTop + DINO_H)) {
        mgBalloons[i].active = false;
        mgBalloonsCaught++;
        playRTTTLOnce(RTTTL_BALLOON_CATCH_SFX, AUDIO_PRIO_LOW);
      }
    }

    mg.score = mgBalloonsCaught;
    if (mgBalloonsCaught >= 10) {
      mg.success = true;
      return true;
    }
  }

  return false;
}

static void mgDraw(uint32_t now) {
  (void)now;
  tft.startWrite();
  for (int y = 0; y < SH; y += BAND_H) {
    int bh = (y + BAND_H <= SH) ? BAND_H : (SH - y);
    band.fillRect(0, 0, SW, bh, MG_SKY);
    band.setClipRect(0, 0, SW, bh);

    if (appMode == MODE_MG_WASH) {
const int CLOUD_W = (int)nuage_W;
const int CLOUD_H = (int)nuage_H;

const int GROUND = SH - UI_BOT_H - 6;      // <-- sol du mini-jeu
const int DINO_W = triW(pet.stage);
const int DINO_H = triH(pet.stage);
const int DINO_Y = GROUND - DINO_H + 25;        // <-- dino posé sur le sol

const uint16_t* frame = triGetFrame(pet.stage, (uint8_t)animIdForState(pet.stage, ST_WALK), mgAnimIdx);
bool flip = flipForMovingRight(mgDinoV >= 0.0f);

// ---- SOL (à ajouter) ----
if (y + bh > GROUND) {
  int gy0 = max(GROUND, y);
  int gy1 = min(SH, y + bh);
  if (gy1 > gy0) band.fillRect(0, gy0 - y, SW, gy1 - gy0, MG_GROUND);
}
if (GROUND >= y && GROUND < y + bh) {
  band.drawFastHLine(0, GROUND - y, SW, MG_GLINE);
}


      if (y + bh > 0 && y < 22) {
        band.setTextColor(TFT_WHITE, MG_SKY);
        band.setTextSize(1);
        band.setCursor(6, 4 - y);
        band.print("Mini-jeu LAVER");
      }

      if (y + bh > 4 && y < 4 + CLOUD_H) {
        drawImageKeyedOnBand(nuage, CLOUD_W, CLOUD_H, (int)mgCloudX - CLOUD_W / 2, 4 - y, false, 0);
      }

      for (int i = 0; i < MG_RAIN_MAX; i++) {
        if (!mgRain[i].active) continue;
        int ry = (int)mgRain[i].y;
        int y0 = ry - y;
        if (y0 + 6 < 0 || y0 >= bh) continue;
        band.drawFastVLine((int)mgRain[i].x, y0, 6, 0x001F);
      }

      if (y + bh > DINO_Y && y < DINO_Y + DINO_H) {
        drawImageKeyedOnBand(frame, DINO_W, DINO_H, (int)mgDinoX, DINO_Y - y, flip, 0);
      }
    } 
    else {
      const int DINO_X = 14;
      const int DINO_W = triW(pet.stage);
      const int DINO_H = triH(pet.stage);
      const int GROUND = SH - UI_BOT_H - 6;
      int dinoTop = (int)(GROUND - DINO_H + mgPlayDinoY + 25);
      const uint16_t* frame = triGetFrame(pet.stage, (uint8_t)animIdForState(pet.stage, ST_WALK), mgAnimIdx);

      if (y + bh > 0 && y < 22) {
        band.setTextColor(TFT_WHITE, MG_SKY); // fond = ciel (ou mets juste TFT_WHITE)
        band.setTextSize(1);
        band.setCursor(6, 4 - y);
        band.print("Mini-jeu JOUER");
      }

// Remplir le bas en herbe à partir du sol (fixe, pas lié au saut)
if (y + bh > GROUND) {
  int gy0 = max(GROUND, y);
  int gy1 = min(SH, y + bh);
  if (gy1 > gy0) {
    band.fillRect(0, gy0 - y, SW, gy1 - gy0, MG_GROUND);
  }
}

// Ligne du sol par-dessus (optionnel mais joli)
if (GROUND >= y && GROUND < y + bh) {
  band.drawFastHLine(0, GROUND - y, SW, MG_GLINE);
}

      for (int i = 0; i < MG_BALLOON_MAX; i++) {
        if (!mgBalloons[i].active) continue;
        int bx = (int)mgBalloons[i].x - (int)ballon_W / 2;
        int by = (int)mgBalloons[i].y - (int)ballon_H / 2;
        if (y + bh > by && y < by + (int)ballon_H) {
          drawImageKeyedOnBand(ballon, (int)ballon_W, (int)ballon_H, bx, by - y, false, 0);
        }
      }

      if (y + bh > dinoTop && y < dinoTop + DINO_H) {
        drawImageKeyedOnBand(frame, DINO_W, DINO_H, DINO_X, dinoTop - y, flipForMovingRight(true), 0);
      }
    }

// ===== HUD bas mini-jeu : SUR VERT, PAS DE NOIR (clippé) =====
const int HUD_TOP = SH - UI_BOT_H;

if (y + bh > HUD_TOP) {
  // Remplir uniquement la partie HUD qui est dans cette bande (jamais de y négatif)
  int y0 = max(HUD_TOP, y);
  int y1 = min(SH, y + bh);
  int h  = y1 - y0;
  if (h > 0) {
    band.fillRect(0, y0 - y, SW, h, MG_GROUND);
  }

  // Texte (ombre + blanc) SANS fond
  band.setTextSize(1);

  int ty1 = (HUD_TOP + 6) - y;   // coord dans la bande
  int ty2 = (HUD_TOP + 20) - y;

  // ombre
  band.setTextColor(TFT_BLACK);
  band.setCursor(7, ty1 + 1);
  if (appMode == MODE_MG_WASH) {
    band.print("Gouttes: "); band.print(mgDropsHit); band.print(" / 50");
  } else {
    band.print("Ballons: "); band.print(mgBalloonsCaught); band.print(" / 10");
  }
  band.setCursor(7, ty2 + 1);
  band.print("Gagne pour sortir");

  // texte
  band.setTextColor(TFT_WHITE);
  band.setCursor(6, ty1);
  if (appMode == MODE_MG_WASH) {
    band.print("Gouttes: "); band.print(mgDropsHit); band.print(" / 50");
  } else {
    band.print("Ballons: "); band.print(mgBalloonsCaught); band.print(" / 10");
  }
  band.setCursor(6, ty2);
  band.print("Gagne pour sortir");
}


    tft.pushImage(0, y, SW, bh, (uint16_t*)band.getBuffer());


  }

  tft.endWrite();
  mgNeedsFullRedraw = false;
}

// ================== RESET ==================
static void resetStatsToDefault() {
  pet.faim=60; pet.soif=60; pet.hygiene=80; pet.humeur=60;
  pet.energie=100; pet.fatigue=100; pet.amour=60; pet.caca=0;
  pet.sante=80; pet.ageMin=0; pet.vivant=true;
  pet.stage=AGE_JUNIOR;
  pet.evolveProgressMin=0;
  pet.poopAccidentLatched=false;
  strcpy(petName, "???");
}

static void resetToEgg(uint32_t now) {
  (void)now;
  resetStatsToDefault();
  phase = PHASE_EGG;
  task.active = false;
  state = ST_SIT;
  animIdx = 0;
  nextAnimTick = 0;

  poopVisible = false; poopUntil = 0;

  berriesLeftAvailable = true; berriesRespawnAt = 0;
  puddleVisible = true; puddleRespawnAt = 0;

  activityVisible = false;
  activityText[0] = 0;
  showMsg = false;

  // mini-jeu reset
  appMode = MODE_PET;
  mg.active = false;
  mg.kind = TASK_NONE;

  uiSel = 0;
  uiSpriteDirty = true; uiForceBands = true;
  audioNextAlertAt = 0;
}

static void showHomeIntro(uint32_t now) {
  tft.fillScreen(TFT_BLACK);

  int imgW = (int)pageaccueil_W;
  int imgH = (int)pageaccueil_H;
  int x = (SW - imgW) / 2;
  int y = (SH - imgH) / 2;
  tft.pushImage(x, y, imgW, imgH, pageaccueil);

#if ENABLE_AUDIO
  if (audioMode != AUDIO_OFF) {
    playRTTTLOnce(RTTTL_HOME_INTRO, AUDIO_PRIO_MED);
  }
#endif

  uint32_t endAt = now + 3000;
  while ((int32_t)(millis() - endAt) < 0) {
    audioUpdate(millis());
    delay(10);
  }
}

// ================== SETUP/LOOP ==================
void setup() {
  Serial.begin(115200);
  delay(200);

  // init SD (HSPI)
  sdInit();

  audioPwmSetup();
  audioSetTone(0, 0);

tft.init();
tft.setRotation(ROT);
#if DISPLAY_PROFILE == DISPLAY_PROFILE_2432S022
Wire.begin(TOUCH_SDA, TOUCH_SCL);
Wire.setClock(400000);
g_touch.init(TOUCH_SDA, TOUCH_SCL, -1, -1);
tft.setBrightness(255);
#else
pinMode(PIN_BL, OUTPUT);
digitalWrite(PIN_BL, HIGH);
softSPIBeginTouch();
#endif

SW = tft.width();
SH = tft.height();

  if (encoderEnabled()) {
    pinMode(ENC_A, INPUT_PULLUP);
    pinMode(ENC_B, INPUT_PULLUP);
  }
  if (encoderButtonEnabled()) pinMode(ENC_BTN, INPUT_PULLUP);
  if (BTN_LEFT >= 0) pinMode(BTN_LEFT, INPUT_PULLUP);
  if (BTN_RIGHT >= 0) pinMode(BTN_RIGHT, INPUT_PULLUP);
  if (BTN_OK >= 0) pinMode(BTN_OK, INPUT_PULLUP);

#if DISPLAY_PROFILE != DISPLAY_PROFILE_2432S022
#if ENABLE_TOUCH_CALIBRATION
  bool touchReady = sdReady ? touchLoadFromSD() : false;
  if (!touchReady) {
    bool canSkip = encoderButtonEnabled() || (BTN_OK >= 0);
    const int COUNTDOWN_SEC = 9;
    bool wantSkip = false;
    bool lastRaw = false;

    if (canSkip) {
      for (int sec = COUNTDOWN_SEC; sec >= 1 && !wantSkip; --sec) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);

        tft.setTextSize(2);
        tft.setCursor(10, 18);
        tft.print("Sans tactile ?");

        tft.setTextSize(1);
        tft.setCursor(10, 55);
        tft.print("Appuie sur OK/encodeur");
        tft.setCursor(10, 68);
        tft.print("pour desactiver le tactile.");

        tft.setTextSize(2);
        tft.setCursor(10, 102);
        char buf[32];
        snprintf(buf, sizeof(buf), "Calib dans: %d", sec);
        tft.print(buf);

        uint32_t t0 = millis();
        while ((uint32_t)(millis() - t0) < 1000UL) {
          bool raw = readButtonRaw(ENC_BTN) || readButtonRaw(BTN_OK);
          if (raw && !lastRaw) {
            delay(20);
            if (readButtonRaw(ENC_BTN) || readButtonRaw(BTN_OK)) { wantSkip = true; break; }
          }
          lastRaw = raw;
          delay(10);
        }
      }
    }

    if (wantSkip) {
      if (sdReady) {
        touchSaveSkipToSD();
        touchReady = touchLoadFromSD();
      } else {
        touchCal.ok = false;
        touchCal.skipped = true;
        touchReady = true;
      }

      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setTextSize(2);
      tft.setCursor(10, 60);
      tft.print("Tactile desactive");
      delay(1200);
    } else {
      if (!touchRunCalibrationWizard()) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED);
        tft.setTextSize(2);
        tft.setCursor(10, 10);
        tft.print("Touch calib FAIL");
        while (true) delay(1000);
      }
      if (sdReady) {
        touchReady = touchLoadFromSD();
      } else {
        touchReady = touchCal.ok;
      }
    }
  }
  (void)touchReady;
#endif
#endif




  worldW = (float)(2 * SW);
  worldMin = 0.0f;
  worldMax = worldW;

  homeX = (HOME_X_MODE == 0) ? (worldW * 0.5f) : (float)(SW / 2);

  bushLeftX  = 20.0f;
  puddleX    = worldW - (float)FLAQUE_W - 20.0f;

  worldX = homeX;
  camX   = worldX - (float)(SW / 2);
  camX = clampf(camX, 0.0f, worldW - (float)SW);

  if (encoderEnabled()) {
    lastAB = readAB();
    attachInterrupt(digitalPinToInterrupt(ENC_A), isrEnc, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENC_B), isrEnc, CHANGE);
  }

  noInterrupts();
  int32_t p0 = encPos;
  interrupts();
  lastDetent = detentFromEnc(p0);

  randomSeed((uint32_t)(micros() ^ (uint32_t)ESP.getEfuseMac()));

  band.setColorDepth(16);
  if (!band.createSprite(SW, BAND_H)) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.print("NO RAM band -> lower BAND_H");
    while (true) delay(1000);
  }

  uiTop.setColorDepth(16);
  uiBot.setColorDepth(16);
  if (!uiTop.createSprite(SW, UI_TOP_H) || !uiBot.createSprite(SW, UI_BOT_H)) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.print("NO RAM UI sprites");
    while (true) delay(1000);
  }

  DZ_L = (int)(SW * 0.30f);
  DZ_R = (int)(SW * 0.60f);

  uint32_t now = millis();

  bool loaded = false;
  if (sdReady) loaded = loadLatestSave(now);

  if (!loaded) {
    resetToEgg(now);
    if (sdReady) saveNow(now, "boot_new");
  }

  showHomeIntro(now);

  lastPetTick = now;

  rebuildUISprites(now);
  uiSpriteDirty = false;

  int dinoX = (int)roundf(worldX - camX);
  int dinoY = (GROUND_Y - TRI_FOOT_Y);

  tft.startWrite();
  renderFrameOptimized(dinoX, dinoY, nullptr, false, 0);
  uiForceBands = false;
  tft.endWrite();
}

void loop() {
  const uint32_t FRAME_MS = 33;
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < FRAME_MS) return;
  last = now;

  // autosave 1/min
  saveMaybeEachMinute(now);

  audioTickAlerts(now);
  audioUpdate(now);

  updateButtons(now);

  // ================== MINI-JEUX (prioritaires) ==================
  if (appMode != MODE_PET) {
    // debounce bouton encodeur (séparé du UI)
    static bool mgRaw = false;
    static bool mgStable = false;
    static uint32_t mgChangeAt = 0;

bool raw = false;
if (ENC_BTN >= 0) raw = (digitalRead(ENC_BTN) == LOW);

    if (raw != mgRaw) { mgRaw = raw; mgChangeAt = now; }
    if ((now - mgChangeAt) > BTN_DEBOUNCE_MS) mgStable = mgRaw;

    static bool mgLastStable = false;
    bool pressedEdge = (mgStable && !mgLastStable);
    mgLastStable = mgStable;
    if ((pressedEdge || btnOkEdge) && appMode == MODE_MG_PLAY) {
      mgJumpRequested = true;
    }

    bool done = mgUpdate(now);
    mgDraw(now);

    if (done) {
      // appliquer effets seulement ici
      if (mg.kind == TASK_WASH) {
        applyTaskEffects(TASK_WASH, now);
        setMsg(mg.success ? "Lavage OK !" : "Lavage rate", now, 1500);
      } else if (mg.kind == TASK_PLAY) {
        applyTaskEffects(TASK_PLAY, now);
        setMsg(mg.success ? "Jeu OK !" : "Jeu rate", now, 1500);
      }

      mg.active = false;
      mg.kind = TASK_NONE;
      appMode = MODE_PET;

      enterState(ST_SIT, now);

  uiSpriteDirty = true;
  uiForceBands  = true;

  audioNextAlertAt = 0;
}
    return; // on saute la logique gestion pendant mini-jeu
  }

  // message timeout
  if (showMsg && (int32_t)(now - msgUntil) >= 0) {
    showMsg = false;
  }

  // caca timeout
  if (poopVisible && (int32_t)(now - poopUntil) >= 0) {
    poopVisible = false;
    uiForceBands = true;
  }

  // respawn baies
  if (!berriesLeftAvailable && berriesRespawnAt != 0 && (int32_t)(now - berriesRespawnAt) >= 0) {
    berriesLeftAvailable = true;
    berriesRespawnAt = 0;
    setMsg("Baies: OK", now, 1200);
  }

  // respawn flaque
  if (!puddleVisible && puddleRespawnAt != 0 && (int32_t)(now - puddleRespawnAt) >= 0) {
    puddleVisible = true;
    puddleRespawnAt = 0;
    setMsg("Eau: OK", now, 1200);
  }

  // hatch
  if (phase == PHASE_HATCHING) {
    if ((int32_t)(now - hatchNext) >= 0) {
      hatchIdx++;
      hatchNext = now + 220;
      uiForceBands = true;

      if (hatchIdx >= 4) {
        phase = PHASE_ALIVE;
        pet.stage = AGE_JUNIOR;
        pet.evolveProgressMin = 0;

        getRandomDinoName(petName, sizeof(petName));

        char txt[64];
        snprintf(txt, sizeof(txt), "Il s'appelle %s", petName);
        activityVisible = true;
        activityStart = now;
        activityEnd   = now + 4500;
        strncpy(activityText, txt, sizeof(activityText)-1);
        activityText[sizeof(activityText)-1] = 0;

        enterState(ST_SIT, now);

        saveNow(now, "hatch");
      }
      uiSpriteDirty = true;
    }
  }



  // INPUT : tactile + encodeur
  handleTouchUI(now);
  handleEncoderUI(now);
  handleButtonsUI(now);

  // tick stats
    if (phase == PHASE_ALIVE) {
    uint32_t tickMs = (uint32_t)(60000UL / (uint32_t)max(1, SIM_SPEED));
    if ((int32_t)(now - lastPetTick) >= (int32_t)tickMs) {
      while ((int32_t)(now - lastPetTick) >= (int32_t)tickMs) {
        lastPetTick += tickMs;
        updatePetTick(now);
      }
    }
  }

  // tâches
  if (task.active) updateTask(now);

  // idle
  if (!task.active && phase == PHASE_ALIVE) idleUpdate(now);

  audioTickMusic(now);

  // barre activité: si juste message, elle disparaît
  if (!task.active && activityVisible && (int32_t)(now - activityEnd) >= 0) {
    activityStopIfFree(now);
    uiSpriteDirty = true;
  }

  // refresh progress barre activité
  if (activityVisible) {
    if (lastActivityUiRefresh == 0) lastActivityUiRefresh = now;
    if ((now - lastActivityUiRefresh) >= ACTIVITY_UI_REFRESH_MS) {
      lastActivityUiRefresh = now;
      uiSpriteDirty = true;
    }
  } else {
    lastActivityUiRefresh = 0;
  }

  static bool lastCritical = false;
  static bool lastBlinkOn = false;
  bool critical = healthCriticalCount() > 0;
  bool blinkOn = critical && ((now / 500UL) % 2U) == 0U;
  if (critical != lastCritical || blinkOn != lastBlinkOn) {
    uiSpriteDirty = true;
    lastCritical = critical;
    lastBlinkOn = blinkOn;
  }

  // rebuild UI
  if (uiSpriteDirty) {
    uint8_t nbtn = uiButtonCount();
    if (uiSel >= nbtn) uiSel = 0;
    rebuildUISprites(now);
    uiSpriteDirty = false;
  }

  // animation / rendu
  if (phase == PHASE_ALIVE) {
    uint8_t animId = animIdForState(pet.stage, state);

    bool forceFace = false;
    bool faceRight = false;
    if (task.active && task.kind == TASK_HUG && task.ph == PH_DO) {
      if (pet.stage == AGE_JUNIOR) animId = (uint8_t)triJ::ANIM_JUNIOR_AMOUR;
      else if (pet.stage == AGE_ADULTE) animId = (uint8_t)triA::ANIM_ADULTE_AMOUR;
      else animId = (uint8_t)triS::ANIM_SENIOR_AMOUR;
    }

    if (task.active && task.ph == PH_DO) {
      if (task.kind == TASK_EAT) { forceFace = true; faceRight = false; }
      if (task.kind == TASK_DRINK) { forceFace = true; faceRight = true; }
    }

    if ((int32_t)(now - nextAnimTick) >= 0) {
      uint8_t cnt = triAnimCount(pet.stage, animId);
      if (cnt == 0) cnt = 1;
      animIdx = (animIdx + 1) % cnt;
      nextAnimTick = now + frameMsForState(state);
    }

    int dinoX = (int)roundf(worldX - camX);
    if (dinoX > DZ_R) camX = worldX - (float)DZ_R;
    else if (dinoX < DZ_L) camX = worldX - (float)DZ_L;
    camX = clampf(camX, 0.0f, worldW - (float)SW);
    dinoX = (int)roundf(worldX - camX);

    int dinoY = (GROUND_Y - TRI_FOOT_Y);

    const uint16_t* frame = triGetFrame(pet.stage, animId, animIdx);

    bool flipX = flipForMovingRight(movingRight);
    if (forceFace) flipX = faceRight ? true : false;

    uint8_t shade = 0;
    if (pet.hygiene <= 0.0f) shade = 2;
    else if (pet.hygiene < 20.0f) shade = 1;

    tft.startWrite();
    renderFrameOptimized(dinoX, dinoY, frame, flipX, shade);
    uiForceBands = false;
    tft.endWrite();
  } else {
    int dinoX = (int)roundf(worldX - camX);
    int dinoY = (GROUND_Y - TRI_FOOT_Y);
    tft.startWrite();
    renderFrameOptimized(dinoX, dinoY, nullptr, false, 0);
    uiForceBands = false;
    tft.endWrite();
  }
}
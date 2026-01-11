// tamadinov4.ino - ESP32 WROOM + CYD 2432S028 (ILI9341) + LovyanGFX
#include <stdint.h>

// ================== ENUMS (Arduino prototype fix) ==================
enum AgeStage : uint8_t { AGE_JUNIOR, AGE_ADULTE, AGE_SENIOR };

enum UiAction : uint8_t {
  UI_REPOS, UI_MANGER, UI_BOIRE, UI_LAVER, UI_JOUER, UI_CACA, UI_CALIN,
  UI_COUNT
};

enum TriState : uint8_t { ST_WALK, ST_JUMP, ST_SIT, ST_BLINK, ST_EAT, ST_SLEEP, ST_DEAD };

enum TaskKind  : uint8_t { TASK_NONE, TASK_SLEEP, TASK_EAT, TASK_DRINK, TASK_WASH, TASK_PLAY, TASK_POOP, TASK_HUG };
enum TaskPhase : uint8_t { PH_GO, PH_DO, PH_RETURN };

enum GamePhase : uint8_t { PHASE_EGG, PHASE_HATCHING, PHASE_ALIVE, PHASE_RESTREADY, PHASE_TOMB };

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

// ================== INCLUDES ==================
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <math.h>
#include <string.h>

#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>

#include "DinoNames.h"

// ================== ASSETS (namespaces rename) ==================
#define triceratops triJ
#include "triceratops_junior_90x90.h"
#undef triceratops

#define triceratops triA
#include "triceratops_adulte_90x90.h"
#undef triceratops

#define triceratops triS
#include "triceratops_senior_90x90.h"
#undef triceratops

#define dino egg
#include "oeuf_50x50.h"
#undef dino

#define dino poop
#include "caca_20x20.h"
#undef dino

#include "tombe_32x54.h"

// Décor
#include "montagne_117x56.h"
#include "sapin_40x80_40x78.h"
#include "arbre_40x80_39x73.h"
#include "buissonbaie_60x60_51x52.h"
#include "buissonsansbaie_60x60_51x52.h"
#include "flaque_eau_60x25_53x20.h"
#include "nuage_65x43.h"
#include "ballon_31x31.h"

#ifndef TFT_BLACK
  #define TFT_BLACK 0x0000
  #define TFT_WHITE 0xFFFF
  #define TFT_RED   0xF800
  #define TFT_GREEN 0x07E0
  #define TFT_BLUE  0x001F
  #define TFT_YELLOW 0xFFE0
#endif

// ================== CONFIG ==================
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
static const int EVOLVE_THR = 80;
static const int EVOLVE_JUNIOR_TO_ADULT_MIN = 80;
static const int EVOLVE_ADULT_TO_SENIOR_MIN = 80;
static const int EVOLVE_SENIOR_TO_REST_MIN  = 40;

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
static const uint32_t CD_EAT_MS   = 8UL  * 60UL * 1000UL;
static const uint32_t CD_DRINK_MS = 3UL  * 60UL * 1000UL;
static const uint32_t CD_WASH_MS  = 5UL  * 60UL * 1000UL;
static const uint32_t CD_PLAY_MS  = 6UL  * 60UL * 1000UL;
static const uint32_t CD_POOP_MS  = 4UL  * 60UL * 1000UL;
static const uint32_t CD_HUG_MS   = 3UL  * 60UL * 1000UL;

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

static const float SLEEP_GAIN_PER_SEC = 0.5f;
static const int   POOP_ACCIDENT_AT = 100;

// ================== TRANSPARENCE KEY ==================
static inline uint16_t swap16(uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); }
static const uint16_t KEY      = 0xF81F;
static const uint16_t KEY_SWAP = 0x1FF8;

// ================== ALIAS décor ==================
#define MONT_W    montagne_pixelart_120x100_W
#define MONT_H    montagne_pixelart_120x100_H
#define MONT_IMG  montagne_pixelart_120x100

#define SAPIN_W   sapin_40x80_W
#define SAPIN_H   sapin_40x80_H
#define SAPIN_IMG sapin_40x80

#define ARBRE_W   arbre_40x80_W
#define ARBRE_H   arbre_40x80_H
#define ARBRE_IMG arbre_40x80

#define BBAIE_W   buissonbaie_60x60_W
#define BBAIE_H   buissonbaie_60x60_H
#define BBAIE_IMG buissonbaie_60x60

#define BSANS_W   buissonsansbaie_60x60_W
#define BSANS_H   buissonsansbaie_60x60_H
#define BSANS_IMG buissonsansbaie_60x60

#define FLAQUE_W   flaque_eau_60x25_W
#define FLAQUE_H   flaque_eau_60x25_H
#define FLAQUE_IMG flaque_eau_60x25

// ================== ÉCRAN (ESP32-2432S028R ILI9341) ==================
// Ecran (ILI9341) sur SPI (pins 14/13/12/2/15) + Touch XPT2046 sur PINS SEPAREES
// Touch (XPT2046) : CLK25 MOSI32 MISO39 CS33 IRQ36 (comme ton code de test)
static const int TOUCH_IRQ  = 36; // input-only
static const int TOUCH_MOSI = 32;
static const int TOUCH_MISO = 39;
static const int TOUCH_CLK  = 25;
static const int TOUCH_CS   = 33;

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
      cfg.panel_width  = 240;
      cfg.panel_height = 320;
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

// ================== ENCODEUR ==================
static const int ENC_A   = 22;
static const int ENC_B   = 27;
static const int ENC_BTN = 35;

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
static inline bool readBtnPressedRaw() { return digitalRead(ENC_BTN) == LOW; }

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
static const int UI_TOP_H = 92;   // + grand pour remonter bars + barre activité
static const int UI_BOT_H = 42;
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
  return 1.0f + (1.0f - (f / 100.0f)) * 5.0f;
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
  return (uint8_t)UI_COUNT;
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
static bool sdReady = false;

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
  pet.sante   = (float)iClamp(ps["sante"]   | 80, 0, 100);

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

  JsonObject ps = doc.createNestedObject("pet");
  ps["faim"]    = fToI100(pet.faim);
  ps["soif"]    = fToI100(pet.soif);
  ps["hygiene"] = fToI100(pet.hygiene);
  ps["humeur"]  = fToI100(pet.humeur);
  ps["energie"] = fToI100(pet.energie);
  ps["fatigue"] = fToI100(pet.fatigue);
  ps["amour"]   = fToI100(pet.amour);
  ps["caca"]    = fToI100(pet.caca);
  ps["sante"]   = fToI100(pet.sante);

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
  if (phase == PHASE_EGG || phase == PHASE_HATCHING) {
    snprintf(line, sizeof(line), "Oeuf");
    showStatusLine = true;
  } else if (phase == PHASE_RESTREADY) {
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

    uiTop.setTextColor(TFT_WHITE, KEY);
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
    uiTextShadow(uiTop, 6, 2, line, TFT_WHITE, KEY);
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

  if (activityVisible) {
    float p = activityProgress(now);
    drawActivityBar(uiTop, SW/2, 74, 140, 18, p, activityText);
  }

  uiBot.fillSprite(KEY);
  uiBot.setTextDatum(middle_center);

  const uint8_t nbtn = uiButtonCount();
  int padb = 4;
  int bw = (SW - padb*(nbtn+1)) / nbtn;
  int bh = UI_BOT_H - 10;
  int yy = 5;
  int r = 8;

  bool hardBusy = (phase == PHASE_HATCHING || phase == PHASE_TOMB);

  for (int i = 0; i < (int)nbtn; i++) {
    int xx = padb + i*(bw + padb);
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
      UiAction a = (UiAction)i;
      baseCol = btnColorForAction(a);
      lab = btnLabel(a);

      bool onCooldown = ((int32_t)(now - cdUntil[(int)a]) < 0);
      bool busy = task.active || (state == ST_SLEEP) || (appMode != MODE_PET);

      if (busy || onCooldown) disabledLook = true;

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

    uiBot.setTextColor(fg, fill);
    uiBot.setTextSize(2);
    if (uiBot.textWidth(lab) > (bw - 6)) uiBot.setTextSize(1);
    uiBot.drawString(lab, xx + bw/2, yy + bh/2);
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

    if (k == TASK_POOP) activityStartTask(now, "Fait caca...", task.plannedTotal);
    else if (k == TASK_HUG)  activityStartTask(now, "Fait un calin...", task.plannedTotal);

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
  if (pet.faim    < 15) ds -= 2;
  if (pet.soif    < 15) ds -= 2;
  if (pet.hygiene < 15) ds -= 1;
  if (pet.humeur  < 10) ds -= 1;
  if (pet.energie < 10) ds -= 1;
  if (pet.fatigue < 10) ds -= 1;
  if (pet.amour   < 10) ds -= 0.5f;
  if (pet.caca >= 95) ds -= 1;

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

// ================== TACTILE (XPT2046 SOFT-SPI + CALIB AFFINE) ==================
static TouchAffine touchCal = { 0,0,0, 0,0,0, false, false };
static const char* TOUCH_CAL_FILE = "/touch_affine.json";

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

static uint16_t medianSmall(uint16_t* a, int n) {
  for (int i=1;i<n;i++){
    uint16_t k=a[i]; int j=i-1;
    while(j>=0 && a[j]>k){ a[j+1]=a[j]; j--; }
    a[j+1]=k;
  }
  return a[n/2];
}

static TouchSample readTouchFiltered() {
  const int N=9;
  uint16_t xs[N], ys[N], zs[N];
  int k=0;
  for(int i=0;i<N;i++){
    TouchSample s=readTouchOnce();
    if(!s.valid){ delay(2); continue; }
    xs[k]=s.x; ys[k]=s.y; zs[k]=s.z; k++;
    delay(2);
  }
  TouchSample out{};
  if(k<5){ out.valid=false; return out; }
  out.x = medianSmall(xs,k);
  out.y = medianSmall(ys,k);
  uint16_t zmax=0; for(int i=0;i<k;i++) if(zs[i]>zmax) zmax=zs[i];
  out.z=zmax;
  out.valid=true;
  return out;
}

static bool waitPressStable(TouchSample &out, uint32_t timeoutMs=25000) {
  uint32_t t0=millis();
  while(millis()-t0<timeoutMs){
    TouchSample a=readTouchFiltered();
    if(!a.valid){ delay(10); continue; }
    delay(40);
    TouchSample b=readTouchFiltered();
    delay(40);
    TouchSample c=readTouchFiltered();
    if(!b.valid || !c.valid){ delay(10); continue; }
    int dx1=abs((int)a.x-(int)b.x), dy1=abs((int)a.y-(int)b.y);
    int dx2=abs((int)b.x-(int)c.x), dy2=abs((int)b.y-(int)c.y);
    if(dx1<45 && dy1<45 && dx2<45 && dy2<45){ out=c; return true; }
    delay(10);
  }
  return false;
}

static void waitRelease(uint32_t timeoutMs=8000){
  uint32_t t0=millis();
  while(millis()-t0<timeoutMs){
    if(!readTouchOnce().valid) return;
    delay(15);
  }
}

// ---- Solve 3x3 (Gaussian) ----
static bool solve3x3(float A[3][3], float B[3], float X[3]) {
  float M[3][4];
  for(int r=0;r<3;r++){
    for(int c=0;c<3;c++) M[r][c]=A[r][c];
    M[r][3]=B[r];
  }

  for(int i=0;i<3;i++){
    int piv=i;
    float best=fabsf(M[i][i]);
    for(int r=i+1;r<3;r++){
      float v=fabsf(M[r][i]);
      if(v>best){ best=v; piv=r; }
    }
    if(best<1e-6f) return false;
    if(piv!=i){
      for(int c=i;c<4;c++){ float tmp=M[i][c]; M[i][c]=M[piv][c]; M[piv][c]=tmp; }
    }

    float div=M[i][i];
    for(int c=i;c<4;c++) M[i][c]/=div;

    for(int r=0;r<3;r++){
      if(r==i) continue;
      float f=M[r][i];
      for(int c=i;c<4;c++) M[r][c]-=f*M[i][c];
    }
  }
  X[0]=M[0][3]; X[1]=M[1][3]; X[2]=M[2][3];
  return true;
}

static bool fitAffineLSQ(const TouchSample* raw, const int* S, int n, float &a, float &b, float &c) {
  float XtX[3][3] = {{0,0,0},{0,0,0},{0,0,0}};
  float XtY[3] = {0,0,0};

  for(int i=0;i<n;i++){
    float x = (float)raw[i].x;
    float y = (float)raw[i].y;
    float yy = (float)S[i];
    float v[3] = {x,y,1.0f};
    for(int r=0;r<3;r++){
      for(int cc=0;cc<3;cc++){
        XtX[r][cc] += v[r]*v[cc];
      }
      XtY[r] += v[r]*yy;
    }
  }

  float p[3];
  if(!solve3x3(XtX, XtY, p)) return false;
  a=p[0]; b=p[1]; c=p[2];
  return true;
}

static bool rawToScreenAffine(const TouchSample& r, int &sx, int &sy){
  if(!touchCal.ok || !r.valid) return false;
  float fx = touchCal.a*(float)r.x + touchCal.b*(float)r.y + touchCal.c;
  float fy = touchCal.d*(float)r.x + touchCal.e*(float)r.y + touchCal.f;
  int W=tft.width(), H=tft.height();
  int ix=(int)lroundf(fx), iy=(int)lroundf(fy);
  if(ix<0) ix=0; if(ix>W-1) ix=W-1;
  if(iy<0) iy=0; if(iy>H-1) iy=H-1;
  sx=ix; sy=iy;
  return true;
}

static void touchBanner(const char* s){
  tft.fillRect(0,0,tft.width(),22,0x0000);
  tft.setTextSize(1);
  tft.setTextColor(0xFFFF,0x0000);
  tft.setCursor(6,6);
  tft.print(s);
}
static void touchCross(int x,int y,uint16_t col){
  tft.drawLine(x-14,y,x+14,y,col);
  tft.drawLine(x,y-14,x,y+14,col);
  tft.drawRect(x-18,y-18,36,36,col);
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
  doc["skip"]= 0;
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
  doc["skip"]= 1;

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
  int W=tft.width(), H=tft.height();
  const int M=28;

  const int TLx=M,     TLy=M;
  const int TRx=W-M,   TRy=M;
  const int BRx=W-M,   BRy=H-M;
  const int BLx=M,     BLy=H-M;

  TouchSample Praw[4]{};
  int Sx[4] = {TLx,TRx,BRx,BLx};
  int Sy[4] = {TLy,TRy,BRy,BLy};

  tft.fillScreen(0x0000);
  touchBanner("Calibration tactile (1/4) : HAUT GAUCHE");
  touchCross(TLx,TLy,0xFFFF);
  if(!waitPressStable(Praw[0])) return false; waitRelease();

  tft.fillScreen(0x0000);
  touchBanner("Calibration tactile (2/4) : HAUT DROIT");
  touchCross(TRx,TRy,0xFFFF);
  if(!waitPressStable(Praw[1])) return false; waitRelease();

  tft.fillScreen(0x0000);
  touchBanner("Calibration tactile (3/4) : BAS DROIT");
  touchCross(BRx,BRy,0xFFFF);
  if(!waitPressStable(Praw[2])) return false; waitRelease();

  tft.fillScreen(0x0000);
  touchBanner("Calibration tactile (4/4) : BAS GAUCHE");
  touchCross(BLx,BLy,0xFFFF);
  if(!waitPressStable(Praw[3])) return false; waitRelease();

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
  TouchSample r = readTouchFiltered();
  if(!r.valid) return false;
  int x,y;
  if(!rawToScreenAffine(r,x,y)) return false;
  sx = (int16_t)x;
  sy = (int16_t)y;
  return true;
}

// ================== TACTILE -> UI (AJOUT) ==================
struct TouchDeb {
  bool rawDown = false;
  bool stableDown = false;
  uint32_t lastChange = 0;
  int16_t x = 0, y = 0;
  int8_t  lastBtn = -1;
};
static TouchDeb touch;
static const uint32_t TOUCH_DEBOUNCE_MS = 25;

static inline bool readTouchRaw(int16_t &x, int16_t &y) {
  return readTouchScreen(x, y);
}

// ================== Input UI ==================
static void handleEncoderUI(uint32_t now) {
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
    setMsg("Reveille !", now, 1500);
    uiSpriteDirty = true; uiForceBands = true;
    return;
  }

  if (task.active) { setMsg("Occupe !", now, 1200); uiSpriteDirty = true; uiForceBands = true; return; }

  uint8_t nbtn = uiButtonCount();
  if (nbtn != UI_COUNT) return;

  UiAction a = (UiAction)uiSel;

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

    if (ty >= (SH - UI_BOT_H)) {
      uint8_t nbtn = uiButtonCount();
      int padb = 4;
      int bw = (SW - padb*(nbtn+1)) / nbtn;

      int idx = (tx - padb) / (bw + padb);
      if (idx < 0) idx = 0;
      if (idx >= (int)nbtn) idx = (int)nbtn - 1;

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

  if (releasedEdge && touch.lastBtn >= 0) {
    uiPressAction(now);
    touch.lastBtn = -1;
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
}

// ================== SETUP/LOOP ==================
void setup() {
  Serial.begin(115200);
  delay(200);

  // init SD (HSPI)
  sdInit();

  pinMode(PIN_BL, OUTPUT);
  digitalWrite(PIN_BL, HIGH);

  tft.init();
  tft.setRotation(1);
  SW = tft.width();
  SH = tft.height();

  // ---- Touch (soft-SPI) ----
  softSPIBeginTouch();

  pinMode(ENC_BTN, INPUT);

  bool touchReady = touchLoadFromSD();
  if (!touchReady) {
    const int COUNTDOWN_SEC = 9;

    bool wantSkip = false;
    bool lastRaw  = false;

    for (int sec = COUNTDOWN_SEC; sec >= 1 && !wantSkip; --sec) {
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);

      tft.setTextSize(2);
      tft.setCursor(10, 18);
      tft.print("Sans tactile ?");

      tft.setTextSize(1);
      tft.setCursor(10, 55);
      tft.print("Appuie 1x sur l'encodeur");
      tft.setCursor(10, 68);
      tft.print("pour desactiver le tactile.");

      tft.setTextSize(2);
      tft.setCursor(10, 102);
      char buf[32];
      snprintf(buf, sizeof(buf), "Calib dans: %d", sec);
      tft.print(buf);

      uint32_t t0 = millis();
      while ((uint32_t)(millis() - t0) < 1000UL) {
        bool raw = (digitalRead(ENC_BTN) == LOW);
        if (raw && !lastRaw) {
          delay(20);
          if (digitalRead(ENC_BTN) == LOW) { wantSkip = true; break; }
        }
        lastRaw = raw;
        delay(10);
      }
    }

    if (wantSkip) {
      touchSaveSkipToSD();
      touchReady = touchLoadFromSD();

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
      touchReady = touchLoadFromSD();
    }
  }

  worldW = (float)(2 * SW);
  worldMin = 0.0f;
  worldMax = worldW;

  homeX = (HOME_X_MODE == 0) ? (worldW * 0.5f) : (float)(SW / 2);

  bushLeftX  = 20.0f;
  puddleX    = worldW - (float)FLAQUE_W - 20.0f;

  worldX = homeX;
  camX   = worldX - (float)(SW / 2);
  camX = clampf(camX, 0.0f, worldW - (float)SW);

  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(ENC_BTN, INPUT);

  lastAB = readAB();
  attachInterrupt(digitalPinToInterrupt(ENC_A), isrEnc, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B), isrEnc, CHANGE);

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

  // ================== MINI-JEUX (prioritaires) ==================
  if (appMode != MODE_PET) {
    // debounce bouton encodeur (séparé du UI)
    static bool mgRaw = false;
    static bool mgStable = false;
    static uint32_t mgChangeAt = 0;

    bool raw = (digitalRead(ENC_BTN) == LOW);
    if (raw != mgRaw) { mgRaw = raw; mgChangeAt = now; }
    if ((now - mgChangeAt) > BTN_DEBOUNCE_MS) mgStable = mgRaw;

    static bool mgLastStable = false;
    bool pressedEdge = (mgStable && !mgLastStable);
    mgLastStable = mgStable;
    if (pressedEdge && appMode == MODE_MG_PLAY) {
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
    if (task.active && task.kind == TASK_HUG) {
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
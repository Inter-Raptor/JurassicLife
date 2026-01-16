# 🦖 JurassicLife (Arduino / ESP32)

This code is made to run on:
- **2432S022**
- **2432S028**
- **Classic ESP32 + ILI9341 320×240 screen** (profile `DISPLAY_PROFILE_ILI9341_320x240`)

The idea is simple: you change 2–3 lines at the top of the code, then you **upload**.

---

## 📌 Table of contents
1. Supported boards  
2. Change the board type (line 11)  
3. Enable / disable audio (line 14)  
4. Upload  
5. Optional: clickable rotary encoder or 3 buttons  
6. Change the pins (the `#if DISPLAY_PROFILE...` block)  
7. Wiring diagrams (encoder / buttons)  
8. UI differences between 2432S022 and 2432S028  
9. Save: SD card required  

---

## 1) ✅ Supported boards

- **2432S022**: supported (warning: no physical controls planned)
- **2432S028**: supported (physical controls possible)
- **ESP32 + ILI9341 320×240**: supported via `DISPLAY_PROFILE_ILI9341_320x240`

---

## 2) 🔁 Change the board type in the code (line 11)

To choose your board, edit line **11**:

```cpp
#define DISPLAY_PROFILE DISPLAY_PROFILE_2432S022
```

You replace **only the right side** with:

### ➜ For 2432S028
```cpp
#define DISPLAY_PROFILE DISPLAY_PROFILE_2432S028
```

### ➜ For ESP32 + ILI9341 320×240 screen
```cpp
#define DISPLAY_PROFILE DISPLAY_PROFILE_ILI9341_320x240
```

> ✅ Do not change `#define DISPLAY_PROFILE`: you only change the value after it.

---

## 3) 🔊 Enable / disable audio (line 14)

You can enable or disable audio by editing line **14**:

```cpp
#define ENABLE_AUDIO 1
```

- `#define ENABLE_AUDIO 0` → audio disabled  
- `#define ENABLE_AUDIO 1` → audio enabled  

✅ If you enable audio, a **new button** will appear in the UI that lets you:
- **switch** between the different audio modes
- and also **increase / decrease the volume**

---

## 4) ⬆️ Upload (the easy part)

Once you have chosen:
- your board (`DISPLAY_PROFILE`)
- and whether you want audio (`ENABLE_AUDIO`)

➡️ You just need to **upload** from the Arduino IDE.

---

## 5) 🎮 Optional: add a clickable encoder or 3 buttons

Optionally, you can add:
- a **clickable rotary encoder**
- or **3 navigation buttons** (Left / OK / Right)

⚠️ **Warning: this is not possible on 2432S022** (not enough accessible pins, so it’s not planned “properly”).  
➡️ On **2432S022**, the recommended use = **touch only**.

On **2432S028** and on the **ESP32 + ILI9341** profile, it’s possible.

---

## 6) 🧷 Change the used pins (block in the code)

Pins are changed here (in your code):

```cpp
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
  static const int ENC_BTN = 35;   // BTN=35 (or -1 to disable)

  static const int BTN_LEFT  = -1;
  static const int BTN_RIGHT = -1;
  static const int BTN_OK    = -1;
#else // DISPLAY_PROFILE_ILI9341_320x240 (classic ESP32 with a screen)
  static const int ENC_A   = 22;   // default encoder pins
  static const int ENC_B   = 27;
  static const int ENC_BTN = 35;

  // 3-button option: set ENC_* to -1 and define BTN_* (default pins to adapt)
  static const int BTN_LEFT  = -1;
  static const int BTN_RIGHT = -1;
  static const int BTN_OK    = -1;
#endif
```

✅ How it works:
- Setting a pin to `-1` = **disabled**
- You choose **either** encoder **or** 3 buttons:
  - **Encoder**: set `ENC_A / ENC_B / ENC_BTN` and keep `BTN_* = -1`
  - **3 buttons**: set `ENC_* = -1` then define `BTN_LEFT / BTN_RIGHT / BTN_OK`

⚠️ On 2432S028, if you use `ENC_BTN = 35` (or another similar pin), you may need to add a **pull-up resistor** (see your diagram).

---

## 7) 🧷 Wiring diagrams (encoder / buttons)

I made a wiring diagram for:
- **encoder** mode
- **3 buttons** mode

➡️ They are in the `screenshot/` folder of the repo (wiring images).

---

## 8) ⚠️ UI differences: 2432S022 vs 2432S028

Warning: if you are on **2432S022**, you won’t have exactly the same UI as on **2432S028**.

- **2432S028** has a bigger screen → allows a different, more comfortable layout
- **2432S022** is smaller → the UI is adapted to that size

So it’s normal if the display isn’t identical.

---

## 9) 💾 Save: SD card required

In all cases, if you want a **save after power-off** for your dinosaur:
➡️ you need a **microSD card**.

Without an SD card, you will lose the save after power-off/restart.

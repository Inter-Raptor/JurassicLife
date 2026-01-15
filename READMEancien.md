# JurassicLife 🦖 — Dinosaur Virtual Pet for ESP32 (CYD 2.8")

JurassicLife is a **Tamagotchi-style dinosaur pet game** built for the **ESP32-2432S028 “CYD” 2.8" touchscreen board** (240×320 ILI9341 + resistive touch).  
Raise a dino from **Egg → Juvenile → Adult → Senior**, take care of its needs, and keep it healthy.

> **Important:** This repository currently focuses on gameplay, visuals, and inputs.  
## Flasher le firmware (.bin)

Un firmware précompilé est disponible dans le dossier `firmware/`.

### Option 1 — Le plus simple (1 seul fichier)
Le fichier `*_merged.bin` contient **bootloader + partitions + application**.  
➡️ Il se flashe à l’offset **0x0**.

> ⚠️ Attention : flasher le `merged.bin` **écrase la flash** (perte de données/paramètres éventuels).

#### Avec esptool (Windows / Linux / macOS)
1. Installe `esptool` (ou utilise `esptool.exe` si tu l’as déjà).
2. Mets ta carte en USB et repère le port série (ex: `COM11`).

Commande :
```bash
esptool --chip esp32 --port COM11 --baud 921600 write-flash 0x0 firmware/JurassicLife_vXX_ESP32_4MB_merged.bin


---

## Demo & Screenshots
- **YouTube demo:** *(add your link here)*
- **Screenshots:** see [`screenshots/`](./screenshots)

---

## Features
- **Life stages:** Egg → Juvenile → Adult → Senior  
- **Core actions:** Rest, Eat, Drink, Wash, Play, Hug, Poop *(depending on the build/version)*  
- **Stats system:** hunger, thirst, hygiene, mood, energy/fatigue, love, poop-need, health  
- **Animations & pixel art:** sprite sheets/frames + scripts to generate `.h` headers for Arduino
- **Multiple input methods:**
  - **Touch only** (recommended / default on CYD)
  - **3 buttons** (Left / Right / Validate)
  - **Rotary encoder** (Rotate + Click)

---

## Hardware
### Primary supported board
- **ESP32-2432S028 (CYD 2.8")**
  - **Display:** ILI9341 240×320
  - **Touch:** resistive (commonly XPT2046 on many CYD boards)

### Optional inputs
- **3 buttons:** Left / Right / Validate  
- **Rotary encoder:** A/B + Click (press)

### Optional add-ons
- **microSD card** (only if your build enables saves/assets in code)
- **Battery/charger** (if you build a portable version)

---

## Controls
JurassicLife is fully playable with **touch only**, but can also be used as a “mini console” with buttons or an encoder.

### Touch mode
- Use on-screen buttons/menus.
- Best choice if you don’t want extra wiring.

### 3-button mode
- **Left/Right:** navigate actions or UI options  
- **Validate:** select / confirm

### Rotary encoder mode
- **Rotate:** navigate (same role as Left/Right)  
- **Click:** validate

> How to select the input mode?  
> Look for input configuration/defines near the top of the Arduino sketch or in a config header.  
> If there is no clear switch yet, you can add one (see **Customization** below).

---

## Repository Layout (current)
This repo currently contains folders like:
- `arduino/` — Arduino source code (project folder for Arduino IDE)
- `Sprites/` — sprites/animations and tooling
- `3DSTL/` — STL files for 3D printing (enclosures/parts)
- `screenshots/` — screenshots used for docs/demo
- `archive/` — old experiments, scripts, and legacy code (not maintained)

> Tip: later you may rename `Sprites/` → `sprites/` and `3DSTL/` → `3d/` to match common conventions.  
> It’s optional, but it makes the repo look more “standard”.

---

## Installation (from source) — Arduino IDE
### Requirements
- **Arduino IDE 2.x**
- **ESP32 Arduino core** installed via Boards Manager

### Steps
1. Clone or download this repository:
   - GitHub: Code → **Download ZIP**  
   - or use `git clone`
2. Open the Arduino project:
   - go into `arduino/` and open the main `.ino` file with Arduino IDE  
   *(the exact subfolder may include “VF” or a version name depending on your current structure).*
3. Arduino IDE → **Tools**:
   - **Board:** choose an ESP32 board profile compatible with your CYD (often *ESP32 Dev Module* works)
   - **Port:** select the COM port of your CYD
4. Install missing libraries if Arduino prompts you.
5. Click **Upload**.

### Common compilation/library notes
Your sketch may rely on:
- a display driver/wrapper (many CYD projects use **LovyanGFX**)
- touch controller support
- SD / SPI support (if saves are enabled)
- JSON (if saves are JSON)

If compilation fails, read the error message: Arduino usually tells you which library is missing.

---

## Firmware (prebuilt `.bin`) — Recommended for players
For easy installation (no IDE), publish **GitHub Releases** with a ready-to-flash package.

### What a release should include
Depending on your ESP32 build, flashing may require **one** file or **three** files:
- `bootloader.bin`
- `partitions.bin`
- `firmware.bin` (your app)

Plus convenience scripts:
- `flash_windows.bat`
- `flash_linux_mac.sh`
- `README_FLASH.txt`

### Typical ESP32 flash offsets
These are very common defaults (verify for your build):
- `0x1000`  → bootloader  
- `0x8000`  → partitions  
- `0x10000` → firmware/app  

Example (Windows PowerShell / CMD) with `esptool.py`:
```bat
esptool.py --chip esp32 --port COM3 --baud 921600 ^
  write_flash -z 0x1000 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin
```

> When you are ready, publish releases on GitHub:  
> **Releases → Draft a new release → Upload the flash package**.

---

## Gameplay Guide
### Goal
Keep your dinosaur healthy by managing its needs over time.  
If needs stay low for too long, **health decreases**.

### Life stages
- **Egg:** initial stage, “pre-birth”
- **Juvenile:** newborn/child stage
- **Adult:** stable stage
- **Senior:** final life stage

### Growing up (evolution)
Evolution is **time-based** and typically requires your dino to be in “good condition”:
- keep multiple stats above a threshold (e.g., hunger/thirst/mood/love)
- maintain health
- spend enough time in the current stage

> Exact thresholds and times are **configurable in the code**.  
> If you want the README to list the exact numbers, document the constants you use (see **Customization**).

### When health reaches 0
If **health hits 0**, the game transitions to a “rest in peace” / tomb state.

---

## Actions (typical set)
Your build may include some or all of the following:

- **Rest / Sleep**
  - increases energy and/or reduces fatigue
- **Eat**
  - increases hunger stat (and may increase poop need)
- **Drink**
  - increases thirst stat
- **Wash**
  - increases hygiene stat
- **Play**
  - increases mood stat
- **Hug**
  - increases love stat
- **Poop**
  - resets poop need (when it gets high)

> If you add mini-games for some actions (wash/play), document them here as well.

---

## Save System (optional)
Some builds save the pet state (stats, stage, timers…) to **microSD** as a small JSON file.

If your build uses saves, consider documenting:
- save file path/name
- what is stored (stats, stage, timers)
- autosave interval (e.g., every minute) and save triggers (actions, stage change, etc.)

---

## Sprites & Asset Pipeline
Sprites/animations live in `Sprites/`.

### Typical pipeline
- Pixel art sources (PNG/Aseprite)  
- Conversion scripts generate `.h` headers (PROGMEM arrays)  
- Arduino code includes these headers to draw/animate

### Recommended documentation
Add a `Sprites/README.md` explaining:
- where the *source* sprites are (PNG/Aseprite)
- which script to run (Python/Node/etc.)
- output folder for generated `.h` files
- naming conventions (frame order, size limits, transparency color, RGB565, etc.)

---

## 3D Printing (STL)
STL files are stored in `3DSTL/`.

Recommended additions:
- `3DSTL/README.md` with:
  - part list (case, lid, buttons, etc.)
  - print settings (layer height, infill, supports)
  - assembly notes and photos

---

## Wiring Plans
It’s strongly recommended to add a `wiring/` folder (or `docs/wiring/`) containing:
- `pinout.md` (GPIO table)
- `wiring_diagram.png` (or `.svg`)
- optional photos of the real build

### Example pinout table (template)
| Function | GPIO | Notes |
|---------:|:----:|------|
| Left     |  ?   | Pull-up recommended |
| Right    |  ?   | Pull-up recommended |
| Validate |  ?   | Pull-up recommended |
| Encoder A|  ?   | Optional |
| Encoder B|  ?   | Optional |
| Encoder SW | ?  | Optional |

Fill this with your real wiring once pinned down.

---

## Customization (recommended)
To make the project easy for others, add a small configuration section in code:
- **Input mode**: TOUCH / BUTTONS / ENCODER
- **Button/encoder GPIOs**
- **Touch calibration** (if needed)
- **Evolution thresholds** and **stage durations**
- **Save enable/disable** + SD pins (if applicable)

If you want, create a `config.h` and keep all user-tunable values there.

---

## Troubleshooting
### I only see a blank screen / white screen
- Make sure you selected an ESP32 board profile compatible with CYD
- Verify the display driver configuration (ILI9341 + correct pins for CYD)
- Check power and USB cable quality

### Touch doesn’t work
- Touch controller (often XPT2046) must be enabled/configured
- Touch calibration may be required for accurate taps

### Buttons/encoder don’t respond
- Confirm GPIOs in code match your wiring
- Use internal pull-ups or add external resistors
- Debounce may be needed for noisy switches

### Build fails with “library not found”
- Install missing libraries from Arduino Library Manager
- Ensure you use the correct ESP32 Arduino core version

---

## Roadmap (ideas)
- [ ] Full English localization (strings + docs)
- [ ] GitHub Releases with one-click flashing pack
- [ ] `wiring/` documentation (pinout + diagrams)
- [ ] Clear `Sprites/README.md` with the `.h` pipeline
- [ ] Cleaner folder naming (`sprites/`, `3d/`)

---

## Contributing
Suggestions, issues and PRs are welcome:
- Report bugs via **Issues**
- Propose improvements via **Pull Requests**
- Share new sprites or UI improvements (with clear licensing)

---

## License
Choose a license that fits your goals:
- **Code:** MIT is a popular, simple choice
- **Assets (sprites/3D):** you may prefer a Creative Commons license (e.g., CC BY)

*(Add your chosen license file in the repo root.)*

---

## Credits
- Created by **Inter-Raptor** (JurassicLife project)
- CYD community references and the ESP32 open-source ecosystem ❤️

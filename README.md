# 🦖 JurassicLife

JurassicLife is a **little virtual dinosaur** (Tamagotchi-style) living on an **ESP32 with a screen**.  
It stays there, close to you, on your desk (or in your pocket if you build a case), ready to continue its life as soon as you power it on.

The concept is simple:
- you take care of your dino,
- you watch it evolve,
- and you make sure it doesn’t spend its entire life… doing poop. 💩

> You can **play right away**… or **modify everything**: sprites, animations, UI, rules, pins, etc.  
> Basically: it’s made so you can have fun tinkering.

---

## ▶️ Video (demo)
[![JurassicLife – YouTube demo](https://img.youtube.com/vi/RPLaATQ_HNw/hqdefault.jpg)](https://youtu.be/RPLaATQ_HNw)

---

## 📌 Table of contents
1. What is JurassicLife?
2. Supported hardware (plug & play)
3. Install / upload (simple)
4. DIY mode (your own build)
5. Actions (GIFs)
6. Save (microSD required)
7. Customize sprites / animations
8. Repo structure

---

## 1) What is JurassicLife?
A small companion/game where you take care of a dinosaur:
- feed it 🍖
- give it a drink 💧
- wash it 🧼
- play with it 🎮
- give it hugs 💖
- and… handle the **“poop moments”** (yes, it’s part of the job).

---

## 2) ✅ Supported hardware (plug & play)
JurassicLife is designed to be **very easy to upload** if you have a supported board:

- **2432S022**
- **2432S028**
- **Classic ESP32 + ILI9341 320×240 screen** (DIY profile)

👉 With a supported board: pick the right profile in the code, upload, and you’re good.

---

## 3) ⬆️ Install / upload (simple)
Arduino code is here:
- `arduino/JurassicLife/`

And the configuration README (boards, audio, pins, encoder/buttons…) is here:
- `arduino/JurassicLife/README.md`

➡️ Basically: change a few `#define` lines at the top of the file, then **upload** from Arduino IDE.

---

## 4) 🧪 DIY mode (your own build)
You can also build your own setup:
- your ESP32
- your ILI9341 320×240 screen
- your buttons / encoder
- your 3D-printed case
- your custom sprites

This project is meant to be a **fun base** you can adapt however you want.

---

## 5) 🎬 Actions (GIF preview)

> This is the “daily life” part.  
> Your dino isn’t complicated… but it has needs. (Like all of us.)

### 🍖 Eat
When it’s hungry, it looks at you like “I haven’t eaten in 3 minutes”.

![Dino eats](screenshots/DinoMange.gif)

### 💧 Drink
Hydration = happy dino. And a happy dino breaks your karma a lot less.

![Dino drinks](screenshots/Dinoboit.gif)

### 🧼 Wash
Because yes… after certain activities, the dino deserves a quick cleanup.

![Dino washes](screenshots/Dinolave.gif)

### 🎮 Play
The dino needs to blow off steam. Otherwise it starts brooding. And a brooding dino… is suspicious.

![Dino plays](screenshots/Dinojoue.gif)

### 💩 Poop
The most noble part of the project: poop management.  
Don’t judge it. Help it. It’s your dino.

![Dino poop](screenshots/Dinocaca.gif)

### 💖 Hug
The secret recipe to boost morale: a hug.  
(And yes, it works even on dinosaurs.)

![Dino hug](screenshots/Dinocalins.gif)

---

## 6) 💾 Save: microSD required
If you want to find your dinosaur again **after power-off**:
➡️ you need a **microSD card**.

No microSD = no persistent save after restart.

---

## 7) 🎨 Customize sprites / animations
Want your own dino? Your own style? Your own scenery?
Everything about sprites/animations + `.h` conversion scripts is here:
- `Sprites/`

There’s a README there explaining how to create sprites and generate `.h` files.

---

## 8) 🗂️ Repo structure (quick)
- `arduino/` : Arduino code (the core of the project)
- `Sprites/` : sprites + `.h` conversion scripts
- `3DSTL/` : printable cases / parts
- `screenshots/` : screenshots + diagrams + GIFs
- `Modifencours/` : tests / WIP
- `archive/` : old / legacy (if present)

---

🦖 Have fun, tinker, customize… and take care of your dino (even when it poops).

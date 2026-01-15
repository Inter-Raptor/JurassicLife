# 🦖 JurassicLife

JurassicLife, c’est un **petit dinosaure virtuel** (style tamagotchi) qui vit sur un **ESP32 avec écran**.  
Il reste là, près de toi, sur ton bureau (ou dans ta poche si tu te fais un boîtier), prêt à reprendre sa vie dès que tu l’allumes.

Le concept est simple :
- tu t’occupes de ton dino,
- tu le vois évoluer,
- et tu t’assures qu’il ne passe pas sa vie à… faire caca. 💩

> Tu peux **jouer direct**… ou **tout modifier** : sprites, animations, UI, règles, pins, etc.  
> Bref : c’est fait pour que tu t’éclates.

---

## ▶️ Vidéo (démo)
[![JurassicLife – Démo YouTube](https://img.youtube.com/vi/RPLaATQ_HNw/hqdefault.jpg)](https://youtu.be/RPLaATQ_HNw)

---

## 📌 Table des matières
1. C’est quoi JurassicLife ?
2. Matériel supporté (plug & play)
3. Installation / téléversement (simple)
4. Mode DIY (ton propre montage)
5. Les actions (GIFs)
6. Sauvegarde (microSD obligatoire)
7. Personnaliser les sprites / animations
8. Organisation du repo

---

## 1) C’est quoi JurassicLife ?
Un petit jeu/compagnon où tu prends soin d’un dinosaure :
- lui donner à manger 🍖
- lui donner à boire 💧
- le laver 🧼
- jouer avec lui 🎮
- lui faire des câlins 💖
- et… gérer **les moments “caca”** (oui, ça fait partie du job).

---

## 2) ✅ Matériel supporté (plug & play)
JurassicLife est pensé pour être **très simple à téléverser** si tu as une carte supportée :

- **2432S022**
- **2432S028**
- **ESP32 classique + écran ILI9341 320×240** (profil DIY)

👉 Avec une carte supportée : tu choisis le bon profil dans le code, tu upload, et c’est parti.

---

## 3) ⬆️ Installation / téléversement (simple)
Le code Arduino est ici :
- `arduino/JurassicLife/`

Et le README “configuration” (cartes, audio, pins, encodeur/boutons…) est ici :
- `arduino/JurassicLife/README.md`

➡️ En gros : tu modifies quelques `#define` en haut du fichier, puis tu **téléverses** avec l’Arduino IDE.

---

## 4) 🧪 Mode DIY (ton propre montage)
Tu peux aussi faire ton montage perso :
- ton ESP32
- ton écran ILI9341 320×240
- tes boutons / encodeur
- ton boîtier imprimé en 3D
- tes sprites custom

Le projet est fait pour être une **base fun** que tu adaptes comme tu veux.

---

## 5) 🎬 Les actions (aperçu en GIF)

> Ici, c’est la partie “vie quotidienne”.  
> Ton dino n’est pas compliqué… mais il a des besoins. (Comme nous tous.)

### 🍖 Manger
Quand il a faim, il te regarde avec des yeux de “j’ai rien mangé depuis 3 minutes”.

![Dino mange](screenshots/DinoMange.gif)

### 💧 Boire
Hydratation = dino heureux. Et un dino heureux, c’est un dino qui casse moins ton karma.

![Dino boit](screenshots/Dinoboit.gif)

### 🧼 Laver
Parce que oui… après certaines activités, le dino mérite un petit coup de propre.

![Dino lave](screenshots/Dinolave.gif)

### 🎮 Jouer
Le dino a besoin de se défouler. Sinon, il rumine. Et un dino qui rumine… c’est suspect.

![Dino joue](screenshots/Dinojoue.gif)

### 💩 Caca
Le moment le plus noble du projet : la gestion du caca.  
Ne le juge pas. Aide-le. C’est ton dino.

![Dino caca](screenshots/Dinocaca.gif)

### 💖 Câlin
La recette secrète pour remonter le moral : un câlin.  
(Et oui, ça marche même sur les dinos.)

![Dino calin](screenshots/Dinocalins.gif)

---

## 6) 💾 Sauvegarde : microSD obligatoire
Si tu veux retrouver ton dinosaure **après une coupure** :
➡️ il faut une **carte microSD**.

Sans microSD : pas de sauvegarde persistante après redémarrage.

---

## 7) 🎨 Personnaliser les sprites / animations
Tu veux ton propre dino ? Ton propre style ? Tes propres décors ?
Tout ce qui concerne les sprites/animations + scripts de conversion est ici :
- `Sprites/`

Tu peux y trouver un README qui explique comment créer tes sprites et générer des fichiers `.h`.

---

## 8) 🗂️ Organisation du repo (vite fait)
- `arduino/` : le code Arduino (le cœur du projet)
- `Sprites/` : sprites + scripts de conversion `.h`
- `3DSTL/` : boîtiers / pièces imprimables
- `screenshots/` : captures + schémas + GIFs
- `Modifencours/` : tests / WIP
- `archive/` : ancien / legacy (si présent)

---

🦖 Amuse-toi, bidouille, customise… et prends soin de ton dino (même quand il fait caca).

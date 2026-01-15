# 🧩 Sprites (JurassicLife)

Ce dossier contient **les sprites / animations** (pixel-art) de JurassicLife, ainsi que les **scripts** qui permettent de convertir tes images en fichiers **`.h`** directement utilisables dans Arduino (tableaux en `PROGMEM`).

L’objectif : tu dessines ton dinosaure (ou tes objets) en PNG/Aseprite → tu lances un script → tu récupères un `.h` prêt à `#include` dans le code.

---

## 📌 Table des matières
1. Le workflow en 30 secondes  
2. Formats acceptés (PNG / Aseprite)  
3. Créer un sprite personnalisé (image fixe)  
4. Créer une animation (plusieurs frames)  
5. Générer les fichiers `.h` avec les scripts  
6. Où mettre les fichiers générés  
7. Conventions (noms, tailles, transparence)  
8. Dépannage  

---

## 1) 🚀 Le workflow en 30 secondes

1. Tu crées/édites tes sprites dans **Aseprite** (recommandé) ou en **PNG**.
2. Tu exportes :
   - soit **un PNG** (image fixe),
   - soit **plusieurs PNG** (une frame = un fichier),
   - soit un **spritesheet** (selon le script).
3. Tu lances un des scripts de ce dossier pour générer un fichier **`.h`**.
4. Tu inclus le `.h` dans le projet Arduino et tu affiches/anim ton sprite.

---

## 2) 🖼️ Formats acceptés (PNG / Aseprite)

- ✅ **PNG** : format recommandé pour la conversion
- ✅ **Aseprite** : tu peux travailler en `.aseprite`, mais au final il faut **exporter en PNG** pour les scripts.

> Astuce Aseprite : utilise une grille (ex: 80×80 / 64×64 / etc.) pour garder des frames propres.

---

## 3) ✍️ Créer un sprite personnalisé (image fixe)

### A) Créer l’image
- Dessine ton sprite (ex: `dino_custom.png`)
- Respecte la taille attendue par ton jeu (ex : 80×80, 96×96… selon tes sprites existants)
- Garde une transparence propre (voir section **7**)

### B) Exporter
- Export en **PNG**
- Si tu utilises Aseprite : `File > Export...` → PNG

---

## 4) 🎞️ Créer une animation (plusieurs frames)

Tu as 2 façons classiques :

### Option 1 — PNG par frame (simple et robuste)
- Tu exportes une suite de fichiers :
  - `walk_000.png`
  - `walk_001.png`
  - `walk_002.png`
  - ...

### Option 2 — Spritesheet
- Tu exportes une seule image contenant toutes les frames en ligne/colonne (selon ton script).

> Conseil : la méthode **PNG par frame** évite 90% des galères.

---

## 5) 🛠️ Générer les fichiers `.h` avec les scripts

Dans ce dossier, tu as **un ou plusieurs scripts** de conversion (souvent en Python).  
Comme les noms peuvent évoluer, le plus simple est :

1. Ouvre le dossier `Sprites/` et repère les scripts (ex : `*.py`).
2. Regarde le README/les commentaires en haut du script : souvent il y a la commande exacte à lancer.

### Exemple de commandes (génériques)

#### A) Conversion d’une image fixe (PNG → .h)
```bash
python script_image_to_h.py --input dino_custom.png --output dino_custom.h
```

#### B) Conversion d’une animation (frames → .h)
```bash
python script_anim_to_h.py --input ./walk_frames/ --output dino_walk.h
```

⚠️ Les noms `script_image_to_h.py` / `script_anim_to_h.py` sont des **exemples** : utilise les vrais noms présents dans ton dossier.

### Prérequis (si Python est utilisé)
- Installe Python 3
- (Optionnel) Installe les dépendances si besoin :
```bash
pip install -r requirements.txt
```
*(si un fichier `requirements.txt` existe dans ce dossier)*

---

## 6) 📦 Où mettre les fichiers générés

Une fois ton `.h` généré :
- Soit tu le gardes dans `Sprites/Generated/` (si tu as un dossier de sortie)
- Soit tu le copies dans le dossier du code Arduino qui inclut déjà les autres sprites/animations

👉 L’important : que ton `.ino` puisse faire :
```cpp
#include "dino_custom.h"
```

---

## 7) 📏 Conventions importantes (noms, tailles, transparence)

### ✅ Noms de fichiers
- Évite les espaces
- Utilise `snake_case`
- Pour les animations : `nom_000.png`, `nom_001.png`, etc.

### ✅ Tailles
- Garde exactement la même taille d’une frame à l’autre (sinon animation bancale)
- Si tu veux “cropper” autour du sprite : fais-le de façon identique sur toutes les frames (ou laisse le script le faire si prévu)

### ✅ Transparence
Selon le script, 2 systèmes existent :
- **Alpha PNG** (transparence normale)
- **Color key** (ex : fond magenta #FF00FF considéré transparent)

👉 Si ton script parle de “transparent color” / “key color”, alors tu dois mettre cette couleur en fond.

> Astuce : si tu vois une couleur “flashy” (magenta/vert) dans tes sprites existants, c’est souvent un color key.

---

## 8) 🧯 Dépannage

### Le `.h` est généré mais l’image est “bizarre” (couleurs fausses)
- Vérifie si le script convertit en **RGB565**
- Vérifie si tu as bien la transparence attendue (alpha vs color key)

### L’animation est dans le mauvais ordre
- Renomme tes frames en `xxx_000`, `xxx_001`, ...
- Attention aux noms comme `walk_1.png`, `walk_10.png`, `walk_2.png` (ordre alphabétique → mauvais)

### Le script ne trouve pas l’image
- Vérifie ton chemin (Windows : attention aux espaces)
- Lance la commande depuis le bon dossier :
```bash
cd Sprites
python ton_script.py ...
```

---

## 💡 Idées (pour rendre ça encore plus “noob friendly”)
Si tu veux, on peut améliorer cette partie en ajoutant :
- un `requirements.txt` (si nécessaire)
- un exemple “prêt à lancer” (1 image + 1 animation)
- un script `run.bat` Windows (double clic → génération)
- un dossier `Generated/` + une convention de sortie claire

Dis-moi juste quels sont les **noms exacts** des scripts dans `Sprites/` (ou fais une capture du dossier), et je te fais une version du README **100% exacte** avec les commandes réelles.

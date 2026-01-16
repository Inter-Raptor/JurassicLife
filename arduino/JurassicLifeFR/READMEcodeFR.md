# 🦖 JurassicLife (Arduino / ESP32)

Ce code est fait pour tourner sur :
- **2432S022**
- **2432S028**
- **ESP32 classique + écran ILI9341 320×240** (profil `DISPLAY_PROFILE_ILI9341_320x240`)

Le principe est simple : tu modifies 2–3 lignes au début du code, puis tu **téléverses**.

---

## 📌 Table des matières
1. Cartes supportées  
2. Modifier le type de carte (ligne 11)  
3. Activer / désactiver l’audio (ligne 14)  
4. Téléverser  
5. Option : encodeur rotatif cliquable ou 3 boutons  
6. Modifier les pins (bloc `#if DISPLAY_PROFILE...`)  
7. Schémas de câblage (encodeur / boutons)  
8. Différences d’interface entre 2432S022 et 2432S028  
9. Sauvegarde : carte SD obligatoire  

---

## 1) ✅ Cartes supportées

- **2432S022** : supportée (attention : pas de contrôles physiques prévus)
- **2432S028** : supportée (contrôles physiques possibles)
- **ESP32 + ILI9341 320×240** : supportée via `DISPLAY_PROFILE_ILI9341_320x240`

---

## 2) 🔁 Modifier le type de carte dans le code (ligne 11)

Pour choisir ta carte, tu modifies la ligne **11** :

```cpp
#define DISPLAY_PROFILE DISPLAY_PROFILE_2432S022
```

Tu remplaces **uniquement la partie de droite** par :

### ➜ Pour 2432S028
```cpp
#define DISPLAY_PROFILE DISPLAY_PROFILE_2432S028
```

### ➜ Pour ESP32 + écran ILI9341 320×240
```cpp
#define DISPLAY_PROFILE DISPLAY_PROFILE_ILI9341_320x240
```

> ✅ Ne change pas `#define DISPLAY_PROFILE` : tu changes seulement la valeur après.

---

## 3) 🔊 Activer / désactiver l’audio (ligne 14)

Tu peux activer ou désactiver l’audio en modifiant la ligne **14** :

```cpp
#define ENABLE_AUDIO 1
```

- `#define ENABLE_AUDIO 0` → audio désactivé  
- `#define ENABLE_AUDIO 1` → audio activé  

✅ Si tu actives l’audio, tu verras apparaître dans l’interface **un nouveau bouton** qui permet :
- de **switcher** entre les différents modes audio
- et aussi **augmenter / réduire le volume**

---

## 4) ⬆️ Téléverser (le plus simple)

Une fois que tu as choisi :
- ta carte (`DISPLAY_PROFILE`)
- et si tu veux l’audio (`ENABLE_AUDIO`)

➡️ Tu n’as plus qu’à **téléverser** depuis l’Arduino IDE.

---

## 5) 🎮 Option : ajouter un encodeur cliquable ou 3 boutons

En option, tu peux ajouter :
- un **encodeur rotatif cliquable**
- ou **3 boutons** de navigation (Gauche / OK / Droite)

⚠️ **Attention : ce n’est pas possible sur 2432S022** (manque de pins accessibles, donc ce n’est pas prévu “proprement”).  
➡️ Sur **2432S022**, l’utilisation recommandée = **tactile uniquement**.

Sur **2432S028** et sur le profil **ESP32 + ILI9341**, c’est possible.

---

## 6) 🧷 Modifier les pins utilisées (bloc dans le code)

Les pins se modifient ici (dans ton code) :

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
```

✅ Comment ça marche :
- Mettre une pin à `-1` = **désactiver**
- Tu choisis **soit** encodeur **soit** 3 boutons :
  - **Encodeur** : tu remplis `ENC_A / ENC_B / ENC_BTN` et tu laisses `BTN_* = -1`
  - **3 boutons** : tu mets `ENC_* = -1` et tu définis `BTN_LEFT / BTN_RIGHT / BTN_OK`

⚠️ Sur 2432S028, si tu utilises `ENC_BTN = 35` (ou un autre pin du même genre), il peut être nécessaire d’ajouter une **résistance pull-up** (voir ton schéma).

---

## 7) 🧷 Schémas de câblage (encodeur / boutons)

J’ai fait un schéma de câblage pour :
- le mode **encodeur**
- le mode **3 boutons**

➡️ Ils sont dans le dossier `screenshot/` du repo (images de câblage).

---

## 8) ⚠️ Différences d’interface : 2432S022 vs 2432S028

Attention : si tu es sur **2432S022**, tu n’auras pas exactement la même interface que sur **2432S028**.

- La **2432S028** a un écran plus grand → permet un affichage différent et plus confortable
- La **2432S022** est plus petite → l’interface est adaptée à cette taille

Donc c’est normal si l’affichage n’est pas identique.

---

## 9) 💾 Sauvegarde : carte SD obligatoire

Dans tous les cas, si tu veux une **sauvegarde après coupure** de ton dinosaure :
➡️ il est nécessaire d’utiliser une **carte microSD**.

Sans carte SD, tu perdras la sauvegarde après coupure/redémarrage.

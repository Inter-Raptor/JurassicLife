#pragma once

/*
  JurassicMusicRTTTL_Clear.h

  Chaque entrée = (QUAND la jouer) + RTTTL.
  RTTTL = "NOM:d=...,o=...,b=...:notes"
  Notes:
  - 'p' = pause
  - # = dièse
  - . = pointé
*/
// ============================================================================
// ACCUEIL (AU LANCEMENT DU JEU) ~3s
// ============================================================================
// b=120 + d=8 => 1 note = 250ms. 12 notes ≈ 3s.
static const char* RTTTL_HOME_INTRO =
  "HOME:d=8,o=6,b=120:c,e,g,c7,g,e,c,p,c,e,g,c7";

// ============================================================================
// PHASES / EVENEMENTS MAJEURS
// ============================================================================

// ÉCLOSION : à lancer quand tu passes en PHASE_HATCHING.
// -> Tu peux la mettre en boucle tant que l'éclosion n'est pas terminée.
static const char* RTTTL_HATCH_INTRO =
  "HATCH:d=16,o=6,b=180:8c,16p,8e,16p,8g,16p,8c7,16p,8b,16p,8c7";

// MORT (SANTE=0) : à lancer quand tu détectes health==0 et que tu passes en PHASE_TOMB.
// -> Souvent 1 seule fois (intro), ou en boucle si tu veux insister sur l'écran tombe.
static const char* RTTTL_DEATH_INTRO =
  "DEATH:d=8,o=5,b=90:8e,8d#,8d,8c#,8c,8p,4b4,8p,2a4";

// REPOSE EN PAIX (FIN NATURELLE) : à lancer quand tu termines le cycle (ex: fin après SENIOR)
// -> Souvent 1 seule fois (intro).
static const char* RTTTL_RIP_INTRO =
  "RIP:d=8,o=5,b=80:8c,8p,8d,8p,8c,8p,4a4,8p,2g4";

// ============================================================================
// VIEILLISSEMENT (AGE STAGE CHANGE)
// ============================================================================

// VIEILLISSEMENT : à jouer quand ageStage passe de JUNIOR -> ADULTE (1 fois)
static const char* RTTTL_AGEUP_JUNIOR_TO_ADULT =
  "AGEUP1:d=16,o=6,b=190:8c,16p,8e,16p,8g,16p,8c7";

// VIEILLISSEMENT : à jouer quand ageStage passe de ADULTE -> SENIOR (1 fois)
static const char* RTTTL_AGEUP_ADULT_TO_SENIOR =
  "AGEUP2:d=16,o=6,b=160:8g,16p,8a,16p,8b,16p,8c7";

// ============================================================================
// ACTIONS (PONCTUEL = AU DEBUT) + LOOPS (PENDANT PH_DO UNIQUEMENT)
// ============================================================================

// MANGER - INTRO : à lancer au début de l'action "manger" (quand tu entres en PH_DO pour TASK_EAT).
static const char* RTTTL_EAT_INTRO =
  "EATIN:d=16,o=5,b=167:16a,32p,16c#6";

// MANGER - LOOP : à lancer pendant PH_DO (TASK_EAT) uniquement.
// -> Stop dès que tu sors de PH_DO (pas pendant PH_GO / PH_RETURN).
static const char* RTTTL_EAT_LOOP =
  "EAT:d=16,o=6,b=220:16c,16p,16c,16p,16g,16p,16a,16p";

// BOIRE - INTRO : à lancer au début de l'action "boire" (entrée en PH_DO pour TASK_DRINK).
static const char* RTTTL_DRINK_INTRO =
  "DRINKIN:d=16,o=5,b=188:16f,32p.,16g#";

// BOIRE - LOOP : à lancer pendant PH_DO (TASK_DRINK) uniquement.
// -> Stop dès que tu sors de PH_DO (pas pendant PH_GO / PH_RETURN).
static const char* RTTTL_DRINK_LOOP =
  "DRINK:d=16,o=6,b=210:16e,16p,16d,16p,16c,16p,16d,16p";

// CACA - INTRO : à lancer au début de l'action "caca" (entrée en PH_DO pour TASK_POOP).
static const char* RTTTL_POOP_INTRO =
  "POOP:d=4,o=3,b=86:f#";

// CALIN - INTRO : à lancer au début de l'action "câlin" (entrée en PH_DO pour TASK_HUG).
static const char* RTTTL_HUG_INTRO =
  "HUG:d=16,o=5,b=100:16e,32p,16a,32p,16b";

// DODO - INTRO : à lancer quand tu entres en sommeil (début de TASK_SLEEP / entrée ST_SLEEP, etc.).
static const char* RTTTL_SLEEP_INTRO =
  "SLEEP:d=8,o=5,b=94:8c,32p.,8a4,32p.,8g4,16p,8a4.";

// ============================================================================
// ALERTES / SFX (BRUITS COURTS)
// ============================================================================

// ALERTE STATS CRITIQUES : à jouer dans audioTickAlerts() si healthCriticalCount()>0,
// au rythme de ton code (ex: toutes les 10s en AUDIO_TOTAL, 20s en AUDIO_LIMITED).
// -> Pour imiter audioPlayBeepCount(count), rejoue ce "beep+pause" 'count' fois (max 10).
static const char* RTTTL_ALERT_UNIT =
  "ALERT:d=8,o=7,b=250:c#7,p";

// MINI-JEU PLUIE : goutte touchée (quand mgDropsHit++ / collision goutte sur dino).
static const char* RTTTL_RAIN_HIT_SFX =
  "RAINHIT:d=32,o=7,b=125:d#7";

// MINI-JEU BALLONS : ballon attrapé (quand mgBalloonsCaught++ / catch ok).
static const char* RTTTL_BALLOON_CATCH_SFX =
  "BALCATCH:d=32,o=7,b=150:e7";

// (Optionnel) ALERTE EN BOUCLE : si tu préfères une alerte continue sans "count".
static const char* RTTTL_ALERT_LOOP =
  "ALERTL:d=32,o=7,b=300:32c#7,32p,32c#7,32p,32c#7,32p,32c#7,32p";

// DinoNames.h
#pragma once

#include <Arduino.h>
#include <pgmspace.h>

// Liste CSV de 200 prénoms (séparés par des virgules)
// (Sans accents pour éviter les soucis d'encodage selon les IDE/outils)
static const char DINO_NAMES_CSV[] PROGMEM =
  "Adam,Adrien,Alain,Albert,Alexandre,Alexis,Alphonse,Amaury,Andre,Antoine,"
  "Armand,Arthur,Auguste,Axel,Baptiste,Bastien,Benjamin,Bernard,Bruno,Camille,"
  "Cedric,Charles,Christian,Christophe,Claude,Clement,Damien,Daniel,David,Denis,"
  "Dominique,Edgar,Edouard,Elie,Emile,Emmanuel,Eric,Ernest,Etienne,Eugene,"
  "Fabien,Felix,Fernand,Florent,Francois,Frederic,Gabriel,Gael,Gautier,Gerard,"
  "Gilbert,Gilles,Gregory,Guillaume,Gustave,Hector,Henri,Herve,Hugo,Isaac,"
  "Jack,Jacques,Jean,Jeremie,Jerome,Joachim,Johan,Jonathan,Joseph,Jules,"
  "Julien,Justin,Karim,Kevin,Laurent,Leo,Leon,Leonard,Lionel,Loic,"
  "Louis,Luc,Lucas,Ludovic,Mael,Marc,Marcel,Marius,Martin,Mathieu,"
  "Maurice,Maxime,Michel,Nathan,Nicolas,Noe,Olivier,Oscar,Patrick,Paul,"
  "Philippe,Pierre,Quentin,Raphael,Raymond,Remi,Richard,Robert,Romain,Samuel,"
  "Sebastien,Serge,Simon,Stephane,Theo,Thierry,Thomas,Timothee,Tristan,Valentin,"
  "Victor,Vincent,William,Xavier,Yann,Yoann,Yohan,Achille,Aline,Amelie,"
  "Anais,Anne,Audrey,Beatrice,Benedicte,Brigitte,Cecile,Carole,Caroline,Catherine,"
  "Celine,Chantal,Charlotte,Christine,Claire,Clemence,Coralie,Danielle,Delphine,Denise,"
  "Diane,Eleonore,Elise,Elodie,Emilie,Emma,Estelle,Eugenie,Fabienne,Florence,"
  "Francoise,Gabrielle,Gaelle,Genevieve,Gisele,Helene,Ines,Isabelle,Jacqueline,Jade,"
  "Jeanne,Josephine,Judith,Julie,Juliette,Karine,Laure,Lea,Leonie,Liliane,"
  "Lisa,Lucie,Madeleine,Manon,Margaux,Marie,Marion,Martine,Mathilde,Melanie,"
  "Mireille,Monique,Nathalie,Nicole,Noemie,Odile,Pauline,Sandrine,Sophie,Valerie";

static inline uint16_t dinoCountCsvItems_P(const char* csvP) {
  if (!csvP) return 0;
  // Si string vide -> 0
  if (pgm_read_byte(csvP) == '\0') return 0;

  uint16_t count = 1;
  for (uint32_t i = 0;; i++) {
    char c = (char)pgm_read_byte(csvP + i);
    if (c == '\0') break;
    if (c == ',') count++;
  }
  return count;
}

// Copie un nom tiré au hasard dans out (outSize inclut le '\0')
static inline void getRandomDinoName(char* out, size_t outSize) {
  if (!out || outSize == 0) return;
  out[0] = '\0';

  const uint16_t n = dinoCountCsvItems_P(DINO_NAMES_CSV);
  if (n == 0) return;

  const uint16_t target = (uint16_t)random(n);

  // Aller au début du token target
  uint16_t idx = 0;
  uint32_t i = 0;
  while (idx < target) {
    char c = (char)pgm_read_byte(DINO_NAMES_CSV + i++);
    if (c == '\0') return; // sécurité
    if (c == ',') idx++;
  }

  // Copier jusqu'à virgule ou fin
  size_t w = 0;
  while (w + 1 < outSize) {
    char c = (char)pgm_read_byte(DINO_NAMES_CSV + i++);
    if (c == '\0' || c == ',') break;
    out[w++] = c;
  }
  out[w] = '\0';
}

#pragma once
#include stdint.h
#include pgmspace.h

#include triceratops_junior_90x90.h
#include triceratops_adulte_90x90.h
#include triceratops_senior_90x90.h

 Petit conteneur générique pour une anim (liste de frames + count)
struct AnimRef {
  const uint16_t const frames;  tableau de pointeurs (souvent en PROGMEM)
  uint8_t count;
};

struct DinoSpritesRef {
  uint16_t w, h, key;
  AnimRef walk;  marche (celle qu’on veut absolument selon l’âge)
};

 IMPORTANT  les arrays xxx_frames[] sont en PROGMEM chez toi,
 donc pour récupérer le pointeur d’une frame - pgm_read_ptr
static inline const uint16_t animGetFrame(const AnimRef& a, uint8_t idx) {
  if (!a.frames  a.count == 0) return nullptr;
  idx %= a.count;
  return (const uint16_t)pgm_read_ptr(&a.frames[idx]);
}

 stage 0=junior, 1=adulte, 2=senior (ça colle à ton enum si tu l’as fait comme d’hab)
static inline DinoSpritesRef triceratopsSpritesForStage(uint8_t stage) {
  using namespace triceratops;

  DinoSpritesRef s;
  s.w = W;
  s.h = H;
  s.key = KEY;

  switch (stage) {
    default
    case 0  JUNIOR
      s.walk = { triceratops_junior_marche_frames, triceratops_junior_marche_count };
      break;

    case 1  ADULTE
      s.walk = { triceratops_adulte_marche_frames, triceratops_adulte_marche_count };
      break;

    case 2  SENIOR
      s.walk = { triceratops_senior_marche_frames, triceratops_senior_marche_count };
      break;
  }

  return s;
}

// This game's own texts (game.h): the Game page's switch for the cinematics, in the launcher's languages. The first
// run's welcome is the launcher's own.

#include "game.h"

namespace launcher::game {

const GameTexts& GameTextsFor(uint32_t language) {
  static const GameTexts kEnglish{
      nullptr, nullptr,
      {"Cinematics without black bars", "No bars above and below the in-engine cinematics."}};
  static const GameTexts kGerman{
      nullptr, nullptr,
      {"Zwischensequenzen ohne Balken", "Keine schwarzen Balken oben und unten."}};
  static const GameTexts kFrench{
      nullptr, nullptr,
      {"Cinématiques sans bandes noires", "Ni en haut ni en bas de l'image."}};
  static const GameTexts kSpanish{
      nullptr, nullptr,
      {"Cinemáticas sin barras negras", "Sin barras arriba y abajo de la imagen."}};
  static const GameTexts kItalian{
      nullptr, nullptr,
      {"Filmati senza bande nere", "Niente bande sopra e sotto l'immagine."}};
  switch (language) {
    case 3:
      return kGerman;
    case 4:
      return kFrench;
    case 5:
      return kSpanish;
    case 6:
      return kItalian;
    default:
      return kEnglish;
  }
}

}  // namespace launcher::game

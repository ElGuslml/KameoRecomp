// This game's own texts (game.h): the Game page's switch for the cinematics, in the launcher's languages, and the
// first run's welcome and last screen in English and Spanish (Gustavo's mockup); in the others the first run's are
// the launcher's own.

#include "game.h"

namespace launcher::game {

const GameTexts& GameTextsFor(uint32_t language) {
  static const GameTexts kEnglish{
      "Welcome to the Enchanted Kingdom",
      "Before the adventure begins the game has to be installed. The launcher extracts its files from your Xbox 360 "
      "ISO; you only need to do it once.",
      {"Cinematics without black bars", "No bars above and below the in-engine cinematics."},
      "The kingdom awaits!",
      "The game was installed. Begin the adventure now, or first review the video, graphics and control settings; you "
      "can always change them in the launcher."};
  static const GameTexts kGerman{
      nullptr, nullptr,
      {"Zwischensequenzen ohne Balken", "Keine schwarzen Balken oben und unten."}};
  static const GameTexts kFrench{
      nullptr, nullptr,
      {"Cinématiques sans bandes noires", "Ni en haut ni en bas de l'image."}};
  static const GameTexts kSpanish{
      "Bienvenido al Reino Encantado",
      "Antes de comenzar la aventura hay que instalar el juego. El launcher extrae los archivos desde tu ISO de Xbox "
      "360; solo tendrás que hacerlo una vez.",
      {"Cinemáticas sin barras negras", "Sin barras arriba y abajo de la imagen."},
      "¡El reino te espera!",
      "El juego se instaló correctamente. Puedes comenzar la aventura ya o revisar primero los ajustes de video, "
      "gráficos y controles; siempre podrás cambiarlos desde el launcher."};
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

#include "catalog.h"

#include "platform.h"

std::string GameDef::localDir() const { return plat::dataDir() + "games/" + dir + "/"; }
std::string GameDef::dataJson() const { return plat::assetDir() + "data/" + id + ".json"; }
bool GameDef::playable() const {
  // Meses já suportados pelo motor (os restantes ficam "Coming soon").
  static const char* supported[] = {"oct", "nov", "dec", "jan", "feb", "mar", "apr", "may"};
  for (auto* s : supported)
    if (id == s) return plat::fileExists(dataJson());
  return false;
}

namespace catalog {

const std::vector<GameDef>& games() {
  static const std::vector<GameDef> g = {
      {0, "oct", "r3", "October 2024", "/logos/doodles/2024/rise-of-the-half-moon-6753651837110583.8-l.png", "october"},
      {1, "nov", "novr2", "November 2024",
       "/logos/doodles/2024/rise-of-the-half-moon-november-6753651837110598-l.png", "november"},
      {2, "dec", "december-r4", "December 2024",
       "/logos/doodles/2024/rise-of-the-half-moon-december-6753651837110600.2-l.png", "december"},
      {3, "jan", "moon_january-r1", "January 2025",
       "/logos/doodles/2025/celebrating-the-rise-of-the-half-moon-jan-6753651837110604-l.png", "jan"},
      {4, "feb", "moon_february-r4", "February 2025",
       "/logos/doodles/2025/celebrating-the-rise-of-the-half-moon-feb-6753651837110610.4-l.png", "february"},
      {5, "mar", "moon_march-rc2", "March 2025",
       "/logos/doodles/2025/rise-of-the-half-moon-march-6753651837110631.2-l.png", "march"},
      {6, "apr", "moon_april-rc2", "April 2025",
       "/logos/doodles/2025/rise-of-the-half-moon-april-6753651837110767.2-l.png", "april"},
      {7, "may", "moon_may-rc3", "May 2025",
       "/logos/doodles/2025/rise-of-the-half-moon-may-6753651837110778.2-l.png", "may"},
  };
  return g;
}

const std::vector<Language>& languages() {
  static const std::vector<Language> l = {
      {"en", "English"},      {"pt-PT", "Português (Portugal)", "Português (PT)"}, {"pt-BR", "Português (Brasil)", "Português (BR)"},
      {"es", "Español"},      {"fr", "Français"},                {"de", "Deutsch"},
      {"it", "Italiano"},     {"nl", "Nederlands"},
  };
  return l;
}

static const char* kSprites[] = {
    "play-sprite.png", "background-sprite.png", "moon-sprite-dark-moon-eye-open-in.png",
    "moon-sprite-dark-moon-eye-open-out.png", "moon-sprite-dark-moon-fade-in.png",
    "moon-sprite-light-moon-fade-in.png", "moon-sprite-light-moon-fade-out.png",
    "moon-sprite-light-moon-happy.png", "moon-sprite-light-moon-sad.png",
    "moon-sprite-light-moon-wildcard-gift.png", "moon-sprite-light-moon-win.png",
    "moon-sprite-light-moon-win-fade-out.png", "cards-sprite.png", "spinner-sprite.png",
    "progress-empty-sprite.png", "progress-star1-empty-to-full-sprite.png",
    "progress-star1-full-sprite.png", "progress-star1-full-to-empty-sprite.png",
    "progress-star2-empty-to-full-sprite.png", "progress-star2-full-sprite.png",
    "progress-star2-full-to-empty-sprite.png", "progress-star3-empty-to-full-sprite.png",
    "progress-star3-full-sprite.png", "rules-sprite.png",
    "GoogleFrame.png", "border.png", "border-card.png", "button_border.png", "cta.png", "audio.ogg",
};

std::vector<std::string> requiredFiles(const GameDef& g) {
  std::vector<std::string> f(std::begin(kSprites), std::end(kSprites));
  if (g.id == "oct" || g.id == "nov") f.push_back("main-sprite.png");
  f.push_back("messages.en.nocache.json");
  return f;
}

std::vector<std::string> downloadFiles(const GameDef& g) {
  auto f = requiredFiles(g);
  f.push_back("intro.mp4");  // vídeo de introdução (opcional: sem ele mostram-se só os textos)
  for (auto& l : languages())
    if (l.code != "en") f.push_back("messages." + l.code + ".nocache.json");
  return f;
}

bool isAvailable(const GameDef& g) {
  for (auto& f : requiredFiles(g))
    if (!plat::fileExists(g.localDir() + f)) return false;
  return true;
}

}  // namespace catalog

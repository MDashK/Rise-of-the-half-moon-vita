#pragma once
#include <string>
#include <vector>

struct GameDef {
  int index;              // 0..7
  std::string id;         // oct, nov, dec, jan, feb, mar, apr, may
  std::string dir;        // pasta no servidor da Google (ex.: moon_january-r1)
  std::string title;      // "October 2024"
  std::string thumbUrl;   // caminho do logótipo do doodle
  std::string monthKey;   // chave de tradução do nome do mês ("october", "jan", ...)

  std::string baseUrl() const { return "https://www.google.com/logos/2024/moon/" + dir + "/"; }
  std::string localDir() const;    // <dados>/games/<dir>/
  std::string thumbPath() const { return localDir() + "thumb.png"; }
  std::string dataJson() const;    // assets/data/<id>.json (incluído na app)
  bool playable() const;           // o motor já suporta este mês
};

struct Language {
  std::string code, name, shortName;
};

namespace catalog {
const std::vector<GameDef>& games();
const std::vector<Language>& languages();
// Ficheiros (relativos à pasta do jogo) que têm de existir para o jogo correr offline.
std::vector<std::string> requiredFiles(const GameDef& g);
// Todos os ficheiros a descarregar (inclui traduções de todas as línguas do menu).
std::vector<std::string> downloadFiles(const GameDef& g);
bool isAvailable(const GameDef& g);
}  // namespace catalog

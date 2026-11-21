#include "save.h"

#include "catalog.h"
#include "json.h"
#include "platform.h"

static std::string path(const std::string& id) { return plat::dataDir() + "save_" + id + ".json"; }

void Save::load(const std::string& id) {
  id_ = id;
  std::string t;
  if (!plat::readText(path(id), t)) return;
  Json j = Json::parse(t);
  seenTutorial = j["has_seen_tutorial"].boolean();
  seenWildcardTutorial = j["has_seen_wildcard_tutorial"].boolean();
  playedWildcardTutorial = j["has_played_wildcard_tutorial"].boolean();
  for (auto& w : j["unlocked_wildcards"].items()) unlockedWildcards.push_back(w.str());
}

void Save::save() const {
  Json j = Json::object();
  j.set("has_seen_tutorial", Json(seenTutorial));
  j.set("has_seen_wildcard_tutorial", Json(seenWildcardTutorial));
  j.set("has_played_wildcard_tutorial", Json(playedWildcardTutorial));
  Json a = Json::array();
  for (auto& w : unlockedWildcards) a.push(Json(w));
  j.set("unlocked_wildcards", a);
  std::string s = j.dump();
  plat::writeFile(path(id_), s.data(), s.size());
}

int Save::monthsWithTutorialSeen() {
  int n = 0;
  for (auto& g : catalog::games()) {
    Save s;
    s.load(g.id);
    if (s.seenTutorial) n++;
  }
  return n;
}

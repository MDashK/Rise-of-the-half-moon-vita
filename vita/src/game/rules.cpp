#include "rules.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace rules {

static const char* kPhases[8] = {"new_moon",      "waxing_crescent", "first_quarter", "waxing_gibbous",
                                  "full_moon",     "waning_gibbous",  "third_quarter", "waning_crescent"};

const char* phaseId(int v) { return v >= 0 && v < 8 ? kPhases[v] : ""; }

BoardDef BoardDef::fromJson(const Json& j) {
  BoardDef b;
  b.id = j["id"].str();
  std::map<std::string, int> index;
  for (auto& t : j["tiles"].items()) {
    index[t["id"].str()] = (int)b.tiles.size();
    b.tiles.push_back({t["id"].str(), (float)t["pos"]["x"].num(), (float)t["pos"]["y"].num()});
  }
  b.neighbors.resize(b.tiles.size());
  for (auto& kv : j["neighbors"].members()) {
    auto it = index.find(kv.first);
    if (it == index.end()) continue;
    for (auto& n : kv.second.items()) {
      auto jt = index.find(n.str());
      if (jt != index.end()) b.neighbors[it->second].push_back(jt->second);
    }
  }
  // As ligações são simétricas no jogo (cada par aparece nos dois sentidos); garante-o.
  for (int a = 0; a < (int)b.neighbors.size(); a++)
    for (int n : b.neighbors[a])
      if (std::find(b.neighbors[n].begin(), b.neighbors[n].end(), a) == b.neighbors[n].end())
        b.neighbors[n].push_back(a);
  b.cardSize = (float)j["cardSize"].num(65);
  b.cardExpandedSize = (float)j["cardExpandedSize"].num(75);
  b.tileSize = (float)j["tileSize"].num(50);
  return b;
}

Game::Game(const BoardDef& def) : def_(def), tiles_(def.tiles.size(), -1) {}

bool Game::full() const {
  for (int c : tiles_)
    if (c < 0) return false;
  return true;
}

std::vector<int> Game::emptyTiles() const {
  std::vector<int> r;
  for (int i = 0; i < (int)tiles_.size(); i++)
    if (tiles_[i] < 0) r.push_back(i);
  return r;
}

int Game::newCard(int value, int wildcard) {
  Card c;
  c.id = (int)cards_.size();
  c.value = value;
  c.wildcard = wildcard;
  cards_.push_back(c);
  return c.id;
}

void Game::place(int cardId, int tile) {
  if (tiles_[tile] >= 0) cards_[tiles_[tile]].tile = -1;
  tiles_[tile] = cardId;
  cards_[cardId].tile = tile;
}

int Game::removeFromTile(int tile) {
  int c = tiles_[tile];
  if (c < 0) return -1;
  tiles_[tile] = -1;
  cards_[c].tile = -1;
  cards_[c].owner = None;
  return c;
}

int Game::cardOn(int tile, int placedTile, int placedCard) const {
  if (tile == placedTile) return placedCard;
  return tiles_[tile];
}

int Game::valueAt(int tile, int placedTile, int placedCard) const {
  int c = cardOn(tile, placedTile, placedCard);
  return c < 0 ? -1 : cards_[c].value;
}

// Tl(): percorre ligações em que a fase avança "dir" (±1) a cada passo.
void Game::walk(int tile, int cardId, int dir, std::vector<int> path, std::vector<bool> visited,
                std::vector<std::vector<int>>& out, int placedTile, int placedCard) const {
  path.push_back(cardId);
  visited[tile] = true;
  int want = ((cards_[cardId].value + dir) % 8 + 8) % 8;
  std::vector<std::pair<int, int>> next;
  for (int n : def_.neighbors[tile]) {
    int c = cardOn(n, placedTile, placedCard);
    if (c < 0 || visited[n] || cards_[c].value < 0) continue;
    if (cards_[c].value == want) next.push_back({n, c});
  }
  if (next.empty()) out.push_back(path);
  else
    for (auto& nc : next) walk(nc.first, nc.second, dir, path, visited, out, placedTile, placedCard);
}

std::vector<Match> Game::findMatches(int cardId, int tile, Owner mover) const {
  std::vector<Match> res;
  const Card& c = cards_[cardId];
  if (c.value < 0) return res;
  for (int n : def_.neighbors[tile]) {
    int o = cardOn(n, tile, cardId);
    if (o < 0 || o == cardId || cards_[o].value < 0) continue;
    int v = cards_[o].value;
    if (v == c.value) res.push_back({MatchType::Pair, {cardId, o}, mover});
    if ((c.value + 4) % 8 == v) res.push_back({MatchType::FullMoon, {cardId, o}, mover});
  }
  std::vector<std::vector<int>> down, up;
  std::vector<bool> visited(def_.tiles.size(), false);
  walk(tile, cardId, -1, {}, visited, down, tile, cardId);
  walk(tile, cardId, +1, {}, visited, up, tile, cardId);
  std::vector<std::vector<int>> combos;
  for (auto& g : down)
    for (auto& h : up) {
      std::vector<int> l(g.rbegin(), g.rend());
      l.pop_back();
      l.insert(l.end(), h.begin(), h.end());
      combos.push_back(l);
    }
  std::set<std::vector<int>> seen;
  for (auto& l : combos) {
    std::set<int> s;
    for (size_t i = 0; i < l.size(); i++) {
      if (s.count(l[i])) {
        l.resize(i);
        break;
      }
      s.insert(l[i]);
    }
    if (l.size() >= 3 && seen.insert(l).second) res.push_back({MatchType::Run, l, mover});
  }
  return res;
}

MatchFilter filterTypes(std::vector<MatchType> types) {
  return [types](const Match& m, const std::vector<Card>&) {
    return std::find(types.begin(), types.end(), m.type) != types.end();
  };
}

MatchFilter filterHasPhase(std::vector<int> phases) {
  return [phases](const Match& m, const std::vector<Card>& cards) {
    for (int id : m.cards)
      if (std::find(phases.begin(), phases.end(), cards[id].value) != phases.end()) return true;
    return false;
  };
}

MatchFilter filterAll(std::vector<MatchFilter> filters) {
  return [filters](const Match& m, const std::vector<Card>& cards) {
    for (auto& f : filters)
      if (f && !f(m, cards)) return false;
    return true;
  };
}

static bool applies(const Modifier& m, const Match& match, const std::vector<Card>& cards) {
  if (m.owner != match.mover) return false;
  return !m.filter || m.filter(match, cards);
}

void Game::score(Match& m, std::vector<int>* consumed) const {
  double mult = 1, add = 0;
  const Modifier* over = nullptr;
  for (auto& mod : mods_) {
    if (mod.kind != ModKind::Mult && mod.kind != ModKind::Add && mod.kind != ModKind::Override) continue;
    if (!applies(mod, m, cards_)) continue;
    if (consumed && mod.step != 0) mod.value += mod.step;  // Cl(): o "updater" corre antes de ler o valor
    if (!mod.persistent && consumed) consumed->push_back(mod.id);
    if (mod.kind == ModKind::Mult) mult *= mod.value;
    else if (mod.kind == ModKind::Add) add += mod.value;
    else if (!over || mod.id > over->id) over = &mod;
  }
  if (over) {
    m.base = over->value;
    m.mult = 1;
    m.add = 0;
    m.override_ = true;
    return;
  }
  m.base = m.type == MatchType::Pair ? 1 : m.type == MatchType::FullMoon ? 2 : (double)m.cards.size();
  m.mult = mult;
  m.add = add;
  m.override_ = false;
}

void Game::removeModifiers(const std::vector<int>& ids) {
  mods_.erase(std::remove_if(mods_.begin(), mods_.end(),
                             [&](const Modifier& m) { return std::find(ids.begin(), ids.end(), m.id) != ids.end(); }),
              mods_.end());
}

void Game::claim(const Match& m) {
  for (int id : m.cards) {
    Card& c = cards_[id];
    if (c.owner != None && c.owner != m.mover && protectedCards[c.owner]) continue;
    c.owner = m.mover;
  }
}

int Game::addModifier(Modifier m) {
  m.id = nextModId_++;
  mods_.push_back(m);
  return m.id;
}

int Game::claimed(Owner o) const {
  int n = 0;
  for (int t : tiles_)
    if (t >= 0 && cards_[t].owner != None && benef(cards_[t].owner) == o) n++;
  return n;
}

int Game::ownedBy(Owner o) const {
  int n = 0;
  for (int t : tiles_)
    if (t >= 0 && cards_[t].owner == o) n++;
  return n;
}

Deck::Deck(std::mt19937& rng, int finiteCopies) : rng_(rng), finite_(finiteCopies > 0) {
  if (!finite_) return;
  for (int v = 0; v < 8; v++)
    for (int k = 0; k < finiteCopies; k++) pile_.push_back(v);
  for (int i = 0; i < 3; i++) std::shuffle(pile_.begin(), pile_.end(), rng_);
}

void Deck::returnCards(const std::vector<int>& values) {
  if (!finite_) return;
  pile_.insert(pile_.end(), values.begin(), values.end());
  std::shuffle(pile_.begin(), pile_.end(), rng_);
}

int Deck::draw() {
  if (finite_ && pile_.empty()) return -1;
  if (pile_.empty()) {
    for (int v = 0; v < 8; v++)
      for (int k = 0; k < 2; k++) pile_.push_back(v);
    for (int i = 0; i < 3; i++) std::shuffle(pile_.begin(), pile_.end(), rng_);
  }
  int v = pile_.front();
  pile_.erase(pile_.begin());
  return v;
}

AiParams AiParams::forLevel(int level, bool noWildcards, bool legacy) {
  static const int kn[6] = {2, 5, 7, 8, 9, 10};
  static const int knOld[6] = {2, 4, 6, 8, 9, 10};
  const double base = legacy ? 1.6 : 1.3;
  AiParams p;
  if (noWildcards && level < 3) {
    p.mean = 2;
    p.spread = base;
  } else {
    int i = std::max(0, std::min(5, level / 3 + (noWildcards ? -1 : 0)));
    p.mean = legacy ? knOld[i] : kn[i];
    p.spread = level < 18 + (noWildcards ? 3 : 0) ? base - (level % 3) * 0.3 : 1;
  }
  return p;
}

AiMove chooseAiMove(const Game& g, const std::vector<int>& hand, Owner me, const AiParams& p, std::mt19937& rng) {
  std::vector<AiMove> moves;
  for (int cid : hand)
    for (int t : g.emptyTiles()) {
      AiMove mv{cid, t, 0};
      for (auto& m : g.findMatches(cid, t, me)) {
        Match mm = m;
        g.score(mm, nullptr);
        mv.score += mm.total();
        if (g.bonusEnabled[me])
          for (int c : mm.cards) {
            Owner o = c == cid ? None : g.card(c).owner;
            if (o == None) mv.score += 1;
            else if (o != me) mv.score += 2;
          }
      }
      moves.push_back(mv);
    }
  if (moves.empty()) return {};
  std::shuffle(moves.begin(), moves.end(), rng);
  std::stable_sort(moves.begin(), moves.end(), [](const AiMove& a, const AiMove& b) { return a.score < b.score; });
  // jn(): escolha ponderada por uma gaussiana sobre a posição na lista ordenada.
  double var = p.spread * p.spread, sum = 0;
  std::vector<double> w(moves.size());
  for (size_t e = 0; e < moves.size(); e++) {
    double x = (double)e / moves.size() * 20 - 10 - p.mean;
    w[e] = std::exp(x * x / (-2 * var)) / std::sqrt(2 * M_PI * var);
    sum += w[e];
  }
  double r = std::uniform_real_distribution<double>(0, sum)(rng), acc = 0;
  for (size_t e = 0; e < moves.size(); e++) {
    acc += w[e];
    if (acc >= r) return moves[e];
  }
  return moves.back();
}

}  // namespace rules

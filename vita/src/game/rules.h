#pragma once
// Regras do "Rise of the Half Moon", transpostas do JavaScript original (versão de janeiro de 2025):
// funções Sl/Tl (combinações), Pl (pontuação com modificadores), ln/nn (IA) e $n (bónus final).
#include <functional>
#include <random>
#include <string>
#include <vector>

#include "json.h"

namespace rules {

enum Owner : int { None = -1, Player = 0, Moon = 1 };
enum class MatchType { Pair, FullMoon, Run };

const char* phaseId(int value);  // 0 -> "new_moon" ... 7 -> "waning_crescent"

struct TileDef {
  std::string id;
  float x, y;
};

struct BoardDef {
  std::string id;
  std::vector<TileDef> tiles;
  std::vector<std::vector<int>> neighbors;  // por índice de slot
  float cardSize = 65, cardExpandedSize = 75, tileSize = 50;
  static BoardDef fromJson(const Json& j);
};

struct Card {
  int id = -1;
  int value = -1;        // fase 0..7 (cartas de lua); -1 nos wildcards
  int wildcard = -1;     // índice do wildcard no mês
  Owner owner = None;    // quem "possui" a carta no tabuleiro (yb)
  int tile = -1;         // slot onde está (va)
};

struct Match {
  MatchType type;
  std::vector<int> cards;  // ids das cartas, por ordem (ciclo: ordem crescente de fase)
  Owner mover;
  // Pontos: base * mult + add (ou override)
  double base = 0, mult = 1, add = 0;
  bool override_ = false;
  double total() const { return base * mult + add; }
  int stars() const { return (int)(total() + 0.5); }
};

// Mult/Add/Override: pontos de cada combinação. BonusMult/BonusAdd: bónus das cartas possuídas no fim.
// EndMult: multiplica o total do nível no fim (Gémeos). EndSteal: no fim rouba "value" (fração) do total
// do adversário (Carneiro).
enum class ModKind { Mult, Add, Override, BonusMult, BonusAdd, EndMult, EndSteal };

using MatchFilter = std::function<bool(const Match&, const std::vector<Card>&)>;

struct Modifier {
  int id = 0;
  ModKind kind;
  double value = 0;
  Owner owner = Player;
  bool persistent = true;  // false: gasta-se depois de uma jogada (ex.: "a próxima combinação vale o triplo")
  double step = 0;         // soma-se a value sempre que é aplicado (Perigeu: cada par de lua cheia vale +1)
  // Filtro opcional (por omissão aplica-se a qualquer combinação do dono).
  MatchFilter filter;
};

// Filtros (classes Vk/sl e Uk/rl do original).
MatchFilter filterTypes(std::vector<MatchType> types);
MatchFilter filterHasPhase(std::vector<int> phases);  // alguma carta da combinação tem uma destas fases
MatchFilter filterAll(std::vector<MatchFilter> filters);

// Estado de um nível (tabuleiro + cartas + modificadores).
class Game {
 public:
  explicit Game(const BoardDef& def);

  const BoardDef& def() const { return def_; }
  int tileCount() const { return (int)def_.tiles.size(); }
  int cardAt(int tile) const { return tiles_[tile]; }
  std::vector<Card>& cards() { return cards_; }
  const std::vector<Card>& cards() const { return cards_; }
  Card& card(int id) { return cards_[id]; }
  const Card& card(int id) const { return cards_[id]; }
  bool full() const;
  std::vector<int> emptyTiles() const;

  int newCard(int value, int wildcard = -1);
  void place(int cardId, int tile);   // coloca sem pontuar
  int removeFromTile(int tile);       // devolve id da carta retirada (-1 se vazio)

  // Combinações formadas ao colocar cardId no slot tile (sem alterar o estado).
  std::vector<Match> findMatches(int cardId, int tile, Owner mover) const;
  // Aplica modificadores (preenche base/mult/add); se consume, marca os não persistentes para remoção.
  void score(Match& m, std::vector<int>* consumed) const;
  void removeModifiers(const std::vector<int>& ids);
  void claim(const Match& m);  // as cartas passam a pertencer a quem fez a combinação

  int addModifier(Modifier m);
  const std::vector<Modifier>& modifiers() const { return mods_; }

  int claimed(Owner o) const;  // cartas que contam para o bónus final de o (inclui as redirecionadas)
  int ownedBy(Owner o) const;  // cartas no tabuleiro com dono o
  Owner benef(Owner o) const { return o == None ? None : beneficiary[o]; }

  // Pontos de combinações (Ra) por jogador.
  double points[2] = {0, 0};
  // Lua Rosa: quem recebe os pontos e as cartas ganhas por cada jogador (Bl/pl do original).
  Owner beneficiary[2] = {Player, Moon};
  bool bonusEnabled[2] = {true, true};  // Xc
  bool canStackOnCards[2] = {false, false};  // Tb (Capricórnio: jogar por cima de cartas)
  bool protectedCards[2] = {false, false};   // Solstício: cartas não podem ser roubadas

 private:
  void walk(int tile, int cardId, int dir, std::vector<int> path, std::vector<bool> visited,
            std::vector<std::vector<int>>& out, int placedTile, int placedCard) const;
  int valueAt(int tile, int placedTile, int placedCard) const;
  int cardOn(int tile, int placedTile, int placedCard) const;

  BoardDef def_;
  std::vector<int> tiles_;  // id da carta em cada slot, -1 vazio
  std::vector<Card> cards_;
  mutable std::vector<Modifier> mods_;  // mutable: "step" avança ao pontuar de verdade
  int nextModId_ = 1;
};

// Baralho: 8 fases x2, baralhado; quando acaba, reabastece (classe Xm).
// Outubro (hm): baralho finito com ceil((slots+1)/8) cópias de cada fase, sem reabastecer; as cartas
// destruídas voltam ao baralho.
class Deck {
 public:
  explicit Deck(std::mt19937& rng, int finiteCopies = 0);
  int draw();  // -1 se o baralho finito acabou
  bool empty() const { return finite_ && pile_.empty(); }
  void returnCards(const std::vector<int>& values);
 private:
  std::mt19937& rng_;
  std::vector<int> pile_;
  bool finite_ = false;
};

struct AiMove {
  int cardId = -1, tile = -1;
  double score = 0;
};

// IA da Lua (função ln + parâmetros de nn).
struct AiParams {
  double mean = 2, spread = 1.3;
  // legacy: outubro/novembro usam [2,4,6,8,9,10] e dispersão 1,6.
  static AiParams forLevel(int level, bool noWildcards, bool legacy = false);
};
AiMove chooseAiMove(const Game& g, const std::vector<int>& hand, Owner me, const AiParams& p, std::mt19937& rng);

}  // namespace rules

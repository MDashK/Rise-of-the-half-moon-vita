#pragma once
#include <functional>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "app.h"
#include "assets.h"
#include "coro.h"
#include "rules.h"
#include "save.h"
#include "ui.h"
#include "video.h"

struct Vec2 {
  float x = 0, y = 0;
};
inline Vec2 lerp(Vec2 a, Vec2 b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }

// Estado visual de uma carta (de lua ou wildcard).
struct CardVis {
  int id = -1;            // id em rules::Game (cartas de lua) ou -100-i (wildcards)
  bool wildcard = false;
  int wildIndex = -1;
  Vec2 pos;
  float size = 84;
  float flip = 180;       // 0 = face, 180 = verso (rotateY do original)
  float alpha = 1;
  float glow = 0;         // brilho de seleção (pk)
  float burn = 0;         // animação de destruição (0..1)
  float rot = 0;
  bool dragging = false;
  bool used = false;      // wildcard já usado (Ob)
  bool dimmed = false;
  float dimT = 0;          // transição para o cinzento (0,25 s)
  int shownOwner = -1;    // dono mostrado (a face muda com uma transição de 250 ms)
  int ownerOverride = -2; // -2: segue as regras; senão, dono mostrado durante a animação de uma combinação
  int prevOwner = -1;
  float ownerT = 1;
};

// Onda circular do ecrã de progresso (ej): raio 0→200 e espessura 4→48 em 3 s, a desvanecer.
struct Ripple {
  Vec2 pos, offset;
  float t = 0;
};

struct StarVis {
  Vec2 pos;
  float size = 25, alpha = 1, rot = 0, scale = 1;
  bool light = true;
};

struct Layout {
  bool portrait = true;
  float W = 540, H = 960;
  float bScale = 1, bOx = 0, bOy = 0;  // transformação do tabuleiro
  float handY[2] = {745, 233};
  float handX[2] = {270, 270};
  float handCard[2] = {84, 55};
  float handGap = 15;
  float wildY = 849, wildSize = 65;
  int wildCols = 4;
  float wildGap = 15;
  Vec2 deck[2];
  Vec2 scoreStar[2];
  Align scoreAlign[2];
  RectF dialog;
  float dialogFont = 28;
  Vec2 moonPivot{270, 15};
  float moonScale = 1;
  RectF mute, rules, quit;
  float monthY = 898, monthFont = 30;
  float frameBorder = 38;

  Vec2 board(float x, float y) const { return {x * bScale + bOx, y * bScale + bOy}; }
  void fitBoard(const rules::BoardDef& b);
  std::vector<Vec2> handPositions(int owner, const std::vector<float>& sizes) const;
  Vec2 wildPos(int i, int n) const;
  static Layout make(bool portrait);
};

class GameScene : public Scene {
 public:
  explicit GameScene(const GameDef& g);
  ~GameScene() override;
  void update(float dt) override;
  void render() override;
  void touch(const Touch& t) override;
  void back() override;
  bool usesGameOrientation() const override { return true; }

  // ---------------------------------------------------------------- fluxos (corrotinas)
  co::Task<> mainFlow();
  co::Task<> loadingScreen();
  co::Task<> introScreen();
  co::Task<> tutorial();
  co::Task<> campaign();
  co::Task<int> playLevel(const rules::BoardDef& board, int level);  // 0 vitória, 1 derrota, 2 empate
  co::Task<> progressScreen(int mode);
  Vec2 progressSlotPos(int b) const;
  void ripple(int slot);
  co::Task<> wildcardReveal(int wildIndex);

  // passos do nível
  co::Task<> showBoard();
  co::Task<> deal();
  co::Task<> drawCard(int owner, bool sound);
  co::Task<> layoutHand(int owner, float ms = 700);
  struct Move {
    int card = -1;  // id da carta de lua, ou -1
    int tile = -1;
    int wild = -1;  // índice do wildcard jogado
  };
  co::Task<Move> playerMove();
  co::Task<Move> moonMove();
  co::Task<> executeMove(int owner, Move mv);
  co::Task<> animateMatch(rules::Match m);
  co::Task<> starsToScore(int owner, std::vector<StarVis> stars, std::vector<int> sfx, float stagger = 175);
  co::Task<> endBonus(int owner);
  co::Task<> playWildcard(int wildIndex);
  co::Task<> moonExpression(int state);
  co::Task<> showDialog(const std::string& text);
  co::Task<> hideDialog();
  co::Task<> dialogFor(const std::string& text, float ms);
  co::Task<int> chooseBoardCard(std::function<bool(int tile)> eligible, const std::string& prompt);
  co::Task<int> chooseMoonHandCard();                       // Taurus: devolve o id da carta escolhida
  co::Task<> destroyTiles(std::vector<int> tiles);
  co::Task<> claimTiles(std::vector<int> tiles, int owner);  // Ym + cl: as cartas mudam de dono, uma a uma
  co::Task<> scoreFade(int owner, bool out);                 // Ip(): esconde/mostra o número da pontuação
  co::Task<> stealStars(int from, int to, int n);            // dn: estrelas de uma pontuação para a outra
  co::Task<> endOfLevelExtras();                             // Lua Rosa, Carneiro e Gémeos depois do bónus
  bool showWildcardReminder(const std::string& wildId) const;
  co::Task<bool> wildcardModal(int wildIndex, bool info);  // true = jogar/continuar

  // utilitários
  CardVis& vis(int cardId);
  CardVis* wildVis(int wildIndex);
  void sfx(Snd s, float delay = 0) { audio::play(assets_.snd(s), delay); }
  Vec2 tilePos(int tile) const;
  float boardCardSize() const { return game_ ? game_->def().cardSize * layout_.bScale : 65; }
  void clearConnections(int tile);
  int phaseSnd(int value, bool player, bool star) const;

  // ---------------------------------------------------------------- render
  void renderBackground();
  void renderFrame();
  void renderMonth();
  void renderBoard();
  void renderCards();
  void renderCard(const CardVis& c);
  void renderHud();
  void renderDialog();
  void renderModal();
  void renderProgress();
  void renderRules();
  void renderLoading();
  void renderQuitConfirm();

  // ---------------------------------------------------------------- estado
  GameDef def_;
  MonthAssets assets_;
  Save save_;
  Layout layout_;
  co::Sched sched_;
  std::mt19937 rng_;
  bool loaded_ = false;
  volatile float audioProgress_ = 0;
  volatile bool audioDone_ = false;

  // nível
  std::unique_ptr<rules::Game> game_;
  std::unique_ptr<rules::Deck> deck_;
  std::vector<int> hand_[2];               // cartas de lua na mão
  std::map<int, CardVis> cards_;           // visual das cartas de lua
  std::vector<CardVis> wild_;              // wildcards do jogador (desbloqueados)
  std::vector<StarVis> stars_;
  std::vector<Ripple> ripples_;
  float tileAlpha_ = 0;
  std::map<std::pair<int, int>, std::pair<rules::MatchType, float>> connType_;
  std::set<int> glowTiles_;                // slots selecionáveis (a pulsar)
  int turn_ = 0;
  float turnAlpha_[2] = {0, 0};
  int shownScore_[2] = {0, 0};
  float scorePulse_[2] = {0, 0};
  float scoreAlpha_[2] = {1, 1}, scoreScale_[2] = {1, 1};
  int handCap_[2] = {3, 3};       // Pa.i: cartas na mão (Peixes +1; Touro 4/2)
  bool flowerMoonPending_ = false;  // Lua das Flores: a próxima jogada do jogador reclama a carta e as vizinhas
  double totalPts_[2] = {0, 0};
  int level_ = 0;                          // nível (vitórias seguidas) — b.g.i
  int setStars_ = -1;                      // estrela atual da série (b.g.g)
  int levelPoints_ = 0, totalPoints_ = 0;  // b.g.u e b.g.Ea
  bool levelActive_ = false;
  bool showHud_ = false;

  // lua (personagem)
  int moonState_ = 0;  // 0 = sem cara (como no original antes da introdução)
  const Anim* moonAnim_ = nullptr;
  bool moonAnimReverse_ = false;
  int moonAnimStart_ = 0, moonAnimEnd_ = 0;
  float moonFrame_ = 0;
  bool moonAnimDone_ = true;
  float bgRotation_ = 0, darkMoon_ = 0;
  int skyPos_ = 1;
  float fadeWhite_ = 0;

  // diálogo
  std::string dialogText_;
  float dialogAlpha_ = 0;
  float sceneAlpha_ = 1;

  // entrada do jogador
  struct Input {
    bool active = false;
    int selected = -1;        // id da carta de lua selecionada
    int dragCard = -1;
    long long touchId = 0;
    Vec2 dragOffset, dragStart;
    int snapTile = -1;
    bool done = false;
    Move result;
    std::function<bool(int)> tileEligible;
    // escolha de cartas no tabuleiro (wildcards)
    bool pickMode = false;
    std::function<bool(int)> pickEligible;
    int picked = -1;
    bool pickHand = false;  // escolher uma carta da mão da Lua (Touro)
    int pickedCard = -1;
    int wildDown = -1;  // wildcard onde o dedo pousou
  } in_;

  // modal (wildcard / revelação)
  struct Modal {
    bool open = false;
    float alpha = 0;
    int wild = -1;
    bool info = false;
    ui::Button a, b;
    int result = -1;
    bool touchStarted = false;
  } modal_;

  // ecrã de progresso
  struct Progress {
    bool active = false;
    int mode = 0;
    float alpha = 0;
    std::string top, bottom, title, nextTitle;
    float topA = 0, bottomA = 0, titleA = 0, statsA = 0, buttonsA = 0, nextA = 0;
    int levelStars = 0, totalStars = 0;
    float statsLevel = 0, statsTotal = 0;
    struct StarSlot {
      const Anim* anim = nullptr;
      float frame = 0;
      bool loop = false, done = false;
      float alpha = 0;
      Frame still;
    } slots[3];
    float lineFill[2] = {0, 0}, lineA[2] = {0, 0};
    ui::Button cont;
    bool continued = false;
    std::vector<int> wildShown;
    float wildA = 0;
  } prog_;

  // revelação de um wildcard (xo): Lua grande com o "presente" e a carta a subir
  struct Reveal {
    bool active = false;
    Vec2 moon;          // centro da Lua grande
    float moonD = 784;  // diâmetro
    float giftFrame = 0;
    float brightness = 1;
    CardVis card;
  } reveal_;
  void renderReveal();

  // regras, ecrã de carregamento, intro, saída
  bool rulesOpen_ = false;
  float rulesAlpha_ = 0;
  bool showRulesButton_ = false;
  bool loadingScreen_ = true;
  float loadingAlpha_ = 1, spinner_ = 0;
  VideoPlayer video_;
  bool videoActive_ = false;
  std::string introText_;
  float introAlpha_ = 0, introY_ = 600;
  bool skipVisible_ = false, skipPressed_ = false;
  ui::Button skipBtn_;
  bool quitConfirm_ = false;
  ui::Button quitYes_, quitNo_;
  ui::ButtonSet hudButtons_;
  ui::Button muteBtn_, rulesBtn_, quitBtn_;
  bool tutorialMode_ = false;
  int seenMonthsAtStart_ = 0;  // lj: meses com tutorial visto, contado ao abrir o jogo
  std::string error_;
  float time_ = 0;
};

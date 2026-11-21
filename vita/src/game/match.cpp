// Fluxo de um nível — transposição das funções ao(), Sn(), Vn(), Wn(), Xn(), pn.execute(), Im/Gm, Zn/$n.
#include <algorithm>
#include <cmath>

#include "game_scene.h"

using rules::Match;
using rules::MatchType;
using rules::Owner;

// ------------------------------------------------------------------ distribuir / mão

co::Task<> GameScene::layoutHand(int owner, float ms) {
  std::vector<float> sizes;
  for (size_t i = 0; i < hand_[owner].size(); i++) sizes.push_back(layout_.handCard[owner]);
  auto pos = layout_.handPositions(owner, sizes);
  std::vector<co::Task<>> anims;
  for (size_t i = 0; i < hand_[owner].size(); i++) {
    int id = hand_[owner][i];
    CardVis& c = vis(id);
    Vec2 from = c.pos, to = pos[i];
    float s0 = c.size, s1 = layout_.handCard[owner], f0 = c.flip, f1 = owner == 0 ? 0.f : 180.f;
    // Lk: posição/tamanho em 70% do tempo e depois a rotação (virar a carta) em 30%.
    anims.push_back([](GameScene* self, int id, Vec2 from, Vec2 to, float s0, float s1, float f0, float f1,
                       float ms) -> co::Task<> {
      co_await co::tween(ms * 0.7f, [=](float k) {
        CardVis& c = self->vis(id);
        if (!c.dragging) c.pos = lerp(from, to, k);
        c.size = s0 + (s1 - s0) * k;
      }, co::easeInOut);
      if (std::fabs(f1 - f0) > 0.5f)
        co_await co::tween(ms * 0.3f, [=](float k) { self->vis(id).flip = f0 + (f1 - f0) * k; }, co::easeInOut);
    }(this, id, from, to, s0, s1, f0, f1, ms));
  }
  co_await co::all(std::move(anims));
}

co::Task<> GameScene::drawCard(int owner, bool sound) {
  int value = deck_->draw();
  if (value < 0) co_return;  // outubro: o baralho acabou
  int id = game_->newCard(value);
  CardVis c;
  c.id = id;
  c.pos = layout_.deck[owner];
  c.size = layout_.handCard[owner];
  c.flip = 180;
  c.alpha = 0;
  cards_[id] = c;
  hand_[owner].push_back(id);
  co_await co::tween(100, [this, id](float k) { vis(id).alpha = k; });
  if (sound) sfx(S_DEAL, 200);
  co_await layoutHand(owner);
}

co::Task<> GameScene::deal() {
  // Sn(): 2 cartas para cada um, alternando; os wildcards por usar aparecem na fila de baixo.
  std::vector<co::Task<>> t;
  for (int k = 0; k < 2; k++) {
    t.push_back(drawCard(0, k == 0));
    t.push_back(drawCard(1, false));
  }
  int n = 0;
  for (auto& w : wild_)
    if (!w.used) n++;
  int i = 0;
  for (auto& w : wild_) {
    if (w.used) {
      w.alpha = 0;
      continue;
    }
    w.pos = layout_.wildPos(i++, n);
    w.size = layout_.wildSize;
    w.flip = 0;
    int wi = w.wildIndex;
    t.push_back(co::tween(250, [this, wi](float k) {
      if (auto* p = wildVis(wi)) p->alpha = k;
    }));
  }
  co_await co::all(std::move(t));
  co_await co::sleep(500);
}

co::Task<> GameScene::showBoard() {
  co_await co::tween(200, [this](float k) { tileAlpha_ = k; });
}

// ------------------------------------------------------------------ diálogo

co::Task<> GameScene::showDialog(const std::string& text) {
  if (dialogAlpha_ > 0.05f && !dialogText_.empty()) {
    float a0 = dialogAlpha_;
    co_await co::tween(300 * a0, [this, a0](float k) { dialogAlpha_ = a0 * (1 - k); });
    co_await co::sleep(50);
  }
  dialogText_ = text;
  co_await co::sleep(100);
  co_await co::tween(300, [this](float k) { dialogAlpha_ = k; });
}

co::Task<> GameScene::hideDialog() {
  float a0 = dialogAlpha_;
  if (a0 <= 0.01f) co_return;
  co_await co::tween(300, [this, a0](float k) { dialogAlpha_ = a0 * (1 - k); });
  dialogText_.clear();
}

co::Task<> GameScene::dialogFor(const std::string& text, float ms) {
  co_await showDialog(text);
  co_await co::sleep(ms);
  co_await hideDialog();
}

// ------------------------------------------------------------------ expressões da lua (Dj)

co::Task<> GameScene::moonExpression(int target) {
  struct T {
    const char* anim;
    bool reverse;
    int from, to;  // fatia de frames (-1 = até ao fim)
  };
  auto trans = [](int a, int b, T& out) {
    const char* fade = "moon-light-moon-fade-in";
    const char* happy = "moon-light-moon-happy";
    const char* sad = "moon-light-moon-sad";
    const char* win = "moon-light-moon-win";
    if (a == 0 && b == 1) out = {fade, false, 0, -1};
    else if (a == 0 && b == 5) out = {"moon-dark-moon-fade-in", false, 0, -1};
    else if (a == 1 && b == 0) out = {fade, true, 0, -1};
    else if (a == 1 && b == 2) out = {happy, false, 0, 29};
    else if (a == 1 && b == 3) out = {sad, false, 0, 29};
    else if (a == 1 && b == 4) out = {win, false, 0, -1};
    else if (a == 2 && b == 1) out = {happy, false, 29, -1};
    else if (a == 3 && b == 1) out = {sad, false, 29, -1};
    else if (a == 4 && b == 1) out = {win, true, 0, -1};
    else if (a == 5 && b == 6) out = {"moon-dark-moon-eye-open-in", false, 0, -1};
    else if ((a == 5 || a == 6) && b == 0) out = {"moon-dark-moon-eye-open-out", false, 0, -1};
    else return false;
    return true;
  };
  // espera pela transição anterior
  co_await co::until([this] { return moonAnimDone_; });
  if (moonState_ == target) co_return;
  T t;
  if (!trans(moonState_, target, t)) {
    // passa pelo estado neutro
    if (moonState_ != 1 && trans(moonState_, 1, t) && target != 1) {
      co_await moonExpression(1);
      co_await moonExpression(target);
      co_return;
    }
    if (!trans(moonState_, target, t)) {
      moonState_ = target;
      co_return;
    }
  }
  const Anim* a = assets_.anim(t.anim);
  moonState_ = target;
  if (!a || a->frames.empty()) co_return;
  moonAnim_ = a;
  moonAnimStart_ = t.from;
  moonAnimEnd_ = t.to < 0 ? (int)a->frames.size() : std::min(t.to, (int)a->frames.size());
  moonAnimReverse_ = t.reverse;
  moonFrame_ = 0;
  moonAnimDone_ = false;
  co_await co::until([this] { return moonAnimDone_; });
}

// ------------------------------------------------------------------ jogadas

co::Task<GameScene::Move> GameScene::playerMove() {
  if (dbg::autoplay) {
    co_await co::sleep(300);
    for (auto& w : wild_)
      if (!w.used && std::uniform_int_distribution<int>(0, 2)(rng_) == 0) {
        gfx::trace("auto: wildcard %s", assets_.wildcards()[w.wildIndex].id.c_str());
        co_return Move{-1, -1, w.wildIndex};
      }
    rules::AiParams p;
    p.mean = 10;
    p.spread = 1;
    auto mv = rules::chooseAiMove(*game_, hand_[0], Owner::Player, p, rng_);
    co_return Move{mv.cardId, mv.tile, -1};
  }
  for (;;) {
    in_ = Input();
    in_.active = true;
    in_.tileEligible = [this](int t) { return game_->cardAt(t) < 0 || game_->canStackOnCards[0]; };
    // qn(): a carta do meio fica selecionada por omissão
    if (!hand_[0].empty()) in_.selected = hand_[0][(hand_[0].size() + 1) / 2 - 1];
    glowTiles_.clear();  // no original os slots só pulsam com o rato por cima (aqui: ao arrastar)
    while (!in_.done) {
      for (int id : hand_[0]) {
        CardVis& c = vis(id);
        float target = id == in_.selected ? 1.f : 0.f;
        c.glow += (target - c.glow) * 0.25f;
      }
      co_await co::frame();
    }
    in_.active = false;
    glowTiles_.clear();
    for (int id : hand_[0]) vis(id).glow = 0;
    Move mv = in_.result;
    if (mv.wild < 0) co_return mv;
    // wildcard: confirmação (modal com "Jogar"/"Cancelar")
    bool play = co_await wildcardModal(mv.wild, false);
    if (play) {
      sfx(S_WILDCARD_PLAY);
      co_return mv;
    }
    sfx(S_CANCEL);
  }
}

co::Task<GameScene::Move> GameScene::moonMove() {
  bool noWild = wild_.empty();
  auto params = rules::AiParams::forLevel(level_, noWild, def_.id == "oct" || def_.id == "nov");
  co_await co::sleep(1000);
  auto mv = rules::chooseAiMove(*game_, hand_[1], Owner::Moon, params, rng_);
  co_return Move{mv.cardId, mv.tile, -1};
}

co::Task<> GameScene::destroyTiles(std::vector<int> tiles) {
  // In()/Wl(): brilho e desaparecimento, 150 ms entre cartas; depois saem do tabuleiro.
  std::vector<co::Task<>> seq;
  for (int t : tiles) {
    int cid = game_->cardAt(t);
    if (cid < 0) continue;
    clearConnections(t);
    co_await co::tween(500, [this, cid](float k) { vis(cid).burn = k; }, co::easeIn);
    co_await co::sleep(150);
  }
  for (int t : tiles) {
    int cid = game_->removeFromTile(t);
    if (cid >= 0) cards_.erase(cid);
  }
}

co::Task<> GameScene::executeMove(int owner, Move mv) {
  int cid = mv.card, tile = mv.tile;
  // on(): jogar por cima de outra carta (Capricórnio) destrói a que lá estava.
  if (game_->cardAt(tile) >= 0) co_await destroyTiles({tile});
  game_->place(cid, tile);
  hand_[owner].erase(std::find(hand_[owner].begin(), hand_[owner].end(), cid));
  vis(cid).glow = 0;
  sfx(S_PLACE);
  {
    CardVis& c = vis(cid);
    Vec2 from = c.pos, to = tilePos(tile);
    float s0 = c.size, s1 = boardCardSize(), f0 = c.flip;
    co_await co::tween(490, [this, cid, from, to, s0, s1](float k) {
      vis(cid).pos = lerp(from, to, k);
      vis(cid).size = s0 + (s1 - s0) * k;
    }, co::easeInOut);
    if (f0 > 0.5f) co_await co::tween(210, [this, cid, f0](float k) { vis(cid).flip = f0 * (1 - k); }, co::easeInOut);
  }
  // kn/Up (Lua das Flores): a carta jogada e as que lhe estão ligadas passam a ser do jogador.
  if (flowerMoonPending_) {
    flowerMoonPending_ = false;
    if (owner == Owner::Player) {
      co_await showDialog(assets_.t("wc_may_flowermoon_popup2"));
      std::vector<int> group{tile};
      for (int n : game_->def().neighbors[tile])
        if (game_->cardAt(n) >= 0) group.push_back(n);
      co_await claimTiles(group, owner);
      co_await co::sleep(1400);
      co_await hideDialog();
    }
  }
  auto matches = game_->findMatches(cid, tile, (Owner)owner);
  gfx::trace("move owner=%d tile=%s value=%d matches=%d", owner, game_->def().tiles[tile].id.c_str(),
             game_->card(cid).value, (int)matches.size());
  // No tutorial (dq) a cara da Lua não reage às combinações.
  if (!matches.empty() && !tutorialMode_) co::spawn(moonExpression(owner == Owner::Moon ? 2 : 3));
  std::vector<int> consumed;
  for (size_t h = 0; h < matches.size(); h++) {
    if (h > 0) co_await co::sleep(350);
    Match m = matches[h];
    game_->score(m, &consumed);
    gfx::trace("match owner=%d type=%d cards=%d pts=%.1f", owner, (int)m.type, (int)m.cards.size(), m.total());
    co_await animateMatch(m);
    game_->claim(m);
    for (int id : m.cards) vis(id).ownerOverride = -2;
    {
      // Diagnóstico: slots da combinação e verificação de que as cartas mudaram de dono.
      std::string tl;
      for (int id : m.cards) tl += " " + game_->def().tiles[game_->card(id).tile].id;
      gfx::trace("  tiles:%s", tl.c_str());
      for (int id : m.cards)
        if (game_->card(id).owner != m.mover && !game_->protectedCards[game_->card(id).owner])
          gfx::trace("  WARN card on tile %s not claimed (owner %d)", game_->def().tiles[game_->card(id).tile].id.c_str(),
                     (int)game_->card(id).owner);
    }
    game_->points[game_->benef((Owner)owner)] += m.total();  // Lua Rosa: os pontos da Lua vão para o jogador
  }
  game_->removeModifiers(consumed);
  if (!tutorialMode_) co::spawn(moonExpression(1));
}

// ------------------------------------------------------------------ animação de uma combinação

co::Task<> GameScene::starsToScore(int owner, std::vector<StarVis> stars, std::vector<int> sfxIds, float stagger) {
  if (stars.empty()) co_return;
  size_t base = stars_.size();
  for (auto& s : stars) {
    s.alpha = 0;
    stars_.push_back(s);
  }
  // aparecem (fade + subida de 20 px)
  co_await co::tween(250, [this, base, stars](float k) {
    for (size_t i = 0; i < stars.size(); i++) {
      if (base + i >= stars_.size()) break;
      stars_[base + i].alpha = k;
      stars_[base + i].pos = {stars[i].pos.x, stars[i].pos.y + 20 * (1 - k)};
    }
  }, co::easeOut);
  co_await co::sleep(100);
  std::vector<co::Task<>> fly;
  for (size_t i = 0; i < stars.size(); i++) {
    fly.push_back([](GameScene* self, size_t idx, StarVis s, int owner, float delay, int snd) -> co::Task<> {
      co_await co::sleep(delay);
      Vec2 from = s.pos, to = self->layout_.scoreStar[owner];
      co_await co::tween(500, [self, idx, from, to](float k) {
        if (idx >= self->stars_.size()) return;
        StarVis& st = self->stars_[idx];
        st.pos = lerp(from, to, k);
        st.rot = k * 6.2831853f * (500.f / 350.f);
        st.alpha = k > 0.9f ? (1 - k) * 10 : 1;
      }, co::easeInOut);
      if (idx < self->stars_.size()) self->stars_[idx].alpha = 0;
      self->shownScore_[owner]++;
      self->scorePulse_[owner] = 1;
      if (snd >= 0) self->sfx((Snd)snd);
    }(this, base + i, stars[i], owner, i * stagger, i < sfxIds.size() ? sfxIds[i] : -1));
  }
  co_await co::all(std::move(fly));
  // limpa as estrelas que já chegaram
  bool allGone = true;
  for (auto& s : stars_) allGone &= s.alpha <= 0.001f;
  if (allGone) stars_.clear();
}

co::Task<> GameScene::animateMatch(Match m) {
  const bool player = m.mover == Owner::Player;
  const int dest = game_->benef(m.mover);  // quem recebe as estrelas (Lua Rosa)
  const bool lightStar = dest == Owner::Player;
  const size_t n = m.cards.size();
  // Hm(): som da combinação
  Snd s;
  if (m.type == MatchType::Pair) s = player ? S_PLAYER_PAIR : S_MOON_PAIR;
  else if (m.type == MatchType::FullMoon) s = player ? S_PLAYER_FULLPAIR : S_MOON_FULLPAIR;
  else {
    int i = std::max(0, std::min((int)n - 3, 5));
    s = (Snd)((player ? S_PLAYER_RUN3 : S_MOON_RUN3) + i);
  }

  // ---- Im.create(): som, as cartas crescem e voltam à face neutra; diálogo com o nome da combinação
  sfx(s);
  float cs = boardCardSize(), ce = game_->def().cardExpandedSize * layout_.bScale;
  std::vector<co::Task<>> phase1;
  for (int id : m.cards) {
    vis(id).ownerOverride = -1;
    phase1.push_back(co::tween(300, [this, id, cs, ce](float k) { vis(id).size = cs + (ce - cs) * k; }, co::easeOut));
  }
  std::string label;
  if (m.type == MatchType::Pair) label = assets_.t("gameplay_phasepair");
  else if (m.type == MatchType::FullMoon) label = assets_.t("gameplay_fullmoonpair");
  else {
    label = assets_.t("gameplay_lunarcycle_" + std::to_string(n));
    if (label.empty()) {
      label = assets_.t("gameplay_lunarcycle");
      auto p = label.find("{{NUMBER}}");
      if (p != std::string::npos) label.replace(p, 10, std::to_string(n));
    }
  }
  // No tutorial o diálogo mostra o texto da lição, não o nome da combinação (Im sem diálogo).
  if (!tutorialMode_) phase1.push_back(showDialog(label));
  co_await co::all(std::move(phase1));
  co_await co::sleep(600);

  // ---- estrelas: xm (pares / override: grupos lado a lado por cima) ou Em (ciclos: um grupo por carta)
  std::vector<std::vector<StarVis>> groups;
  std::vector<std::vector<int>> groupSfx;
  float starSize = cs * 0.3f;
  int base = (int)std::lround(m.base), mult = (int)std::lround(m.mult);
  bool perCard = m.type == MatchType::Run && !m.override_;
  if (!perCard) {
    float minx = 1e9, maxx = -1e9, miny = 1e9;
    for (int id : m.cards) {
      Vec2 p = vis(id).pos;
      minx = std::min(minx, p.x - ce / 2);
      maxx = std::max(maxx, p.x + ce / 2);
      miny = std::min(miny, p.y - ce / 2);
    }
    float cx = (minx + maxx) / 2, y = miny - starSize * 0.5f - 10;
    int k = 0;
    for (int a = 0; a < base; a++) {
      float x = cx - (base * starSize + (base - 1) * 20) * 0.5f + starSize * 0.5f + (starSize + 20) * a;
      groups.emplace_back();
      groupSfx.emplace_back();
      for (int j = 0; j < mult; j++, k++) {
        groups.back().push_back({{x, y}, starSize, 1, 0, 1, lightStar});
        int cardIdx = m.type == MatchType::Pair ? 0 : a % (int)n;
        groupSfx.back().push_back(phaseSnd(game_->card(m.cards[cardIdx]).value, lightStar, true));
      }
    }
  } else {
    for (size_t i = 0; i < n; i++) {
      Vec2 p = vis(m.cards[i]).pos;
      float o = -ce * 0.5f + ce * 0.035f + starSize * 0.5f;
      groups.emplace_back();
      groupSfx.emplace_back();
      for (int j = 0; j < mult; j++) {
        groups.back().push_back({{p.x + o, p.y + o}, starSize, 1, 0, 1, lightStar});
        groupSfx.back().push_back(phaseSnd(game_->card(m.cards[i]).value, lightStar, true));
      }
    }
  }
  // as estrelas entram no vetor de desenho (invisíveis até aparecerem)
  std::vector<size_t> groupStart;
  for (auto& g : groups) {
    groupStart.push_back(stars_.size());
    for (auto s2 : g) {
      s2.alpha = 0;
      stars_.push_back(s2);
    }
  }

  // ---- Gm(): em cada carta, a nota de fase e a cor do novo dono (ul); ligações; estrelas a aparecer
  auto cardStep = [this, m, n](size_t i) -> co::Task<> {
    rules::Card& c = game_->card(m.cards[i]);
    bool p = m.mover == Owner::Player;
    Snd snd = (Snd)phaseSnd(c.value, p, false);
    if (m.type == MatchType::Pair) {
      if (i == 0) sfx(snd);
    } else if (m.type == MatchType::FullMoon) {
      sfx(snd, i * 160 * 0.75f);
    } else {
      sfx(snd);
    }
    bool keep = c.owner != Owner::None && c.owner != m.mover && game_->protectedCards[c.owner];
    vis(m.cards[i]).ownerOverride = keep ? c.owner : m.mover;
    co_await co::sleep(160);
    (void)n;
  };
  std::vector<int> tiles;
  for (int id : m.cards) tiles.push_back(game_->card(id).tile);
  auto connStep = [this, tiles, m](size_t i) -> co::Task<> {  // ligação entre a carta i-1 e a i
    auto key = std::make_pair(std::min(tiles[i - 1], tiles[i]), std::max(tiles[i - 1], tiles[i]));
    connType_[key] = {m.type, 0.f};
    co_await co::tween(150, [this, key](float k) {
      auto it = connType_.find(key);
      if (it != connType_.end()) it->second.second = k;
    });
  };
  auto appear = [this, groups, groupStart](size_t g) -> co::Task<> {
    size_t b = groupStart[g];
    auto stars = groups[g];
    co_await co::tween(250, [this, b, stars](float k) {
      for (size_t i = 0; i < stars.size() && b + i < stars_.size(); i++) {
        stars_[b + i].alpha = k;
        stars_[b + i].pos = {stars[i].pos.x, stars[i].pos.y + 20 * (1 - k)};
      }
    }, co::easeOut);
  };
  if (!tutorialMode_) co::spawn(hideDialog());
  if (perCard) {
    // ciclo: carta a carta, cada uma com a sua estrela, e a ligação seguinte entre elas
    for (size_t i = 0; i < n; i++) {
      std::vector<co::Task<>> t;
      t.push_back(cardStep(i));
      if (i < groups.size()) t.push_back(appear(i));
      co_await co::all(std::move(t));
      if (i + 1 < n) co_await connStep(i + 1);
    }
  } else {
    std::vector<co::Task<>> t;
    for (size_t i = 0; i < n; i++) t.push_back(cardStep(i));
    for (size_t i = 1; i < n; i++) t.push_back(connStep(i));
    for (size_t g = 0; g < groups.size(); g++) t.push_back(appear(g));
    co_await co::all(std::move(t));
  }
  co_await co::sleep(100);

  // ---- Lb: as estrelas voam para a pontuação (175 ms entre estrelas, +100 ms entre grupos)
  {
    std::vector<co::Task<>> fly;
    float delay = 0;
    for (size_t g = 0; g < groups.size(); g++) {
      for (size_t j = 0; j < groups[g].size(); j++) {
        size_t idx = groupStart[g] + j;
        int snd = groupSfx[g][j];
        fly.push_back([](GameScene* self, size_t idx, int owner, float delay, int snd) -> co::Task<> {
          co_await co::sleep(delay);
          if (idx >= self->stars_.size()) co_return;
          Vec2 from = self->stars_[idx].pos, to = self->layout_.scoreStar[owner];
          co_await co::tween(500, [self, idx, from, to](float k) {
            if (idx >= self->stars_.size()) return;
            StarVis& st = self->stars_[idx];
            st.pos = lerp(from, to, k);
            st.rot = k * 6.2831853f * (500.f / 350.f);
            st.alpha = k > 0.9f ? (1 - k) * 10 : 1;
          }, co::easeInOut);
          if (idx < self->stars_.size()) self->stars_[idx].alpha = 0;
          self->shownScore_[owner]++;
          self->scorePulse_[owner] = 1;
          if (snd >= 0) self->sfx((Snd)snd);
        }(this, idx, dest, delay, snd));
        delay += 175;
      }
      delay += 100;
    }
    co_await co::all(std::move(fly));
    bool allGone = true;
    for (auto& st : stars_) allGone &= st.alpha <= 0.001f;
    if (allGone) stars_.clear();
  }

  std::vector<co::Task<>> shrink;
  for (int id : m.cards)
    shrink.push_back(co::tween(200, [this, id, cs, ce](float k) { vis(id).size = ce + (cs - ce) * k; }, co::easeIn));
  co_await co::all(std::move(shrink));

  // Lm(): pontos extra somados (+N)
  int add = (int)std::lround(m.add);
  if (add > 0 && !m.override_) {
    co_await co::sleep(150);
    sfx(s);
    co_await showDialog("+" + std::to_string(add));
    std::vector<StarVis> extra;
    std::vector<int> es;
    Vec2 p = vis(m.cards[0]).pos;
    for (int a = 0; a < add; a++) {
      extra.push_back({{p.x - (add - 1) * (starSize + 20) / 2 + a * (starSize + 20), p.y - cs / 2 - 10}, starSize, 1, 0,
                       1, lightStar});
      es.push_back(phaseSnd(game_->card(m.cards[a % n]).value, lightStar, true));
    }
    co_await starsToScore(dest, extra, es, 150);
    co_await hideDialog();
  }
}

// ------------------------------------------------------------------ bónus final ($n)

co::Task<> GameScene::endBonus(int owner) {
  int claimed = game_->claimed((Owner)owner);
  double ra = game_->points[owner];
  double ha = claimed, total = ra + ha;
  if (game_->bonusEnabled[owner]) {
    double mult = 1, add = 0;
    for (auto& mod : game_->modifiers()) {
      if (mod.owner != owner) continue;
      if (mod.kind == rules::ModKind::BonusMult) mult *= mod.value;
      if (mod.kind == rules::ModKind::BonusAdd) add += mod.value;
    }
    if (mult > 1) {
      total += ha * (mult - 1);
      ha *= mult;
    }
    // Fm(): cada carta possuída dá "mult" estrelas
    std::vector<int> owned;
    for (int t = 0; t < game_->tileCount(); t++) {
      int c = game_->cardAt(t);
      if (c >= 0 && game_->card(c).owner == owner) owned.push_back(c);
    }
    std::sort(owned.begin(), owned.end(), [this](int a, int b) {
      Vec2 pa = vis(a).pos, pb = vis(b).pos;
      return std::fabs(pa.y - pb.y) >= 0.001f ? pa.y < pb.y : pa.x < pb.x;
    });
    if (!owned.empty()) {
      sfx(owner == 0 ? S_PLAYER_BONUS : S_MOON_BONUS);
      float cs = boardCardSize(), ce = game_->def().cardExpandedSize * layout_.bScale;
      std::vector<co::Task<>> grow;
      for (int id : owned)
        grow.push_back(co::tween(300, [this, id, cs, ce](float k) { vis(id).size = cs + (ce - cs) * k; }));
      co_await co::all(std::move(grow));
      co_await co::sleep(100);
      std::vector<StarVis> stars;
      std::vector<int> snds;
      float star = cs * 0.3f;
      for (int id : owned) {
        Vec2 p = vis(id).pos;
        float o = -ce * 0.5f + ce * 0.035f + star * 0.5f;
        for (int k = 0; k < (int)mult; k++) {
          stars.push_back({{p.x + o, p.y + o}, star, 1, 0, 1, game_->benef((Owner)owner) == 0});
          snds.push_back(phaseSnd(game_->card(id).value, game_->benef((Owner)owner) == 0, true));
        }
      }
      co_await starsToScore(game_->benef((Owner)owner), stars, snds);
      co_await co::sleep(100);
      std::vector<co::Task<>> shrink;
      for (int id : owned)
        shrink.push_back(co::tween(300, [this, id, cs, ce](float k) { vis(id).size = ce + (cs - ce) * k; }));
      co_await co::all(std::move(shrink));
    }
    if (add > 0) {
      ha += add;
      total += add;
      co_await co::sleep(350);
      sfx(owner == 0 ? S_PLAYER_BONUS : S_MOON_BONUS);
      co_await showDialog("+" + std::to_string((int)add));
      std::vector<StarVis> stars;
      Vec2 c = layout_.board(270, 450);
      float star = boardCardSize() * 0.3f;
      for (int a = 0; a < (int)add; a++)
        stars.push_back({{c.x - (add - 1) * (star + 20) / 2 + a * (star + 20), c.y}, star, 1, 0, 1, owner == 0});
      co_await starsToScore(owner, stars, {}, 120);
      co_await hideDialog();
    }
    co_await co::sleep(1000);
  } else {
    total -= ha;
    ha = 0;
  }
  totalPts_[owner] = total;
}

// ------------------------------------------------------------------ escolher carta no tabuleiro

co::Task<int> GameScene::chooseBoardCard(std::function<bool(int)> eligible, const std::string& prompt) {
  if (!prompt.empty()) co_await showDialog(prompt);
  in_.pickMode = true;
  in_.pickEligible = eligible;
  in_.picked = -1;
  glowTiles_.clear();
  for (int t = 0; t < game_->tileCount(); t++)
    if (eligible(t)) {
      glowTiles_.insert(t);
      vis(game_->cardAt(t)).glow = 1;
    }
  if (dbg::autoplay && !glowTiles_.empty()) {
    co_await co::sleep(600);
    std::vector<int> opts(glowTiles_.begin(), glowTiles_.end());
    in_.picked = opts[std::uniform_int_distribution<size_t>(0, opts.size() - 1)(rng_)];
  }
  co_await co::until([this] { return in_.picked >= 0; });
  for (int t : glowTiles_)
    if (game_->cardAt(t) >= 0) vis(game_->cardAt(t)).glow = 0;
  glowTiles_.clear();
  in_.pickMode = false;
  if (!prompt.empty()) co_await hideDialog();
  co_return in_.picked;
}

// ------------------------------------------------------------------ modal do wildcard

co::Task<bool> GameScene::wildcardModal(int wild, bool info) {
  modal_.wild = wild;
  modal_.info = info;
  modal_.result = -1;
  modal_.touchStarted = false;
  modal_.a = ui::Button{{}, info ? text::upper(assets_.t("continue")) : text::upper(assets_.t("play"))};
  modal_.b = ui::Button{{}, text::upper(assets_.t("cancel"))};
  modal_.a.onTap = [this] { modal_.result = 1; };
  modal_.b.onTap = [this] { modal_.result = 0; };
  modal_.open = true;
  if (dbg::autoplay) {
    co_await co::sleep(800);
    modal_.result = 1;
  }
  co_await co::until([this] { return modal_.result >= 0; });
  modal_.open = false;
  co_await co::until([this] { return modal_.alpha < 0.05f; });
  co_return modal_.result == 1;
}

// ------------------------------------------------------------------ nível completo (ao)

co::Task<int> GameScene::playLevel(const rules::BoardDef& board, int level) {
  game_ = std::make_unique<rules::Game>(board);
  deck_ = std::make_unique<rules::Deck>(rng_, def_.id == "oct" ? ((int)board.tiles.size() + 1 + 7) / 8 : 0);
  hand_[0].clear();
  hand_[1].clear();
  cards_.clear();
  stars_.clear();
  connType_.clear();
  shownScore_[0] = shownScore_[1] = 0;
  totalPts_[0] = totalPts_[1] = 0;
  for (int i = 0; i < 2; i++) {
    scoreAlpha_[i] = scoreScale_[i] = 1;
    handCap_[i] = 3;
  }
  flowerMoonPending_ = false;
  turn_ = 0;
  layout_.fitBoard(board);
  tileAlpha_ = 0;
  levelActive_ = true;
  showHud_ = true;
  showRulesButton_ = true;
  static const Snd music[3] = {S_MUSIC_GAME1, S_MUSIC_GAME2, S_MUSIC_GAME3};
  audio::playMusic(assets_.snd(music[level % 3]), 500);

  co_await showBoard();
  co_await deal();
  // Tn(): explicação dos wildcards na primeira vez que há exatamente um
  int unused = 0;
  for (auto& w : wild_) unused += !w.used;
  if (!save_.seenWildcardTutorial && wild_.size() == 1) {
    save_.seenWildcardTutorial = true;
    save_.save();
    co_await showDialog(assets_.t("wildcardintro2"));
    co_await co::sleep(3500);
    co_await showDialog(assets_.t("wildcardintro3"));
    co_await co::sleep(3000);
    co::spawn(hideDialog());
  }
  (void)unused;

  const bool october = def_.id == "oct";
  while (!game_->full()) {
    // an(): em outubro o nível também acaba quando o baralho e as duas mãos ficam vazios
    if (october && deck_->empty() && hand_[0].empty() && hand_[1].empty()) break;
    int who = turn_;
    // Vn()/Fn(): repõe a mão até à capacidade (3, ou outra com Peixes/Touro); outubro (Ym): uma carta por vez
    std::vector<co::Task<>> draws;
    int need = october ? 1 : std::max(0, handCap_[who] - (int)hand_[who].size());
    for (int i = 0; i < need; i++) draws.push_back(drawCard(who, i == 0));
    co_await co::all(std::move(draws));
    co::spawn(co::tween(200, [this, who](float k) { turnAlpha_[who] = k; }));
    bool again = true;
    while (again && !hand_[who].empty()) {
      Move mv = who == 0 ? co_await playerMove() : co_await moonMove();
      if (mv.wild >= 0) {
        co_await playWildcard(mv.wild);
        again = true;
      } else {
        if (mv.card < 0) break;
        co_await executeMove(who, mv);
        if (who == 0) game_->canStackOnCards[0] = false;  // Zb/Mb/Gb: só vale para uma jogada
        again = false;
      }
      if (game_->full()) break;
    }
    co::spawn(co::tween(200, [this, who](float k) { turnAlpha_[who] = 1 - k; }));
    turn_ = (turn_ + 1) % 2;
  }
  co_await co::sleep(500);
  // Zn(): pontos de bónus
  co_await showDialog(assets_.t("gameplay_bonuspoints"));
  co_await co::sleep(1500);
  co_await hideDialog();
  co_await endBonus(1);
  co_await endBonus(0);
  co_await endOfLevelExtras();
  levelPoints_ = (int)std::lround(totalPts_[0]);
  double p = totalPts_[0], m = totalPts_[1];
  gfx::trace("level %d board %s: player %.0f (Ra %.0f) moon %.0f (Ra %.0f)", level, board.id.c_str(), p,
             game_->points[0], m, game_->points[1]);
  co_return p > m ? 0 : p < m ? 1 : 2;
}


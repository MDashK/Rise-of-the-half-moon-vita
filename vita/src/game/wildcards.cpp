// Wildcards de todos os meses (classes "Ob"/"Mb"/"Pb" de cada jogo original) e efeitos de fim de nível.
#include <algorithm>
#include <cmath>

#include "game_scene.h"

using rules::MatchType;
using rules::ModKind;
using rules::Owner;

static co::Task<> both(co::Task<> a, co::Task<> b) {
  std::vector<co::Task<>> t;
  t.push_back(std::move(a));
  t.push_back(std::move(b));
  co_await co::all(std::move(t));
}

// ------------------------------------------------------------------ utilitários dos efeitos

bool GameScene::showWildcardReminder(const std::string& id) const {
  // De fevereiro em diante o lembrete "wildcard_finish" nunca é mostrado (Dd nunca é passado);
  // em dezembro, o Capricórnio desliga-o (vais jogar já por cima de uma carta).
  if (def_.id == "jan" || def_.id == "oct" || def_.id == "nov") return true;
  if (def_.id == "dec") return id != "wildcard_capricorn";
  return false;
}

co::Task<> GameScene::claimTiles(std::vector<int> tiles, int owner) {
  for (int t : tiles) {
    int c = game_->cardAt(t);
    if (c < 0) continue;
    game_->card(c).owner = (Owner)owner;
    co_await co::sleep(160);
  }
}

co::Task<> GameScene::scoreFade(int o, bool out) {
  // hj (sair): cresce 5% em 400 ms e desvanece nos últimos 200 ms; gj (entrar): aparece em 150 ms e volta ao
  // tamanho normal em 400 ms.
  co_await co::tween(400, [this, o, out](float k) {
    if (out) {
      scoreScale_[o] = 1 + 0.05f * k;
      scoreAlpha_[o] = k < 0.5f ? 1.f : 1.f - (k - 0.5f) * 2;
    } else {
      scoreScale_[o] = 1.05f - 0.05f * k;
      scoreAlpha_[o] = std::min(1.f, k * 400.f / 150.f);
    }
  }, co::easeOut);
}

co::Task<> GameScene::stealStars(int from, int to, int n) {
  if (n <= 0) co_return;
  Vec2 src = layout_.scoreStar[from], dst = layout_.scoreStar[to];
  size_t base = stars_.size();
  for (int i = 0; i < n; i++) stars_.push_back({src, 28, 0, 0, 1, to == Owner::Player});
  co_await co::tween(250, [this, base, n, src](float k) {
    for (int i = 0; i < n && base + i < stars_.size(); i++) {
      stars_[base + i].alpha = k;
      stars_[base + i].pos = {src.x, src.y + 20 * (1 - k)};
    }
  }, co::easeOut);
  std::vector<co::Task<>> fly;
  for (int i = 0; i < n; i++) {
    fly.push_back([](GameScene* self, size_t idx, int i, int from, int to, Vec2 a, Vec2 b) -> co::Task<> {
      co_await co::sleep(i * 275.f);  // 175 ms entre estrelas + 100 ms entre grupos (um por estrela)
      self->shownScore_[from]--;
      co_await co::tween(500, [self, idx, a, b](float k) {
        if (idx >= self->stars_.size()) return;
        StarVis& s = self->stars_[idx];
        s.pos = lerp(a, b, k);
        s.rot = k * 6.2831853f * (500.f / 350.f);
        s.alpha = k > 0.9f ? (1 - k) * 10 : 1;
      }, co::easeInOut);
      if (idx < self->stars_.size()) self->stars_[idx].alpha = 0;
      self->shownScore_[to]++;
      self->scorePulse_[to] = 1;
      self->sfx((Snd)self->phaseSnd(i % 8, to == Owner::Player, true));
    }(this, base + i, i, from, to, src, dst));
  }
  co_await co::all(std::move(fly));
  bool allGone = true;
  for (auto& s : stars_) allGone &= s.alpha <= 0.001f;
  if (allGone) stars_.clear();
}

co::Task<int> GameScene::chooseMoonHandCard() {
  if (hand_[1].empty()) co_return -1;
  // Fp(): as cartas da Lua viram-se para cima (como Lk: 70% do tempo sem mexer, 30% a rodar)
  co_await co::sleep(490);
  std::vector<int> ids = hand_[1];
  co_await co::tween(210, [this, ids](float k) {
    for (int id : ids) vis(id).flip = 180 * (1 - k);
  }, co::easeInOut);
  for (int id : ids) vis(id).glow = 1;
  in_.pickHand = true;
  in_.pickedCard = -1;
  if (dbg::autoplay) {
    co_await co::sleep(600);
    in_.pickedCard = ids[std::uniform_int_distribution<size_t>(0, ids.size() - 1)(rng_)];
  }
  co_await co::until([this] { return in_.pickedCard >= 0; });
  in_.pickHand = false;
  for (int id : ids) vis(id).glow = 0;
  co_return in_.pickedCard;
}

// ------------------------------------------------------------------ fim do nível (On/tn/Qn/Rn)

co::Task<> GameScene::endOfLevelExtras() {
  // Depois do bónus, os números mostrados passam a ser os totais (incluem as cartas redirecionadas).
  for (int o = 0; o < 2; o++) shownScore_[o] = (int)std::lround(totalPts_[o]);
  // Carneiro (hl/ol): o ladrão fica com uma fração do total do adversário, com estrelas a voar.
  std::vector<rules::Modifier> mods = game_->modifiers();
  for (auto& mod : mods) {
    if (mod.kind != ModKind::EndSteal) continue;
    int thief = mod.owner, victim = 1 - thief;
    int d = (int)std::ceil(mod.value * totalPts_[victim]);
    totalPts_[thief] += d;
    totalPts_[victim] -= d;
    gfx::trace("steal %d from %d", d, victim);
    co_await stealStars(victim, thief, d);
    co_await co::sleep(1500);
  }
  // Gémeos (nl/Tn): o total do nível é multiplicado (primeiro a Lua, depois o jogador).
  for (int o : {1, 0}) {
    double d = 1;
    for (auto& mod : mods)
      if (mod.kind == ModKind::EndMult && mod.owner == o) d *= mod.value;
    if (d == 1) continue;
    char buf[16];
    snprintf(buf, sizeof buf, "%gx", d);
    co::spawn(showDialog(d == 2 ? assets_.t("wc_may_gemini_popup2") : std::string(buf)));
    co_await scoreFade(o, true);
    totalPts_[o] *= d;
    shownScore_[o] = (int)std::lround(totalPts_[o]);
    co_await scoreFade(o, false);
    co_await co::sleep(1000);
    co_await hideDialog();
  }
}

// ------------------------------------------------------------------ jogar um wildcard

co::Task<> GameScene::playWildcard(int w) {
  CardVis* v = wildVis(w);
  if (v) {
    v->used = true;
    v->dimmed = true;
  }
  const WildcardDef& d = assets_.wildcards()[w];
  const std::string& id = d.id;
  co_await showDialog(assets_.t(d.popup));
  co_await co::sleep(def_.id == "oct" || def_.id == "nov" ? 2000 : 2500);
  co_await hideDialog();

  auto addMod = [this](ModKind kind, double value, Owner owner, rules::MatchFilter filter = nullptr,
                       bool persistent = true, double step = 0) {
    rules::Modifier mod;
    mod.kind = kind;
    mod.value = value;
    mod.owner = owner;
    mod.filter = filter;
    mod.persistent = persistent;
    mod.step = step;
    game_->addModifier(mod);
  };
  auto ownedTiles = [this](Owner o) {
    std::vector<int> r;
    for (int t = 0; t < game_->tileCount(); t++) {
      int c = game_->cardAt(t);
      if (c >= 0 && game_->card(c).owner == o) r.push_back(t);
    }
    return r;
  };
  auto withNeighbours = [this](int t) {  // Vp(): a carta e as que lhe estão diretamente ligadas
    std::vector<int> g{t};
    for (int n : game_->def().neighbors[t])
      if (game_->cardAt(n) >= 0) g.push_back(n);
    return g;
  };
  const auto moonOwned = [this](int t) {
    int c = game_->cardAt(t);
    return c >= 0 && game_->card(c).owner == Owner::Moon;
  };

  // Destrói cartas e devolve-as ao baralho (outubro: o baralho é finito).
  auto destroyToDeck = [this](std::vector<int> tiles) -> co::Task<> {
    std::vector<int> values;
    for (int t : tiles)
      if (game_->cardAt(t) >= 0) values.push_back(game_->card(game_->cardAt(t)).value);
    co_await destroyTiles(tiles);
    deck_->returnCards(values);
  };

  // ---- outubro
  if (id == "wildcard_scorpio") {
    game_->bonusEnabled[Owner::Moon] = false;
  } else if (id == "wildcard_super_moon") {
    addMod(ModKind::Mult, 2, Owner::Player, rules::filterTypes({MatchType::FullMoon}));
  } else if (id == "wildcard_leonids_meteor") {
    std::vector<int> onBoard;
    for (int t = 0; t < game_->tileCount(); t++)
      if (game_->cardAt(t) >= 0) onBoard.push_back(t);
    std::shuffle(onBoard.begin(), onBoard.end(), rng_);
    if (onBoard.size() > 2) onBoard.resize(2);
    if (!onBoard.empty()) co_await destroyToDeck(onBoard);
  } else if (id == "wildcard_hunter_moon") {
    auto t = ownedTiles(Owner::Moon);
    if (!t.empty()) co_await destroyToDeck(t);
  }
  // ---- novembro
  else if (id == "wildcard_winter_solstice") {
    game_->protectedCards[Owner::Player] = true;  // Nb = false: a Lua já não rouba as tuas cartas
  } else if (id == "wildcard_sagittarius") {
    addMod(ModKind::Mult, 3, Owner::Player, nullptr, false);  // Bb.push(3): só a próxima jogada com combinações
  } else if (id == "wildcard_geminid_meteor") {
    auto t = ownedTiles(Owner::Moon);
    std::shuffle(t.begin(), t.end(), rng_);
    t.resize((t.size() + 1) / 2);
    if (!t.empty()) {
      float cs = boardCardSize(), ce = game_->def().cardExpandedSize * layout_.bScale;
      std::vector<int> ids;
      for (int x : t) ids.push_back(game_->cardAt(x));
      co_await co::tween(300, [this, ids, cs, ce](float k) {
        for (int c : ids) vis(c).size = cs + (ce - cs) * k;
      });
      co_await claimTiles(t, Owner::Player);
      co_await co::sleep(250);
      co_await co::tween(200, [this, ids, cs, ce](float k) {
        for (int c : ids) vis(c).size = ce + (cs - ce) * k;
      });
    }
  } else if (id == "wildcard_beaver_moon") {
    bool any = false;
    for (int t = 0; t < game_->tileCount(); t++) any |= game_->cardAt(t) >= 0;
    if (any) {
      int t = co_await chooseBoardCard([this](int x) { return game_->cardAt(x) >= 0; }, "");
      int old = game_->card(game_->cardAt(t)).value;
      co_await destroyTiles({t});
      // tn: a fase "espelhada" (nova <-> cheia, crescente <-> minguante)
      int value = old == 0 ? 4 : old == 4 ? 0 : 8 - old;
      int cid = game_->newCard(value);
      CardVis c;
      c.id = cid;
      c.pos = tilePos(t);
      c.size = boardCardSize();
      c.flip = 0;
      c.alpha = 0;
      cards_[cid] = c;
      hand_[0].push_back(cid);
      co_await co::tween(300, [this, cid](float k) { vis(cid).alpha = k; });
      co_await executeMove(Owner::Player, Move{cid, t, -1});  // conta como jogada tua
    }
  }
  // ---- dezembro
  else if (id == "wildcard_capricorn") {
    game_->canStackOnCards[0] = true;
  } else if (id == "wildcard_long_night_moon") {
    game_->bonusEnabled[Owner::Moon] = false;
  } else if (id == "wildcard_light_of_mars") {
    addMod(ModKind::Add, 2, Owner::Player, rules::filterTypes({MatchType::Run}));
  } else if (id == "wildcard_quadrantids_meteor") {
    std::vector<int> onBoard;
    for (int t = 0; t < game_->tileCount(); t++)
      if (game_->cardAt(t) >= 0) onBoard.push_back(t);
    std::shuffle(onBoard.begin(), onBoard.end(), rng_);
    onBoard.erase(onBoard.begin(), onBoard.begin() + (onBoard.size() + 1) / 2);  // slice(ceil(n/2))
    if (!onBoard.empty()) co_await destroyTiles(onBoard);
  }
  // ---- janeiro
  else if (id == "wildcard_aquarius") {
    addMod(ModKind::BonusAdd, 10, Owner::Player);
  } else if (id == "wildcard_light_of_venus") {
    addMod(ModKind::BonusMult, 2, Owner::Player);
  } else if (id == "wildcard_moon_apogee") {
    addMod(ModKind::Override, 1, Owner::Moon);
  } else if (id == "wildcard_wolf_moon") {
    if (!ownedTiles(Owner::Moon).empty()) {
      int t = co_await chooseBoardCard(moonOwned, "");
      // Kn(): todas as cartas ligadas com o mesmo dono
      std::vector<int> group, stack{t};
      std::vector<bool> seen(game_->tileCount(), false);
      while (!stack.empty()) {
        int x = stack.back();
        stack.pop_back();
        if (seen[x]) continue;
        seen[x] = true;
        if (!moonOwned(x)) continue;
        group.push_back(x);
        for (int n : game_->def().neighbors[x]) stack.push_back(n);
      }
      co_await destroyTiles(group);
    }
  }
  // ---- fevereiro
  else if (id == "wildcard_pisces") {
    handCap_[0]++;
    co_await drawCard(0, true);
  } else if (id == "wildcard_snow_moon") {
    addMod(ModKind::Mult, 3, Owner::Player, rules::filterTypes({MatchType::FullMoon}));
  } else if (id == "wildcard_total_lunar_eclipse") {
    addMod(ModKind::Mult, 4, Owner::Player, rules::filterHasPhase({0}));
  } else if (id == "wildcard_equinox") {
    co_await both(scoreFade(0, true), scoreFade(1, true));
    std::swap(game_->points[0], game_->points[1]);
    std::swap(shownScore_[0], shownScore_[1]);
    co_await both(scoreFade(0, false), scoreFade(1, false));
  }
  // ---- março
  else if (id == "wildcard_aries") {
    addMod(ModKind::EndSteal, 0.5, Owner::Player);
  } else if (id == "wildcard_worm_moon") {
    addMod(ModKind::Mult, 2, Owner::Player, rules::filterTypes({MatchType::Run}));
  } else if (id == "wildcard_solar_eclipse") {
    addMod(ModKind::Mult, 4, Owner::Player, rules::filterHasPhase({1, 7}));
  } else if (id == "wildcard_lyrids_meteor") {
    auto t = ownedTiles(Owner::Moon);
    if (!t.empty()) co_await destroyTiles(t);
  }
  // ---- abril
  else if (id == "wildcard_taurus") {
    int cid = co_await chooseMoonHandCard();
    if (cid >= 0) {
      hand_[1].erase(std::find(hand_[1].begin(), hand_[1].end(), cid));
      hand_[0].push_back(cid);
      sfx(S_DEAL, 200);
      co_await both(layoutHand(0), layoutHand(1));
    }
    handCap_[1] = 2;
    handCap_[0] = 4;
  } else if (id == "wildcard_pink_moon") {
    game_->beneficiary[Owner::Moon] = Owner::Player;
  } else if (id == "wildcard_eta_aquarids_meteor") {
    auto t = ownedTiles(Owner::None);
    if (!t.empty()) co_await destroyTiles(t);
  } else if (id == "wildcard_close_approach") {
    addMod(ModKind::Add, 1, Owner::Player, rules::filterTypes({MatchType::Pair, MatchType::FullMoon}));
  }
  // ---- maio
  else if (id == "wildcard_gemini") {
    addMod(ModKind::EndMult, 2, Owner::Player);
  } else if (id == "wildcard_flowe_moon") {
    game_->canStackOnCards[0] = true;
    flowerMoonPending_ = true;
  } else if (id == "wildcard_moon_at_perigee") {
    addMod(ModKind::Add, 0, Owner::Player, rules::filterTypes({MatchType::FullMoon}), true, 1);
  } else if (id == "wildcard_great_global_cluster_in_hercules") {
    if (!ownedTiles(Owner::Moon).empty()) {
      int t = co_await chooseBoardCard(moonOwned, "");
      co_await showDialog(assets_.t("wc_may_heracles_popup2"));
      co_await claimTiles(withNeighbours(t), Owner::Player);
      co_await co::sleep(1400);
      co_await hideDialog();
    }
  } else {
    gfx::trace("WARN wildcard %s not implemented", id.c_str());
  }

  // Dn(): na primeira vez, lembra que ainda falta jogar uma carta de lua
  if (!save_.playedWildcardTutorial) {
    save_.playedWildcardTutorial = true;
    save_.save();
    if (showWildcardReminder(id)) {
      co_await co::sleep(500);
      co_await showDialog(assets_.t("wildcard_finish"));
      co_await co::sleep(3000);
      co::spawn(hideDialog());
    }
  }
}

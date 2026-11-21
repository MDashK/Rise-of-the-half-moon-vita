// Ecrãs fora do tabuleiro: carregamento, introdução, tutorial, campanha, progresso, wildcards e regras.
#include <algorithm>
#include <cmath>

#include "game_scene.h"
#include "platform.h"

using rules::Owner;

// ------------------------------------------------------------------ configuração da campanha por mês

struct CampaignCfg {
  std::string tutorialBoard, first, firstIfSeen, second, third;
  std::vector<std::string> l1, l2, l3;
};

// Lida do JSON extraído do jogo original (chave "campaign"); valores por omissão se faltar.
static CampaignCfg campaignFor(const Json& data) {
  CampaignCfg c = {"board3x3", "board3x3", "board3x3", "board4x4", "board4x5", {"board3x4"}, {"board4x4"}, {"board4x5"}};
  if (!data.has("campaign")) return c;
  const Json& j = data["campaign"];
  c.tutorialBoard = j["tutorial"].str(c.tutorialBoard);
  c.first = j["first"].str(c.first);
  c.firstIfSeen = j["firstIfSeen"].str(c.firstIfSeen);
  c.second = j["second"].str(c.second);
  c.third = j["third"].str(c.third);
  std::vector<std::string>* lists[3] = {&c.l1, &c.l2, &c.l3};
  for (size_t i = 0; i < 3 && i < j["lists"].items().size(); i++) {
    std::vector<std::string> l;
    for (auto& s : j["lists"].items()[i].items()) l.push_back(s.str());
    if (!l.empty()) *lists[i] = l;
  }
  return c;
}

// ------------------------------------------------------------------ fluxo principal

co::Task<> GameScene::mainFlow() {
  co_await loadingScreen();
  if (!error_.empty()) co_return;
  co_await introScreen();
  if (!save_.seenTutorial) {
    co_await tutorial();
    save_.seenTutorial = true;
    save_.save();
  }
  co_await campaign();
}

co::Task<> GameScene::loadingScreen() {
  loadingScreen_ = true;
  co_await co::frame();
  co_await co::frame();
  if (!assets_.load(def_, app::settings().language)) {
    error_ = "Could not load the game files. Please download the game again from the main menu.";
    co_return;
  }
  loaded_ = true;
  std::string ogg = def_.localDir() + "audio.ogg", pcm = def_.localDir() + "audio.pcm";
  audioDone_ = false;
  app::runAsync([this, ogg, pcm] {
    audio::loadGameAudio(ogg, pcm, &audioProgress_);
    audioDone_ = true;
  });
  float start = sched_.now();
  co_await co::until([this] { return (bool)audioDone_; });
  float elapsed = sched_.now() - start;
  if (elapsed < 1500) co_await co::sleep(1500 - elapsed);
  co_await co::tween(750, [this](float k) { fadeWhite_ = k; });  // Mm(): termina em branco
  loadingScreen_ = false;
}

co::Task<> GameScene::introScreen() {
  // Gp: vídeo de introdução com os textos por cima (sincronizados com o tempo do vídeo).
  skipPressed_ = false;
  skipVisible_ = true;
  skipBtn_.label = assets_.t("skip");
  auto& lines = assets_.intro();
  size_t next = 0;
  bool haveVideo = !dbg::autoplay && video_.open(def_.localDir() + "intro.mp4");
  gfx::trace("intro: video %s", haveVideo ? "opened" : "not available");
  if (haveVideo) {
    videoActive_ = true;
    video_.setVolume(1);
    co_await co::tween(750, [this](float k) { fadeWhite_ = 1 - k; });  // Om()
    float t0 = sched_.now(), fadeOutStart = -1;
    bool fading = false;
    for (;;) {
      float t = video_.timeMs() > 0 ? video_.timeMs() : sched_.now() - t0;
      while (next < lines.size() && t >= lines[next].start) {
        auto& l = lines[next++];
        if (l.type == 0) {
          introText_ = assets_.t(l.text);
          introY_ = l.y;
          co::spawn(co::tween(300, [this](float k) { introAlpha_ = k; }));
        } else {
          co::spawn(co::tween(300, [this](float k) { introAlpha_ = 1 - k; }));
        }
      }
      // Ep(): aos 29 s (ou ao saltar) o ecrã fica branco em 2 s; ao saltar o som também desce.
      if (!fading && (t > 29000 || skipPressed_)) {
        fading = true;
        bool fadeVolume = skipPressed_;
        fadeOutStart = sched_.now();
        co::spawn(co::tween(2000, [this, fadeVolume](float k) {
          fadeWhite_ = k;
          if (fadeVolume) video_.setVolume(1 - k);
        }));
      }
      if (fading && sched_.now() - fadeOutStart >= 2000) break;
      if (video_.ended() && !fading) break;
      co_await co::frame();
    }
    bool showedVideo = video_.hasFrame();
    skipVisible_ = false;
    introAlpha_ = 0;
    introText_.clear();
    video_.close();
    videoActive_ = false;
    fadeWhite_ = 1;
    assets_.loadDeferred();  // com o ecrã branco: carrega o resto das texturas agora que o vídeo libertou a memória
    if (showedVideo || skipPressed_) {
      co_await co::tween(750, [this](float k) { fadeWhite_ = 1 - k; });  // u(): Om()
      co_await moonExpression(1);                                        // Dj(Lj, 1)
      co_return;
    }
    // O vídeo não chegou a mostrar nada: segue com a introdução só com textos.
    gfx::trace("intro: video produced no frames, using text intro");
    next = 0;
    skipVisible_ = true;
  }
  // Sem vídeo (PC ou ficheiro em falta): só os textos sobre o céu.
  assets_.loadDeferred();
  if (fadeWhite_ > 0) co_await co::tween(750, [this](float k) { fadeWhite_ = 1 - k; });
  audio::playMusic(assets_.snd(S_MUSIC_TUTORIAL), 1500);
  float t0 = sched_.now();
  bool spinIn = false;
  if (dbg::autoplay) skipPressed_ = true;
  while (!skipPressed_ && next < lines.size()) {
    float t = sched_.now() - t0;
    if (!spinIn && t > 500) {
      spinIn = true;
      co::spawn(moonExpression(1));
    }
    auto& l = lines[next];
    if (t >= l.start) {
      next++;
      if (l.type == 0) {
        introText_ = assets_.t(l.text);
        introY_ = l.y;
        co::spawn(co::tween(300, [this](float k) { introAlpha_ = k; }));
      } else {
        co::spawn(co::tween(300, [this](float k) { introAlpha_ = 1 - k; }));
      }
    }
    co_await co::frame();
  }
  if (!skipPressed_) co_await co::sleep(1500);
  skipVisible_ = false;
  float a0 = introAlpha_;
  co_await co::tween(300, [this, a0](float k) { introAlpha_ = a0 * (1 - k); });
  introText_.clear();
  audio::stopMusic(1500);
  co_await moonExpression(1);
}

// ------------------------------------------------------------------ tutorial (cq)

co::Task<> GameScene::tutorial() {
  CampaignCfg cfg = campaignFor(assets_.data());
  const rules::BoardDef* b = assets_.board(cfg.tutorialBoard);
  if (!b) co_return;
  tutorialMode_ = true;
  game_ = std::make_unique<rules::Game>(*b);
  deck_ = std::make_unique<rules::Deck>(rng_);
  hand_[0].clear();
  hand_[1].clear();
  cards_.clear();
  connType_.clear();
  shownScore_[0] = shownScore_[1] = 0;
  layout_.fitBoard(*b);
  levelActive_ = true;
  showHud_ = true;
  tileAlpha_ = 0;
  skipPressed_ = false;
  skipVisible_ = true;
  audio::playMusic(assets_.snd(S_MUSIC_TUTORIAL), 750);

  auto tileIndex = [this](const std::string& id) {
    for (int i = 0; i < game_->tileCount(); i++)
      if (game_->def().tiles[i].id == id) return i;
    return 0;
  };
  auto phase = [](const char* p) {
    for (int v = 0; v < 8; v++)
      if (std::string(rules::phaseId(v)) == p) return v;
    return 0;
  };
  // Up(): a Lua coloca uma carta diretamente num slot
  auto moonPlaces = [this, tileIndex, phase](const char* ph, const char* tid) -> co::Task<> {
    int id = game_->newCard(phase(ph));
    CardVis c;
    c.id = id;
    c.pos = layout_.board(270, -100);
    c.size = boardCardSize();
    c.flip = 180;
    c.alpha = 1;
    cards_[id] = c;
    hand_[1].push_back(id);
    co_await executeMove(1, Move{id, tileIndex(tid), -1});
  };
  // Wp(): o jogador recebe uma carta e só pode jogá-la num slot
  auto playerPlaces = [this, tileIndex, phase](const char* ph, const char* tid, const char* textKey,
                                               float ms) -> co::Task<> {
    int id = game_->newCard(phase(ph));
    CardVis c;
    c.id = id;
    c.pos = layout_.deck[0];
    c.size = layout_.handCard[0];
    c.flip = 180;
    c.alpha = 0;
    cards_[id] = c;
    hand_[0].push_back(id);
    co_await co::tween(100, [this, id](float k) { vis(id).alpha = k; });
    sfx(S_DEAL, 200);
    co_await layoutHand(0);
    int target = tileIndex(tid);
    Move mv;
    for (;;) {
      in_ = Input();
      in_.active = true;
      in_.selected = id;
      in_.tileEligible = [target](int t) { return t == target; };
      glowTiles_ = {target};
      while (!in_.done && !skipPressed_) {
        vis(id).glow = 1;
        co_await co::frame();
      }
      in_.active = false;
      glowTiles_.clear();
      vis(id).glow = 0;
      if (skipPressed_) co_return;
      mv = in_.result;
      if (mv.card >= 0) break;
    }
    std::vector<co::Task<>> t;
    t.push_back(executeMove(0, mv));
    t.push_back(dialogFor(assets_.t(textKey), ms));
    co_await co::all(std::move(t));
  };

  // Esperas que terminam logo se o jogador tocar em "Skip".
  if (dbg::autoplay) skipPressed_ = true;
  auto wait = [this](float ms) {
    float end = sched_.now() + ms;
    return co::until([this, end] { return skipPressed_ || sched_.now() >= end; });
  };
  auto say = [this, wait](const char* key, float ms) -> co::Task<> {
    if (skipPressed_) co_return;
    co_await showDialog(assets_.t(key));
    if (ms > 0) co_await wait(ms);
  };
  auto both = [](co::Task<> a, co::Task<> b) -> co::Task<> {
    std::vector<co::Task<>> t;
    t.push_back(std::move(a));
    t.push_back(std::move(b));
    co_await co::all(std::move(t));
  };

  co_await showBoard();
  co_await say("tutorial_01", 7000);
  if (!skipPressed_) co_await both(moonPlaces("waxing_crescent", "0"), say("tutorial_02", 0));
  if (!skipPressed_) co_await playerPlaces("waxing_crescent", "1", "tutorial_03", 4000);
  co_await say("tutorial_04", 4000);
  if (!skipPressed_) co_await both(moonPlaces("third_quarter", "3"), say("tutorial_05", 0));
  if (!skipPressed_) co_await playerPlaces("first_quarter", "4", "tutorial_06", 4000);
  co_await say("tutorial_07", 5000);
  if (!skipPressed_) co_await both(moonPlaces("new_moon", "6"), moonPlaces("third_quarter", "8"));
  co_await say("tutorial_08", 0);
  if (!skipPressed_) co_await playerPlaces("waning_crescent", "7", "tutorial_09", 4000);
  co_await say("tutorial_10", 3000);
  if (!skipPressed_) {
    auto later = [this, wait]() -> co::Task<> {
      co_await wait(1000);
      if (skipPressed_) co_return;
      co_await showDialog(assets_.t("tutorial_11"));
      co_await wait(7000);
    };
    co_await both(moonPlaces("waning_gibbous", "5"), later());
  }
  if (!skipPressed_) co_await both(say("tutorial_12", 0), playerPlaces("full_moon", "2", "tutorial_13", 7000));
  co_await say("tutorial_14", 4000);
  if (!skipPressed_) co_await wait(1000);
  if (!skipPressed_) co_await both(say("tutorial_15", 5000), endBonus(0));
  if (!skipPressed_) co_await wait(1000);
  showRulesButton_ = true;
  co_await say("landingpage_rules", 7000);
  co_await hideDialog();
  skipVisible_ = false;
  showRulesButton_ = true;
  audio::stopMusic(1875);
  co_await co::tween(750, [this](float k) { sceneAlpha_ = 1 - k; tileAlpha_ = 1 - k; dialogAlpha_ *= 1 - k; });
  levelActive_ = false;
  showHud_ = false;
  dialogText_.clear();
  dialogAlpha_ = 0;
  sceneAlpha_ = 1;
  tutorialMode_ = false;
  cards_.clear();
  hand_[0].clear();
  hand_[1].clear();
  game_.reset();
}

// ------------------------------------------------------------------ campanha (up.j)

co::Task<> GameScene::campaign() {
  CampaignCfg cfg = campaignFor(assets_.data());
  auto pick = [this](const std::vector<std::string>& l) {
    std::vector<std::string> ok;
    for (auto& s : l)
      if (assets_.board(s)) ok.push_back(s);
    if (ok.empty()) return assets_.boards().begin()->first;
    return ok[std::uniform_int_distribution<size_t>(0, ok.size() - 1)(rng_)];
  };
  int seenMonths = seenMonthsAtStart_;
  // wildcards já desbloqueados (pela ordem de desbloqueio)
  wild_.clear();
  if (dbg::has("allwild"))  // testes: todos os wildcards disponíveis desde o início
    for (auto& w : assets_.wildcards())
      if (std::find(save_.unlockedWildcards.begin(), save_.unlockedWildcards.end(), w.id) == save_.unlockedWildcards.end())
        save_.unlockedWildcards.push_back(w.id);
  for (auto& id : save_.unlockedWildcards)
    for (size_t i = 0; i < assets_.wildcards().size(); i++)
      if (assets_.wildcards()[i].id == id) {
        CardVis c;
        c.wildcard = true;
        c.wildIndex = (int)i;
        c.id = -100 - (int)i;
        c.flip = 0;
        c.alpha = 0;
        wild_.push_back(c);
      }
  // por desbloquear, em ordem aleatória
  std::vector<int> locked;
  for (size_t i = 0; i < assets_.wildcards().size(); i++)
    if (std::find(save_.unlockedWildcards.begin(), save_.unlockedWildcards.end(), assets_.wildcards()[i].id) ==
        save_.unlockedWildcards.end())
      locked.push_back((int)i);
  std::shuffle(locked.begin(), locked.end(), rng_);

  if (dbg::has("reveal") && !assets_.wildcards().empty()) co_await wildcardReveal(locked.empty() ? 0 : locked[0]);
  for (;;) {
    setStars_ = -1;
    co_await progressScreen(0);
    std::vector<std::string> boards;
    if (level_ == 0) {
      std::string c = seenMonths > 0 ? cfg.firstIfSeen : cfg.first;
      if (wild_.empty()) boards = {c, cfg.second, cfg.third};
      else boards = {c, pick(cfg.l2), pick(cfg.l3)};
    } else {
      boards = {pick(cfg.l1), pick(cfg.l2), pick(cfg.l3)};
    }
    bool lost = false;
    for (int h = 0; h < (int)boards.size(); ++h) {
      const rules::BoardDef* b = assets_.board(boards[h]);
      if (!b) b = &assets_.boards().begin()->second;
      sceneAlpha_ = 0;
      co::spawn(co::tween(300, [this](float k) { sceneAlpha_ = k; }));
      int result = co_await playLevel(*b, level_);
      totalPoints_ += levelPoints_;
      if (result == 0) level_++;
      // fim do nível: desvanece o tabuleiro e a música
      audio::stopMusic(2500);
      co_await co::tween(1000, [this](float k) { tileAlpha_ = 1 - k; sceneAlpha_ = 1 - k; });
      levelActive_ = false;
      showHud_ = false;
      cards_.clear();
      hand_[0].clear();
      hand_[1].clear();
      stars_.clear();
      for (auto& w : wild_) w.alpha = 0;
      co_await progressScreen(result == 0 ? 2 : result == 2 ? 4 : 3);
      if (result == 1) {
        totalPoints_ = 0;
        level_ = 0;
        for (auto& w : wild_) {
          w.used = false;
          w.dimmed = false;
        }
        lost = true;
        break;
      }
      if (result == 2) --h;
    }
    if (lost) continue;
    if (!locked.empty()) {
      int w = locked.front();
      locked.erase(locked.begin());
      save_.unlockedWildcards.push_back(assets_.wildcards()[w].id);
      save_.save();
      CardVis c;
      c.wildcard = true;
      c.wildIndex = w;
      c.id = -100 - w;
      c.flip = 0;
      c.alpha = 0;
      wild_.push_back(c);
      gfx::trace("unlocked wildcard %s", assets_.wildcards()[w].id.c_str());
      co_await wildcardReveal(w);
      co_await progressScreen(1);
    }
  }
}

// ------------------------------------------------------------------ céu: posições 0/1/2 (Ej/Fj)

static const float kBgRot[3] = {-3.14159265f / 2, 0, 3.14159265f / 2};

static co::Task<> moveSky(GameScene* s, int pos, float ms, int face) {
  if (s->skyPos_ == pos) co_return;
  int from = s->skyPos_;
  s->skyPos_ = pos;
  co_await s->moonExpression(0);
  float r0 = s->bgRotation_, r1 = kBgRot[pos];
  float d0 = s->darkMoon_;
  co_await co::tween(ms, [s, r0, r1, pos, from, d0](float k) {
    s->bgRotation_ = r0 + (r1 - r0) * k;
    if (pos == 2 || from == 2) {
      float n = pos == 2 ? k : 1 - k;
      s->darkMoon_ = n < 0.3f ? 0 : (n - 0.3f) / 0.7f;
    }
  }, co::easeInOut);
  co_await s->moonExpression(face);
}

// ------------------------------------------------------------------ ecrã de progresso (ep)

co::Task<> GameScene::progressScreen(int mode) {
  Progress& P = prog_;
  P = Progress();
  P.active = true;
  P.mode = mode;
  int d = (mode == 3 || mode == 4) ? level_ + 1 : level_;
  auto levelTitle = [this](int n) {
    std::string s = assets_.t("leveltitle");
    auto p = s.find("%NUMBER%");
    if (p != std::string::npos) s.replace(p, 8, std::to_string(n));
    return s;
  };
  P.title = setStars_ >= 0 ? levelTitle(d) : "";
  P.nextTitle = levelTitle(level_ + 1);
  if (mode == 2) P.top = assets_.t("results_win");
  else if (mode == 3) P.top = assets_.t("lose_title");
  else if (mode == 4) P.top = assets_.t("tie_subtitle");
  if (mode == 0) P.bottom = assets_.t("progress_title");
  else if (mode == 1) P.bottom = assets_.t(wild_.size() < assets_.wildcards().size() ? "win_keeplaying_02" : "win_keeplaying_03");
  std::string cont = mode == 4 ? "try_again" : mode == 3 ? "playagain" : "continue";
  P.cont = ui::Button{{}, text::upper(assets_.t(cont))};
  P.cont.onTap = [this] { prog_.continued = true; };
  if (dbg::autoplay) P.continued = true;
  gfx::trace("progress mode %d level %d stars %d total %d", mode, level_, setStars_, totalPoints_);
  P.levelStars = levelPoints_;
  P.totalStars = totalPoints_;
  if (layout_.portrait) P.cont.rect = {163, 792, 214, 60};  // um pouco abaixo de "Total points"
  else P.cont.rect = {600, 440, 280, 60};

  static const char* emptyAnim[3] = {"progress-empty", "progress-empty#2", "progress-empty#3"};
  static const char* toFull[3] = {"progress-star1-empty-to-full", "progress-star2-empty-to-full",
                                  "progress-star3-empty-to-full"};
  static const char* full[3] = {"progress-star1-full", "progress-star2-full", "progress-star3-full"};
  static const char* toEmpty[3] = {"progress-star1-full-to-empty", "progress-star2-full-to-empty", ""};
  auto setSlot = [this](int b, const char* anim, bool loop) {
    auto& s = prog_.slots[b];
    s.anim = assets_.anim(anim);
    s.frame = 0;
    s.loop = loop;
    s.done = false;
  };
  auto still = [this](int b) {
    auto& s = prog_.slots[b];
    s.anim = nullptr;
    s.still = assets_.progressEmpty(b);
  };

  if (mode != 1) {
    static const Snd music[5] = {S_MUSIC_PROGRESS, S_MUSIC_WILDCARD, S_MUSIC_PROGRESS, S_MUSIC_LOSE, S_MUSIC_LOSE};
    audio::playMusic(assets_.snd(music[mode]), 750);
  }
  if (mode == 0 || mode == 2) sfx(S_PROGRESS);
  // Bo(): posição do céu
  co::Task<> sky = mode == 0 ? moveSky(this, 1, 1500, 1)
                 : mode == 2 ? moveSky(this, 0, 2500, 1)
                 : (mode == 3 || mode == 4) ? moveSky(this, 2, 2500, 5)
                             : co::Task<>();
  bool skyDone = false;
  co::spawn([](co::Task<> t, bool* f) -> co::Task<> {
    co_await t;
    *f = true;
  }(std::move(sky), &skyDone));

  co_await co::tween(300, [this](float k) { prog_.alpha = k; });
  if (mode == 2) co_await co::tween(300, [this](float k) { prog_.topA = k; });
  bool results = mode == 2 || mode == 3 || mode == 4;
  if (results) co::spawn(co::tween(300, [this](float k) { prog_.titleA = k; }));
  if (mode == 0) co::spawn(co::tween(300, [this](float k) { prog_.bottomA = k; }));

  // estrelas da série
  for (int b = 0; b < 3; b++) {
    bool isFull = b < setStars_;
    if (b == setStars_) {
      ripple(b);
      sfx(S_STAR_APPEAR);
    }
    if (mode == 0) {
      setSlot(b, emptyAnim[b], false);
      prog_.slots[b].alpha = 1;
      sfx(S_STAR_APPEAR, 230);
      co_await co::until([this, b] { return prog_.slots[b].done; });
    }
    if (isFull) setSlot(b, full[b], true);
    else still(b);
    if (mode != 0) co::spawn(co::tween(1000, [this, b](float k) { prog_.slots[b].alpha = k; }));
    if (b < 2) {
      prog_.lineA[b] = 1;
      if (b < setStars_ - 1) prog_.lineFill[b] = 1;
    }
    if (results && b == 0) {
      co::spawn([](GameScene* s) -> co::Task<> {
        co_await co::sleep(500);
        co_await co::tween(300, [s](float k) { s->prog_.statsA = k; });
        if (s->prog_.levelStars > 0) s->sfx(S_RESULT_COUNT);
        float a = (float)(s->prog_.totalStars - s->prog_.levelStars);
        co_await co::tween(800, [s, a](float k) {
          s->prog_.statsLevel = s->prog_.levelStars * k;
          s->prog_.statsTotal = a + s->prog_.levelStars * k;
        });
      }(this));
    }
  }
  if (mode == 0) {
    co_await co::sleep(500);
    co_await co::tween(300, [this](float k) { prog_.bottomA = 1 - k; });
    co_await co::tween(300, [this](float k) { prog_.nextA = k; });
    ripple(0);
    sfx(S_NEXT_LEVEL);
  }
  if (mode == 0 || mode == 2) {
    co_await co::sleep(1000);
    co_await co::until([&skyDone] { return skyDone; });
    // Oo(): a estrela atual enche-se
    int b = setStars_;
    if (b >= 0 && b < 3) {
      if (b > 0)
        co::spawn(co::tween(500, [this, b](float k) { prog_.lineFill[b - 1] = k; }, co::easeOut));
      co::spawn(moonExpression(4));
      ripple(b);
      static const Snd starSnd[3] = {S_STAR_FULL1, S_STAR_FULL2, S_STAR_FULL3};
      sfx(starSnd[b]);
      setSlot(b, toFull[b], false);
      co_await co::until([this, b] { return prog_.slots[b].done; });
      setSlot(b, full[b], true);
    }
    setStars_++;
    if (mode == 2) {
      co_await co::tween(300, [this](float k) { prog_.buttonsA = k; });
      co_await co::until([this] { return prog_.continued; });
      sfx(S_BUTTON);
      sfx(S_PROGRESS_CONTINUE);
      co_await co::tween(300, [this](float k) {
        prog_.buttonsA = 1 - k;
        prog_.titleA = std::min(prog_.titleA, 1 - k);
        prog_.statsA = std::min(prog_.statsA, 1 - k);
        prog_.topA = std::min(prog_.topA, 1 - k);
      });
      if (setStars_ < 3) {
        co::spawn(moveSky(this, 1, 1500, 1));
        co_await co::tween(300, [this](float k) { prog_.nextA = k; });
        ripple(setStars_);
        sfx(S_NEXT_LEVEL);
        co_await co::sleep(800);
      }
    } else {
      co::spawn(moonExpression(1));
    }
  } else if (mode == 3 || mode == 4) {
    co_await co::until([&skyDone] { return skyDone; });
    co_await co::tween(300, [this](float k) { prog_.topA = k; });
    if (mode == 3) {
      for (int b = 0; b < 3 && b < setStars_; b++) {
        if (toEmpty[b][0]) setSlot(b, toEmpty[b], false);
        else still(b);
        if (b < 2) prog_.lineFill[b] = 0;
      }
    }
    co::spawn(moonExpression(6));
    co_await co::tween(300, [this](float k) { prog_.buttonsA = k; });
    co_await co::until([this] { return prog_.continued; });
    sfx(S_BUTTON);
    co_await co::tween(300, [this](float k) {
      prog_.buttonsA = 1 - k;
      prog_.titleA = std::min(prog_.titleA, 1 - k);
      prog_.statsA = std::min(prog_.statsA, 1 - k);
      prog_.topA = std::min(prog_.topA, 1 - k);
    });
    co_await co::tween(1000, [this](float k) {
      for (auto& s : prog_.slots) s.alpha = std::min(s.alpha, 1 - k);
      prog_.lineA[0] = prog_.lineA[1] = 1 - k;
    });
    co_await moveSky(this, 1, 1500, 1);
    if (mode == 3) setStars_ = -1;
  } else if (mode == 1) {
    prog_.wildShown.clear();
    for (auto& w : wild_) prog_.wildShown.push_back(w.wildIndex);
    co_await co::tween(300, [this](float k) { prog_.wildA = k; });
    sfx(S_WILDCARD_PLAY);
    co_await co::tween(300, [this](float k) { prog_.bottomA = k; });
    co_await co::tween(300, [this](float k) { prog_.buttonsA = k; });
    co_await co::until([this] { return prog_.continued; });
    sfx(S_BUTTON);
    sfx(S_PROGRESS_CONTINUE);
    co::spawn(moveSky(this, 1, 1500, 1));
    setStars_ = -1;
  }
  audio::stopMusic(1875);
  co_await co::tween(750, [this](float k) { prog_.alpha = 1 - k; });
  P.active = false;
  co_await co::sleep(500);
}

// ------------------------------------------------------------------ revelação de um wildcard (xo)

co::Task<> GameScene::wildcardReveal(int w) {
  // Transposição de xo/qo/so: coordenadas originais (540x960); na horizontal tudo é reduzido e centrado.
  bool por = layout_.portrait;
  float k = por ? 1.f : 0.56f;
  float cx = layout_.W / 2;
  auto Y = [&](float y) { return por ? y : y * k; };
  Reveal& R = reveal_;
  co_await co::tween(750, [this](float t) { fadeWhite_ = t; });  // Mm()
  R = Reveal();
  R.active = true;
  R.moon = {cx, Y(6.5f)};
  R.moonD = 653 * 1.2f * k;
  R.card = CardVis();
  R.card.wildcard = true;
  R.card.wildIndex = w;
  R.card.pos = {cx, Y(675)};
  R.card.size = 20 * k;
  R.card.flip = 180;
  int savedState = moonState_;
  moonState_ = 0;
  co_await co::sleep(750);
  R.brightness = 1.6f;
  sfx(S_WILDCARD_REVEAL);
  audio::playMusic(assets_.snd(S_MUSIC_WILDCARD), 0, 6375);
  // qo(): 6375 ms — a Lua sobe e cresce, a carta sobe a rodar, o presente abre
  auto rise = [&]() -> co::Task<> {
    co_await co::tween(6375, [this, k, cx, Y](float t) {
      float e = co::easeInOut(t);
      reveal_.moon = {cx, Y(6.5f + (-80.5f - 6.5f) * e)};
      reveal_.moonD = 653 * (1.2f + (1.37f - 1.2f) * t) * k;
      reveal_.card.pos = {cx, Y(675 + (292 - 675) * e)};
      reveal_.card.rot = 180.f * t;
      reveal_.giftFrame = 0;  // a animação do presente está parada até a carta chegar à Lua
    });
  };
  auto flash = [&]() -> co::Task<> {
    co_await co::sleep(6225);
    co_await co::tween(150, [this](float t) { reveal_.brightness = 1 + 0.6f * t; });
  };
  {
    std::vector<co::Task<>> t;
    t.push_back(co::tween(750, [this](float t) { fadeWhite_ = 1 - t; }));  // Om()
    t.push_back([](Reveal* r) -> co::Task<> {
      co_await co::sleep(400);
      co_await co::tween(500, [r](float t) { r->brightness = 1.6f - 0.6f * t; });
    }(&R));
    t.push_back(rise());
    t.push_back(flash());
    co_await co::all(std::move(t));
  }
  // so(): a carta vira-se para a frente e cresce até 200 px, com uma onda
  {
    Ripple rp;
    rp.pos = R.card.pos;
    rp.offset = {0, Y(271) - Y(292)};
    ripples_.push_back(rp);
    std::vector<co::Task<>> t;
    t.push_back(co::tween(500, [this](float t) { reveal_.brightness = 1.6f - 0.6f * t; }));
    // to(): só agora a Lua "abre" o presente (24 fps)
    if (const Anim* gift = assets_.anim("moon-light-moon-wildcard-gift")) {
      float n = (float)gift->frames.size();
      t.push_back(co::tween(n * 1000.f / 24.f, [this, n](float t) { reveal_.giftFrame = t * (n - 1); }));
    }
    t.push_back(co::tween(1800, [this, k, Y](float t) {
      float e = co::easeOut(t);
      reveal_.card.rot = 180.f * (1 - e);
      reveal_.card.flip = 180.f * (1 - e);
      reveal_.card.size = (20 + 180 * e) * k;
      reveal_.card.pos.y = Y(292 + (271 - 292) * e);
    }));
    co_await co::all(std::move(t));
  }
  bool cont = co_await wildcardModal(w, true);
  (void)cont;
  sfx(S_BUTTON);
  co_await co::tween(750, [this](float t) { fadeWhite_ = t; });
  R.active = false;
  moonState_ = savedState;
  co_await co::sleep(750);
  co_await co::tween(750, [this](float t) { fadeWhite_ = 1 - t; });
}

void GameScene::renderReveal() {
  const Reveal& R = reveal_;
  if (!R.active) return;
  Frame light = assets_.moonLight();
  const Anim* gift = assets_.anim("moon-light-moon-wildcard-gift");
  float n = R.moonD / 653.f;  // escala do canvas da Lua (N)
  for (int pass = 0; pass < 2; pass++) {
    if (pass == 1) {
      if (R.brightness <= 1.01f) break;
      gfx::setAdditive(true);
    }
    float a = pass == 0 ? 1.f : (R.brightness - 1);
    // metade iluminada rodada -90° (fica virada para baixo)
    assets_.drawFrame(light, R.moon.x, R.moon.y, R.moonD, R.moonD, a, -90);
    if (gift && !gift->frames.empty()) {
      int fi = std::min((int)R.giftFrame, (int)gift->frames.size() - 1);
      const Frame& f = gift->frames[fi];
      assets_.drawFrame(f, R.moon.x, R.moon.y + 80 * n, f.w * n, f.h * n, a);
    }
    CardVis c = R.card;
    c.alpha = a;
    renderCard(c);
    gfx::setAdditive(false);
  }
}

// ------------------------------------------------------------------ desenho do progresso

Vec2 GameScene::progressSlotPos(int b) const {
  static const float Go[3] = {606, 453, 300};
  if (layout_.portrait) return {270, Go[b]};
  return {300, 470 - b * 140.f};
}

// Fo()/ej(): onda que parte da estrela b da série.
void GameScene::ripple(int b) {
  Ripple r;
  r.pos = progressSlotPos(std::max(0, std::min(2, b)));
  ripples_.push_back(r);
}

void GameScene::renderProgress() {
  const Theme& th = assets_.theme();
  Progress& P = prog_;
  float A = P.alpha;
  bool por = layout_.portrait;
  // posições das 3 estrelas (de baixo para cima no original)
  auto slotPos = [&](int b) -> Vec2 { return progressSlotPos(b); };
  float sc = por ? 1.f : 0.62f;
  auto& f = app::font(FontKind::Light);
  // linhas entre estrelas
  for (int b = 0; b < 2; b++) {
    if (P.lineA[b] <= 0) continue;
    Vec2 p = slotPos(b), q = slotPos(b + 1);
    float y0 = p.y - (b == 0 ? 25 : 35) * sc, y1 = q.y + 18 * sc;
    gfx::line(p.x, y0, q.x, y1, 1, th.lineEmpty.withAlpha(A * P.lineA[b]));
    if (P.lineFill[b] > 0)
      gfx::line(p.x, y0, q.x, y0 + (y1 - y0) * P.lineFill[b], 2, Color(255, 255, 255, (uint8_t)(255 * A * P.lineA[b])));
  }
  for (int b = 0; b < 3; b++) {
    auto& s = P.slots[b];
    if (s.alpha <= 0) continue;
    Vec2 p = slotPos(b);
    Frame fr = s.still;
    if (s.anim && !s.anim->frames.empty())
      fr = s.anim->frames[std::min((size_t)s.frame, s.anim->frames.size() - 1)];
    if (fr.valid()) assets_.drawFrame(fr, p.x, p.y, fr.w * sc, fr.h * sc, A * s.alpha);
  }
  // wildcards desbloqueados (modo 1)
  if (P.mode == 1 && P.wildA > 0) {
    static const Vec2 slotsPor[4] = {{70, 120}, {130, 200}, {470, 120}, {410, 200}};
    for (int i = 0; i < 4; i++) {
      Vec2 p = por ? slotsPor[i] : Vec2{560.f + i * 90.f, 200.f};
      float h = 27 * (por ? 1 : 0.9f);
      Color c = th.lineEmpty.withAlpha(A * P.wildA);
      gfx::line(p.x - h, p.y, p.x, p.y - h, 1.5f, c);
      gfx::line(p.x, p.y - h, p.x + h, p.y, 1.5f, c);
      gfx::line(p.x + h, p.y, p.x, p.y + h, 1.5f, c);
      gfx::line(p.x, p.y + h, p.x - h, p.y, 1.5f, c);
      if (i < (int)P.wildShown.size())
        assets_.drawFrame(assets_.wildcards()[P.wildShown[i]].front, p.x, p.y, 56, 56, A * P.wildA,
                          i < 2 ? 45.f : -45.f);
    }
  }
  // textos
  RectF topR = por ? RectF{54, 154, 432, 80} : RectF{480, 40, 440, 80};
  RectF botR = por ? RectF{27, 650, 486, 90} : RectF{480, 250, 440, 120};
  if (P.topA > 0 && !P.top.empty()) f.drawBox(P.top, topR, 32, 20, th.progressText, Align::Center, A * P.topA);
  if (P.bottomA > 0 && !P.bottom.empty())
    f.drawBox(P.bottom, botR, 32, 20, th.progressText, Align::Center, A * P.bottomA);
  if (P.nextA > 0) f.drawBox(P.nextTitle, botR, 32, 20, th.progressText, Align::Center, A * P.nextA);
  if (P.titleA > 0 && !P.title.empty()) {
    RectF r = por ? RectF{0, 650, 540, 40} : RectF{480, 150, 440, 40};
    f.drawBox(P.title, r, 28, 18, th.progressText, Align::Center, A * P.titleA);
  }
  if (P.statsA > 0) {
    float y = por ? 700 : 200;
    float cx = por ? 270 : 700;
    std::string l1 = assets_.t("results_matchstars"), l2 = assets_.t("results_totalstars");
    Color c = th.progressText.withAlpha(A * P.statsA);
    f.draw(l1, cx - 6, y, 24, c, Align::Right);
    f.draw(l2, cx - 6, y + 34, 24, c, Align::Right);
    auto& bf = app::font(FontKind::Bold);
    bf.draw(std::to_string((int)P.statsLevel), cx + 6, y, 24, Color(255, 255, 255, (uint8_t)(255 * A * P.statsA)));
    bf.draw(std::to_string((int)P.statsTotal), cx + 6, y + 34, 24, Color(255, 255, 255, (uint8_t)(255 * A * P.statsA)));
  }
  if (P.buttonsA > 0) {
    ui::Button b = P.cont;
    b.enabled = true;
    RectF r = b.pressed ? b.rect.inset(3) : b.rect;
    gfx::fillRect(r, th.button.withAlpha(A * P.buttonsA));
    if (auto* t = assets_.tex("button_border.png")) gfx::nineSlice(t, r, 4, 4, A * P.buttonsA);
    app::font(FontKind::Bold).drawBox(b.label, r.inset(8), 30, 18, Color(255, 255, 255), Align::Center, A * P.buttonsA);
  }
}

// ------------------------------------------------------------------ regras

void GameScene::renderRules() {
  const Theme& th = assets_.theme();
  float a = rulesAlpha_;
  float W = layout_.W, H = layout_.H;
  gfx::fillRect({0, 0, W, H}, Color(0, 0, 0, (uint8_t)(178 * a)));
  auto& f = app::font(FontKind::Light);
  auto& b = app::font(FontKind::Regular);
  Color white(255, 255, 255);
  auto graphic = [&](const char* k, float cx, float cy, float s) {
    Frame fr = assets_.rules(k);
    if (fr.valid()) assets_.drawFrame(fr, cx, cy, fr.w * s, fr.h * s, a);
  };
  if (layout_.portrait) {
    RectF p{27.5f, 80, 485, 800};
    gfx::verticalGradient(p, th.rulesTop.withAlpha(a), th.rulesBottom.withAlpha(a));
    if (auto* t = assets_.tex("border.png")) gfx::nineSlice(t, p.inset(-10), 30, 24, a);
    b.draw(text::upper(assets_.t("rules_title")), W / 2, p.y + 30, 38, white, Align::Center, a);
    f.drawBox(assets_.t("rules_instruction1") + "\n\n" + assets_.t("rules_instruction2"), {p.x + 10, p.y + 80, 465, 120},
              20, 14, white, Align::Center, a);
    RectF m{p.x + 10, p.y + 215, 465, 345};
    gfx::fillRoundRect(m, 12, th.rulesInner.withAlpha(a));
    b.draw(text::upper(assets_.t("rules_match")), W / 2, m.y + 15, 26, white, Align::Center, a);
    graphic("phase", m.x + m.w * 0.25f, m.y + 90, 1);
    graphic("fullmoon", m.x + m.w * 0.75f, m.y + 90, 1);
    f.drawBox(assets_.t("rules_phasepair_title"), {m.x, m.y + 125, m.w / 2, 26}, 20, 14, white, Align::Center, a);
    f.drawBox(assets_.t("rules_phasepair_points"), {m.x, m.y + 151, m.w / 2, 26}, 20, 14, th.rulesPoints, Align::Center, a);
    f.drawBox(assets_.t("rules_fullmoon_title"), {m.x + m.w / 2, m.y + 125, m.w / 2, 26}, 20, 14, white, Align::Center, a);
    f.drawBox(assets_.t("rules_fullmoon_points"), {m.x + m.w / 2, m.y + 151, m.w / 2, 26}, 20, 14, th.rulesPoints,
              Align::Center, a);
    graphic("cycle", W / 2, m.y + 250, 1);
    f.drawBox(assets_.t("rules_cycle_title"), {m.x, m.y + 290, m.w, 26}, 20, 14, white, Align::Center, a);
    f.drawBox(assets_.t("rules_cycle_points"), {m.x, m.y + 314, m.w, 26}, 20, 14, th.rulesPoints, Align::Center, a);
    b.draw(text::upper(assets_.t("bonus_title")), W / 2, p.y + 590, 26, white, Align::Center, a);
    graphic("bonus_light", W / 2 - 70, p.y + 660, 1);
    graphic("bonus_dark", W / 2 + 70, p.y + 660, 1);
    f.drawBox(assets_.t("bonus_instruction"), {p.x + 10, p.y + 700, 465, 60}, 18, 12, white, Align::Center, a);
  } else {
    RectF p{40, 30, 880, 484};
    gfx::verticalGradient(p, th.rulesTop.withAlpha(a), th.rulesBottom.withAlpha(a));
    if (auto* t = assets_.tex("border.png")) gfx::nineSlice(t, p.inset(-8), 30, 20, a);
    b.draw(text::upper(assets_.t("rules_title")), W / 2, p.y + 16, 32, white, Align::Center, a);
    f.drawBox(assets_.t("rules_instruction1") + "\n\n" + assets_.t("rules_instruction2"), {p.x + 20, p.y + 70, 380, 200},
              20, 14, white, Align::Center, a);
    b.draw(text::upper(assets_.t("bonus_title")), p.x + 210, p.y + 300, 24, white, Align::Center, a);
    graphic("bonus_light", p.x + 150, p.y + 365, 0.9f);
    graphic("bonus_dark", p.x + 270, p.y + 365, 0.9f);
    f.drawBox(assets_.t("bonus_instruction"), {p.x + 20, p.y + 400, 380, 60}, 18, 12, white, Align::Center, a);
    RectF m{p.x + 420, p.y + 70, 440, 395};
    gfx::fillRoundRect(m, 12, th.rulesInner.withAlpha(a));
    b.draw(text::upper(assets_.t("rules_match")), m.cx(), m.y + 14, 24, white, Align::Center, a);
    graphic("phase", m.x + m.w * 0.25f, m.y + 90, 0.95f);
    graphic("fullmoon", m.x + m.w * 0.75f, m.y + 90, 0.95f);
    f.drawBox(assets_.t("rules_phasepair_title"), {m.x, m.y + 125, m.w / 2, 26}, 20, 14, white, Align::Center, a);
    f.drawBox(assets_.t("rules_phasepair_points"), {m.x, m.y + 151, m.w / 2, 26}, 20, 14, th.rulesPoints, Align::Center, a);
    f.drawBox(assets_.t("rules_fullmoon_title"), {m.x + m.w / 2, m.y + 125, m.w / 2, 26}, 20, 14, white, Align::Center, a);
    f.drawBox(assets_.t("rules_fullmoon_points"), {m.x + m.w / 2, m.y + 151, m.w / 2, 26}, 20, 14, th.rulesPoints,
              Align::Center, a);
    graphic("cycle", m.cx(), m.y + 260, 0.95f);
    f.drawBox(assets_.t("rules_cycle_title"), {m.x, m.y + 330, m.w, 26}, 20, 14, white, Align::Center, a);
    f.drawBox(assets_.t("rules_cycle_points"), {m.x, m.y + 356, m.w, 26}, 20, 14, th.rulesPoints, Align::Center, a);
  }
  // fechar (X)
  float cx = layout_.portrait ? 482 : 890, cy = layout_.portrait ? 110 : 56;
  gfx::line(cx - 14, cy - 14, cx + 14, cy + 14, 2, Color(255, 255, 255, (uint8_t)(255 * a)));
  gfx::line(cx + 14, cy - 14, cx - 14, cy + 14, 2, Color(255, 255, 255, (uint8_t)(255 * a)));
}

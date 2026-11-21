// Cena de jogo: estado visual, layout (vertical/horizontal), desenho e toque.
#include "game_scene.h"

#include <algorithm>
#include <cmath>

#include "i18n.h"
#include "platform.h"
#include "scenes.h"

using rules::Owner;

// ================================================================== Layout

Layout Layout::make(bool portrait) {
  Layout L;
  L.portrait = portrait;
  if (portrait) {
    // Coordenadas originais do doodle (540x960).
    L.W = 540;
    L.H = 960;
    L.deck[0] = {640, 745};
    L.deck[1] = {640, 233};
    L.scoreStar[0] = {42, 679};
    L.scoreAlign[0] = Align::Left;
    L.scoreStar[1] = {498, 303};
    L.scoreAlign[1] = Align::Right;
    L.dialog = {54, 163, 432, 110};
    L.mute = {18, 18, 52, 52};
    L.rules = {26, 876, 48, 48};
    L.quit = {470, 18, 52, 52};
  } else {
    L.W = 960;
    L.H = 544;
    L.moonPivot = {125, 6};
    L.moonScale = 0.62f;
    L.handX[0] = 814;
    L.handX[1] = 140;
    L.handY[0] = 170;
    L.handY[1] = 250;
    L.handCard[0] = 66;
    L.handCard[1] = 46;
    L.handGap = 10;
    L.wildY = 300;
    L.wildSize = 62;
    L.wildCols = 2;
    L.wildGap = 16;
    L.deck[0] = {1060, 175};
    L.deck[1] = {-100, 250};
    L.scoreStar[0] = {770, 72};
    L.scoreAlign[0] = Align::Left;
    L.scoreStar[1] = {72, 320};
    L.scoreAlign[1] = Align::Left;
    L.dialog = {262, 40, 436, 70};
    L.dialogFont = 24;
    L.mute = {28, 470, 50, 50};
    L.rules = {92, 470, 50, 50};
    L.quit = {884, 26, 50, 50};
    L.monthY = 486;
    L.monthFont = 22;
    L.frameBorder = 30;
  }
  return L;
}

void Layout::fitBoard(const rules::BoardDef& b) {
  if (portrait) {
    bScale = 1;
    bOx = bOy = 0;
    return;
  }
  float minx = 1e9, miny = 1e9, maxx = -1e9, maxy = -1e9;
  for (auto& t : b.tiles) {
    minx = std::min(minx, t.x);
    maxx = std::max(maxx, t.x);
    miny = std::min(miny, t.y);
    maxy = std::max(maxy, t.y);
  }
  float pad = b.cardExpandedSize * 0.5f + 6;
  minx -= pad; maxx += pad; miny -= pad; maxy += pad;
  RectF area{258, 112, 444, 410};
  bScale = std::min(1.f, std::min(area.w / (maxx - minx), area.h / (maxy - miny)));
  bOx = area.cx() - (minx + maxx) * 0.5f * bScale;
  bOy = area.cy() - (miny + maxy) * 0.5f * bScale;
}

std::vector<Vec2> Layout::handPositions(int owner, const std::vector<float>& sizes) const {
  // nl(): centrado em handX, espaçamento fixo.
  std::vector<Vec2> r;
  float total = (sizes.size() > 0 ? (sizes.size() - 1) * handGap : 0);
  for (float s : sizes) total += s;
  float x = handX[owner] - total / 2;
  for (float s : sizes) {
    r.push_back({x + s / 2, handY[owner]});
    x += s + handGap;
  }
  return r;
}

Vec2 Layout::wildPos(int i, int n) const {
  int cols = std::min(wildCols, std::max(1, n));
  int row = i / wildCols, col = i % wildCols;
  int inRow = std::min(cols, n - row * wildCols);
  float total = inRow * wildSize + (inRow - 1) * wildGap;
  float x = handX[0] - total / 2 + wildSize / 2 + col * (wildSize + wildGap);
  return {x, wildY + row * (wildSize + wildGap)};
}

// ================================================================== Cena

GameScene::GameScene(const GameDef& g) : def_(g), rng_(std::random_device{}()) {
  layout_ = Layout::make(app::settings().orientation != Orientation::Landscape);
  save_.load(g.id);
  // O original conta os tutoriais vistos quando a página carrega (antes do tutorial deste mês),
  // por isso na primeira vez o 1.º nível é sempre o tabuleiro 3x3.
  seenMonthsAtStart_ = Save::monthsWithTutorialSeen();
  muteBtn_.rect = layout_.mute;
  muteBtn_.onTap = [this] {
    app::settings().muted = !app::settings().muted;
    audio::setMuted(app::settings().muted);
    app::settings().save();
  };
  rulesBtn_.rect = layout_.rules;
  rulesBtn_.onTap = [this] {
    if (!showRulesButton_) return;
    rulesOpen_ = true;
    sfx(S_RULES_OPEN);
  };
  quitBtn_.rect = layout_.quit;
  quitBtn_.onTap = [this] { quitConfirm_ = true; };
  hudButtons_.buttons = {&muteBtn_, &rulesBtn_, &quitBtn_};
  float cx = layout_.W / 2, cy = layout_.H / 2;
  quitYes_ = ui::Button{{cx - 190, cy + 20, 180, 60}, i18n::tr("quit")};
  quitNo_ = ui::Button{{cx + 10, cy + 20, 180, 60}, i18n::tr("keep_playing")};
  quitYes_.onTap = [this] {
    audio::stopAll();
    app::setScene(makeMenuScene());
  };
  quitNo_.onTap = [this] { quitConfirm_ = false; };
  // .ddl-skip-button: right 20 + margin 8, bottom 50
  skipBtn_.rect = layout_.portrait ? RectF{403, 852, 109, 54} : RectF{790, 448, 120, 54};
  skipBtn_.onTap = [this] { skipPressed_ = true; };

  co::Sched* prev = co::current();
  co::current() = &sched_;
  sched_.spawn(mainFlow());
  co::current() = prev;
}

GameScene::~GameScene() {
  sched_.clear();
  video_.close();
  audio::unloadGameAudio();
}

void GameScene::back() {
  if (rulesOpen_) {
    rulesOpen_ = false;
    sfx(S_RULES_CLOSE);
  } else {
    quitConfirm_ = !quitConfirm_;
  }
}

void GameScene::update(float dt) {
  time_ += dt;
  spinner_ += dt;
  if (videoActive_) video_.update();
  sched_.tick(dt);
  // animação da lua (24 fps)
  if (moonAnim_ && !moonAnimDone_) {
    moonFrame_ += dt * 24.f / 1000.f;
    int n = moonAnimEnd_ - moonAnimStart_;
    if (moonFrame_ >= n) {
      moonFrame_ = (float)(n - 1);
      moonAnimDone_ = true;
    }
  }
  for (auto& s : prog_.slots)
    if (s.anim && !s.done) {
      s.frame += dt * 24.f / 1000.f;
      if (s.frame >= s.anim->frames.size()) {
        if (s.loop) s.frame = std::fmod(s.frame, (float)s.anim->frames.size());
        else {
          s.frame = (float)s.anim->frames.size() - 1;
          s.done = true;
        }
      }
    }
  for (int i = 0; i < 2; i++) scorePulse_[i] = std::max(0.f, scorePulse_[i] - dt / 500.f);
  if (game_)
    for (auto& kv : cards_) {
      if (kv.first < 0 || kv.first >= (int)game_->cards().size()) continue;
      const rules::Card& rc = game_->card(kv.first);
      CardVis& v = kv.second;
      int owner = rc.tile < 0 ? -1 : v.ownerOverride != -2 ? v.ownerOverride : rc.owner;
      if (owner != v.shownOwner) {
        v.prevOwner = v.shownOwner;
        v.shownOwner = owner;
        v.ownerT = 0;
      }
      v.ownerT = std::min(1.f, v.ownerT + dt / 250.f);
    }
  for (auto& w : wild_) w.dimT = std::max(0.f, std::min(1.f, w.dimT + (w.dimmed ? dt : -dt) / 250.f));
  for (auto& r : ripples_) r.t += dt;
  ripples_.erase(std::remove_if(ripples_.begin(), ripples_.end(), [](const Ripple& r) { return r.t >= 3000; }),
                 ripples_.end());
  float target = rulesOpen_ ? 1.f : 0.f;
  rulesAlpha_ += (target - rulesAlpha_) * std::min(1.f, dt / 120.f);
  modal_.alpha += ((modal_.open ? 1.f : 0.f) - modal_.alpha) * std::min(1.f, dt / 120.f);
}

Vec2 GameScene::tilePos(int tile) const {
  auto& t = game_->def().tiles[tile];
  return layout_.board(t.x, t.y);
}

CardVis& GameScene::vis(int id) { return cards_[id]; }

CardVis* GameScene::wildVis(int w) {
  for (auto& c : wild_)
    if (c.wildIndex == w) return &c;
  return nullptr;
}

void GameScene::clearConnections(int tile) {
  for (auto it = connType_.begin(); it != connType_.end();) {
    if (it->first.first == tile || it->first.second == tile) it = connType_.erase(it);
    else ++it;
  }
}

int GameScene::phaseSnd(int v, bool player, bool star) const {
  // vl/um: crescente e minguante partilham o mesmo som.
  static const int idx[8] = {0, 1, 2, 3, 4, 3, 2, 1};
  int i = idx[v & 7];
  if (star) return (player ? S_PLAYER_STAR_NEW : S_MOON_STAR_NEW) + i;
  return (player ? S_PLAYER_PHASE_NEW : S_MOON_PHASE_NEW) + i;
}

// ================================================================== Toque

static bool cardHit(const CardVis& c, float x, float y, float pad = 10) {
  float h = c.size * 0.5f + pad;
  return std::fabs(x - c.pos.x) <= h && std::fabs(y - c.pos.y) <= h;
}

void GameScene::touch(const Touch& t) {
  if (quitConfirm_) {
    ui::ButtonSet s;
    s.buttons = {&quitYes_, &quitNo_};
    static ui::ButtonSet keep;
    keep.buttons = s.buttons;
    keep.touch(t);
    return;
  }
  if (rulesOpen_) {
    if (t.kind == Touch::Up) {
      rulesOpen_ = false;
      sfx(S_RULES_CLOSE);
    }
    return;
  }
  if (modal_.open) {
    static ui::ButtonSet ms;
    ms.buttons = {&modal_.a};
    if (!modal_.info) ms.buttons.push_back(&modal_.b);
    if (t.kind == Touch::Down) modal_.touchStarted = true;
    // Tocar fora da janela cancela, mas só se o toque começou com a janela já aberta
    // (o dedo que abriu a janela ao tocar no wildcard não a pode fechar ao ser levantado).
    if (!ms.touch(t) && t.kind == Touch::Up && !modal_.info && modal_.touchStarted) modal_.result = 0;
    return;
  }
  if (prog_.active && prog_.buttonsA > 0.5f) {
    static ui::ButtonSet ps;
    ps.buttons = {&prog_.cont};
    if (ps.touch(t)) return;
  }
  if (skipVisible_) {
    static ui::ButtonSet ss;
    ss.buttons = {&skipBtn_};
    if (ss.touch(t)) return;
  }
  if (!loadingScreen_ && hudButtons_.touch(t)) return;

  // ---- escolher uma carta da mão da Lua (Touro)
  if (in_.pickHand) {
    if (t.kind == Touch::Up)
      for (int id : hand_[1])
        if (cardHit(vis(id), t.x, t.y, 14)) {
          in_.pickedCard = id;
          return;
        }
    return;
  }
  // ---- escolher uma carta no tabuleiro (efeitos de wildcards)
  if (in_.pickMode) {
    if (t.kind == Touch::Up) {
      for (int i = 0; i < game_->tileCount(); i++) {
        Vec2 p = tilePos(i);
        float h = boardCardSize() * 0.5f + 8;
        if (std::fabs(t.x - p.x) <= h && std::fabs(t.y - p.y) <= h && in_.pickEligible(i)) {
          in_.picked = i;
          return;
        }
      }
    }
    return;
  }
  if (!in_.active) return;

  auto tileAt = [&](float x, float y) {
    int best = -1;
    float bd = 1e9;
    for (int i = 0; i < game_->tileCount(); i++) {
      if (!in_.tileEligible(i)) continue;
      Vec2 p = tilePos(i);
      float h = std::max(boardCardSize(), 50.f * layout_.bScale) * 0.5f + 10;
      float dx = std::fabs(x - p.x), dy = std::fabs(y - p.y);
      if (dx <= h && dy <= h && dx + dy < bd) {
        bd = dx + dy;
        best = i;
      }
    }
    return best;
  };

  if (t.kind == Touch::Down) {
    // carta da mão?
    for (int id : hand_[0]) {
      CardVis& c = vis(id);
      if (cardHit(c, t.x, t.y)) {
        sfx(S_PICKUP);
        in_.selected = id;
        in_.dragCard = id;
        in_.touchId = t.id;
        in_.dragOffset = {c.pos.x - t.x, c.pos.y - t.y};
        in_.dragStart = c.pos;
        in_.snapTile = -1;
        c.dragging = true;
        return;
      }
    }
    // wildcard? (não se arrasta: escolhe-se com um toque, ao levantar o dedo)
    for (auto& w : wild_) {
      if (!w.used && w.alpha > 0.5f && cardHit(w, t.x, t.y)) {
        in_.wildDown = w.wildIndex;
        in_.touchId = t.id;
        return;
      }
    }
  } else if (t.kind == Touch::Move && in_.dragCard >= 0 && t.id == in_.touchId) {
    CardVis& c = vis(in_.dragCard);
    Vec2 p{t.x + in_.dragOffset.x, t.y + in_.dragOffset.y};
    int tile = tileAt(t.x, t.y);
    if (tile >= 0) {
      if (tile != in_.snapTile) sfx(S_SNAP);
      c.pos = tilePos(tile);
    } else {
      c.pos = p;
    }
    in_.snapTile = tile;
  } else if (t.kind == Touch::Up) {
    if (in_.wildDown >= 0) {
      int wi = in_.wildDown;
      in_.wildDown = -1;
      CardVis* w = wildVis(wi);
      if (w && !w->used && cardHit(*w, t.x, t.y)) {
        sfx(S_WILDCARD_SELECT);
        in_.result = Move{-1, -1, wi};
        in_.done = true;
      }
      return;
    }
    if (in_.dragCard >= 0 && t.id == in_.touchId) {
      CardVis& c = vis(in_.dragCard);
      int tile = tileAt(t.x, t.y);
      c.dragging = false;
      int card = in_.dragCard;
      in_.dragCard = -1;
      bool moved = std::fabs(c.pos.x - in_.dragStart.x) + std::fabs(c.pos.y - in_.dragStart.y) > 12;
      if (tile >= 0 && moved) {
        in_.result = Move{card, tile, -1};
        in_.done = true;
      } else {
        // volta para a mão (fica selecionada para tocar depois num slot)
        Vec2 from = c.pos, to = in_.dragStart;
        co::Sched* prev = co::current();
        co::current() = &sched_;
        sched_.spawn(co::tween(250, [this, card, from, to](float k) { vis(card).pos = lerp(from, to, k); },
                               co::easeOut));
        co::current() = prev;
      }
      return;
    }
    // toque num slot com a carta selecionada
    int tile = tileAt(t.x, t.y);
    if (tile >= 0 && in_.selected >= 0) {
      in_.result = Move{in_.selected, tile, -1};
      in_.done = true;
    }
  }
}

// ================================================================== Desenho

void GameScene::render() {
  renderBackground();
  if (loadingScreen_ && error_.empty()) {
    renderLoading();
    renderFrame();
    return;
  }
  if (levelActive_) {
    renderBoard();
    renderCards();
  }
  if (videoActive_ && video_.hasFrame()) {
    // O vídeo é vertical (540x960): ocupa o ecrã todo na vertical; na horizontal fica centrado.
    float vh = layout_.H, vw = vh * video_.width() / std::max(1, video_.height());
    SDL_FRect d{layout_.W / 2 - vw / 2, 0, vw, vh};
    SDL_Rect src{0, 0, video_.width(), video_.height()};  // sem as colunas de alinhamento
    SDL_RenderCopyF(gfx::renderer(), video_.texture(), &src, &d);
  }
  renderHud();
  renderReveal();
  if (!introText_.empty() && introAlpha_ > 0) {
    if (layout_.portrait) {
      // posições originais (por cima do vídeo 540x960)
      app::font(FontKind::Light)
          .drawBox(introText_, {40, introY_, 460, 125}, 31, 18, assets_.theme().text, Align::Center, introAlpha_);
    } else {
      // Na horizontal o vídeo fica reduzido ao centro: o texto vai para a faixa livre por baixo do
      // tabuleiro do vídeo, sem tapar o botão "Skip" nem o nome do mês.
      app::font(FontKind::Light)
          .drawBox(introText_, {180, 400, 600, 76}, 26, 16, assets_.theme().text, Align::Center, introAlpha_);
    }
  }
  for (auto& r : ripples_) {
    float k = r.t / 3000.f, e = 1 - (1 - k) * (1 - k);  // easeOut
    float rad = 200 * e, wdt = 4 + 44 * k, a = 1 - e;
    Vec2 p{r.pos.x + r.offset.x * e, r.pos.y + r.offset.y * e};
    Color c = assets_.theme().lineEmpty;
    for (int i = 0; i < 3; i++)
      gfx::strokeCircle(p.x, p.y, rad, wdt * (1 + i * 0.35f), c.withAlpha(a * (i == 0 ? 0.55f : 0.18f)));
  }
  if (prog_.active) renderProgress();
  renderDialog();
  for (auto& s : stars_) {
    float sz = s.size * s.scale;
    gfx::star(s.pos.x, s.pos.y, sz, s.light ? Color(255, 255, 255, (uint8_t)(255 * s.alpha))
                                            : Color(0, 0, 0, (uint8_t)(255 * s.alpha)),
              (s.light ? assets_.theme().starWhiteStroke : assets_.theme().starBlackStroke).withAlpha(s.alpha));
  }
  if (skipVisible_) {
    RectF r = skipBtn_.pressed ? skipBtn_.rect.inset(3) : skipBtn_.rect;
    gfx::fillRect(r, (skipBtn_.pressed ? assets_.theme().buttonHi : assets_.theme().button));
    if (auto* t = assets_.tex("button_border.png")) gfx::nineSlice(t, {r.x, r.y - 3, r.w, r.h + 6}, 5, 5);
    app::font(FontKind::Light).drawBox(skipBtn_.label, r.inset(6), 24, 16, assets_.theme().text);
  }
  renderMonth();  // por baixo da janela do wildcard (que na horizontal chega a essa zona)
  renderModal();
  if (fadeWhite_ > 0) gfx::fillRect({0, 0, layout_.W, layout_.H}, Color(255, 255, 255, (uint8_t)(255 * fadeWhite_)));
  renderFrame();
  if (rulesAlpha_ > 0.01f) renderRules();
  if (quitConfirm_) renderQuitConfirm();
  if (!error_.empty()) {
    gfx::fillRect({0, 0, layout_.W, layout_.H}, Color(0, 0, 0, 200));
    app::font(FontKind::Regular).drawBox(error_, {40, 0, layout_.W - 80, layout_.H}, 26, 14, Color(255, 255, 255));
  }
}

void GameScene::renderBackground() {
  const Theme& th = assets_.theme();
  float W = layout_.W, H = layout_.H;
  if (!loaded_) {
    gfx::fillRect({0, 0, W, H}, Color(20, 12, 22));
    return;
  }
  // .ddl-background-inner: 1980x1280 em (-720,-265), roda em torno do ponto da lua.
  Vec2 piv = layout_.moonPivot;
  float s = layout_.portrait ? 1.f : 0.8f;
  float rot = bgRotation_;
  // Gradiente do céu. É prolongado muito para além do retângulo original (com as cores das pontas), para
  // que ao rodar no fim de cada nível nunca fique uma parte do ecrã a preto (sobretudo na horizontal).
  {
    RectF inner{piv.x - 990 * s, piv.y - 280 * s, 1980 * s, 1280 * s};
    const float ext = 4000;
    float c = std::cos(rot), sn = std::sin(rot);
    auto R = [&](float x, float y) {
      float dx = x - piv.x, dy = y - piv.y;
      return SDL_FPoint{piv.x + dx * c - dy * sn, piv.y + dx * sn + dy * c};
    };
    std::vector<std::pair<float, Color>> stops;
    stops.push_back({inner.x - ext, th.sky.front().second});
    for (auto& st : th.sky) stops.push_back({inner.x + inner.w * st.first, st.second});
    stops.push_back({inner.x + inner.w + ext, th.sky.back().second});
    float y0 = inner.y - ext, y1 = inner.y + inner.h + ext;
    std::vector<SDL_Vertex> v;
    std::vector<int> idx;
    for (size_t i = 0; i + 1 < stops.size(); i++) {
      float x0 = stops[i].first, x1 = stops[i + 1].first;
      Color a = stops[i].second, b = stops[i + 1].second;
      int base = (int)v.size();
      SDL_FPoint p[4] = {R(x0, y0), R(x1, y0), R(x1, y1), R(x0, y1)};
      Color cs[4] = {a, b, b, a};
      for (int k = 0; k < 4; k++) v.push_back({p[k], SDL_Color{cs[k].r, cs[k].g, cs[k].b, 255}, {0, 0}});
      int q[6] = {base, base + 1, base + 2, base, base + 2, base + 3};
      idx.insert(idx.end(), q, q + 6);
    }
    if (!gfx::dbgNoGeometry)
      SDL_RenderGeometry(gfx::renderer(), nullptr, v.data(), (int)v.size(), idx.data(), (int)idx.size());
  }
  // estrelas
  Frame st = assets_.stars();
  if (st.valid()) {
    float cx = piv.x + std::cos(rot) * 0 - std::sin(rot) * (640 - 280) * s;  // centro do sprite 1980x1280
    float cy = piv.y + std::sin(rot) * 0 + std::cos(rot) * (640 - 280) * s;
    assets_.drawFrame(st, cx, cy, st.w * s, st.h * s, 1.f, rot * 180.f / 3.14159265f);
  }
  // lua (canvas de 653 px desenhado a metade do tamanho)
  float ms = layout_.moonScale;
  Frame light = assets_.moonLight(), dark = assets_.moonDark();
  float moonSize = 326.5f * ms;
  // O canvas da lua está dentro do céu: roda com ele. A cara (personagem) fica sempre direita.
  float rotDeg = rot * 180.f / 3.14159265f;
  // Durante a revelação de um wildcard a Lua grande substitui a do céu (no original fica por cima dela);
  // durante o vídeo de introdução o vídeo tapa o céu (na horizontal a Lua ficaria visível ao lado).
  if (reveal_.active || videoActive_) return;
  // A lua com o lado escuro (por baixo) só aparece na derrota/empate; a metade iluminada fica sempre por cima.
  if (darkMoon_ > 0) assets_.drawFrame(dark, piv.x, piv.y, moonSize, moonSize, darkMoon_, rotDeg);
  assets_.drawFrame(light, piv.x, piv.y, moonSize, moonSize, 1.f, rotDeg);
  // personagem (expressões)
  if (moonAnim_ && !moonAnim_->frames.empty()) {
    int n = moonAnimEnd_ - moonAnimStart_;
    int i = std::max(0, std::min(n - 1, (int)moonFrame_));
    int fi = moonAnimReverse_ ? moonAnimEnd_ - 1 - i : moonAnimStart_ + i;
    fi = std::max(0, std::min((int)moonAnim_->frames.size() - 1, fi));
    const Frame& f = moonAnim_->frames[fi];
    float scale = (f.w > 300 ? 0.5f : 1.f) * ms;
    assets_.drawFrame(f, piv.x, piv.y + 40 * ms, f.w * scale, f.h * scale);
  }
}

void GameScene::renderFrame() {
  // GoogleFrame.png (1080x318, cantos de 85 px): o centro do topo ("G·O·O·G·L·E") e da base (fases da lua)
  // mantêm a proporção; o espaço que sobra é preenchido com o traço duplo.
  if (const Texture* fr = assets_.tex("GoogleFrame.png")) {
    float W = layout_.W, H = layout_.H, b = layout_.frameBorder, s = b / 85.f;
    auto blit = [&](int sx, int sy, int sw, int sh, float dx, float dy, float dw, float dh) {
      SDL_Rect r{sx, sy, sw, sh};
      gfx::draw(fr, &r, {dx, dy, dw, dh});
    };
    int tw = fr->w, th = fr->h;
    blit(0, 0, 85, 85, 0, 0, b, b);
    blit(tw - 85, 0, 85, 85, W - b, 0, b, b);
    blit(0, th - 85, 85, 85, 0, H - b, b, b);
    blit(tw - 85, th - 85, 85, 85, W - b, H - b, b, b);
    blit(0, 85, 85, th - 170, 0, b, b, H - 2 * b);
    blit(tw - 85, 85, 85, th - 170, W - b, b, b, H - 2 * b);
    float cw = std::min((float)(tw - 170) * s, W - 2 * b), x0 = (W - cw) / 2;
    for (int row = 0; row < 2; row++) {
      int sy = row == 0 ? 0 : th - 85;
      float dy = row == 0 ? 0 : H - b;
      blit(85, sy, tw - 170, 85, x0, dy, cw, b);
      if (x0 > b + 0.5f) {
        blit(100, sy, 60, 85, b, dy, x0 - b + 1, b);
        blit(tw - 160, sy, 60, 85, x0 + cw - 1, dy, W - b - (x0 + cw) + 1, b);
      }
    }
  }
}

void GameScene::renderMonth() {
  if (loaded_ && !loadingScreen_) {
    // Na horizontal a janela do wildcard chega à base do ecrã: o mês desaparece enquanto está aberta.
    float a = layout_.portrait ? 1.f : 1.f - modal_.alpha;
    app::font(FontKind::Light)
        .draw(text::upper(assets_.monthName()), layout_.W / 2, layout_.monthY, layout_.monthFont, assets_.theme().text,
              Align::Center, a);
  }
}

void GameScene::renderLoading() {
  float W = layout_.W, H = layout_.H;
  gfx::fillRect({0, 0, W, H}, Color(0, 0, 0, (uint8_t)(120 * loadingAlpha_)));
  const Anim* sp = assets_.anim("spinner");
  if (sp && !sp->frames.empty() && loaded_) {
    int i = (int)(spinner_ * 24 / 1000) % sp->frames.size();
    assets_.drawFrame(sp->frames[i], W / 2, H / 2 - 30, 150, 150, loadingAlpha_);
  }
  std::string label = loaded_ ? assets_.t("loading") : "Loading...";
  if (!audioDone_ && audioProgress_ > 0) label += "";
  app::font(FontKind::Light).draw(label, W / 2, H / 2 + 60, 24, Color::hex(0xFDBC8C), Align::Center, loadingAlpha_);
}

void GameScene::renderBoard() {
  const Theme& th = assets_.theme();
  const auto& def = game_->def();
  float a = tileAlpha_;
  // ligações
  for (int i = 0; i < (int)def.tiles.size(); i++)
    for (int n : def.neighbors[i]) {
      if (n < i) continue;
      Vec2 p = tilePos(i), q = tilePos(n);
      auto it = connType_.find({i, n});
      gfx::line(p.x, p.y, q.x, q.y, 2 * layout_.bScale, th.connection.withAlpha(a));
      if (it == connType_.end()) continue;
      float ca = it->second.second * a;
      Color w(255, 255, 255, (uint8_t)(255 * ca));
      Vec2 m{(p.x + q.x) / 2, (p.y + q.y) / 2};
      float dx = q.x - p.x, dy = q.y - p.y, len = std::sqrt(dx * dx + dy * dy);
      float nx = -dy / len, ny = dx / len;
      switch (it->second.first) {
        case rules::MatchType::Pair:
          gfx::strokeCircle(m.x + nx * 5, m.y + ny * 5, 3 * layout_.bScale + 0.5f, 1.5f, w);
          gfx::strokeCircle(m.x - nx * 5, m.y - ny * 5, 3 * layout_.bScale + 0.5f, 1.5f, w);
          break;
        case rules::MatchType::FullMoon:
          gfx::fillCircle(m.x, m.y, 5 * layout_.bScale + 0.5f, w);
          break;
        case rules::MatchType::Run:
          gfx::line(p.x, p.y, q.x, q.y, 6 * layout_.bScale, w);
          gfx::fillCircle(p.x, p.y, 3 * layout_.bScale, w);
          gfx::fillCircle(q.x, q.y, 3 * layout_.bScale, w);
          break;
      }
    }
  // slots
  Frame tf = assets_.tile();
  float ts = def.tileSize * layout_.bScale;
  for (int i = 0; i < (int)def.tiles.size(); i++) {
    Vec2 p = tilePos(i);
    assets_.drawFrame(tf, p.x, p.y, ts, ts, a);
    if (glowTiles_.count(i) || (in_.dragCard >= 0 && in_.snapTile == i)) {
      float pulse = 0.6f * (0.5f + 0.5f * std::sin(time_ / 600.f * 3.14159265f));
      float h = ts * 0.85f * 0.5f;
      Color c(255, 255, 255, (uint8_t)(255 * pulse));
      // contorno tracejado (dash 8,4)
      for (int side = 0; side < 4; side++) {
        float x0 = side == 1 ? p.x + h : p.x - h, y0 = side == 2 ? p.y + h : p.y - h;
        bool horiz = side == 0 || side == 2;
        for (float d = 0; d < 2 * h; d += 12) {
          float e = std::min(2 * h, d + 8);
          if (horiz) gfx::line(x0 + d, y0, x0 + e, y0, 3, c);
          else gfx::line(x0, y0 + d, x0, y0 + e, 3, c);
        }
      }
    }
  }
}

// Moldura da carta selecionada: o SVG "ddl-card-border" do original (retângulo arredondado com um círculo em
// cada canto), em branco, a 127% do tamanho da carta e com um leve brilho.
static void drawSelectionBorder(float cx, float cy, float w, float h, float a) {
  if (a <= 0.01f) return;
  float sx = w * 1.27f / 298.f, sy = h * 1.27f / 294.f;
  float x0 = cx - w * 1.27f / 2, y0 = cy - h * 1.27f / 2;
  RectF r{x0 + 10.5f * sx, y0 + 8.5f * sy, 277.f * sx, 277.f * sy};
  for (int i = 3; i >= 1; i--)
    gfx::strokeRoundRect(r, 26 * sx, 1.9f + i * 2.5f, Color(255, 255, 255, (uint8_t)(35 * a)));
  gfx::strokeRoundRect(r, 26 * sx, 1.6f, Color(255, 255, 255, (uint8_t)(255 * a)));
  float cr = 15 * sx;
  const float pts[4][2] = {{282, 278}, {282, 16}, {16, 278}, {16, 16}};
  for (auto& p : pts) gfx::strokeCircle(x0 + p[0] * sx, y0 + p[1] * sy, cr, 1.4f, Color(255, 255, 255, (uint8_t)(255 * a)));
}

void GameScene::renderCard(const CardVis& c) {
  if (c.alpha <= 0.01f) return;
  float flipCos = std::cos(c.flip * 3.14159265f / 180.f);
  float w = c.size * std::fabs(flipCos), h = c.size;
  bool back = flipCos < 0;
  auto faceFor = [&](int owner) {
    const rules::Card& rc = game_->card(c.id);
    if (owner == Owner::Player) return assets_.cardLight(rc.value);
    if (owner == Owner::Moon) return assets_.cardDark(rc.value);
    return assets_.cardFace(rc.value);
  };
  Color tint = c.dimmed ? Color(140, 140, 140) : Color();
  if (c.burn > 0) tint = Color(255, 255, 255);
  float sc = 1 + 0.05f * c.burn, A = c.alpha * (1 - c.burn);
  if (c.wildcard) {
    auto& wd = assets_.wildcards()[c.wildIndex];
    const Frame& fr = back ? wd.back : wd.front;
    // wildcard jogado: fica "acinzentado" (contrast(.25) do original) com uma transição de 0,25 s
    if (c.dimT < 1) assets_.drawFrame(fr, c.pos.x, c.pos.y, w * sc, h * sc, A * (1 - c.dimT), c.rot);
    if (c.dimT > 0) assets_.drawFrameDim(fr, c.pos.x, c.pos.y, w * sc, h * sc, A * c.dimT, c.rot);
  } else if (back) {
    assets_.drawFrame(assets_.cardBack(), c.pos.x, c.pos.y, w * sc, h * sc, A, c.rot, tint);
  } else {
    // .ddl-card-light/.ddl-card-dark aparecem com uma transição de opacidade de 0,25 s
    if (c.ownerT < 1 && c.prevOwner != c.shownOwner)
      assets_.drawFrame(faceFor(c.prevOwner), c.pos.x, c.pos.y, w * sc, h * sc, A * (1 - c.ownerT), c.rot, tint);
    assets_.drawFrame(faceFor(c.shownOwner), c.pos.x, c.pos.y, w * sc, h * sc, A * c.ownerT, c.rot, tint);
  }
  if (c.burn > 0)
    gfx::fillRoundRect({c.pos.x - w / 2 - 6, c.pos.y - h / 2 - 6, w + 12, h + 12}, 12,
                       Color(255, 255, 255, (uint8_t)(120 * c.burn * (1 - c.burn) * 4)));
  drawSelectionBorder(c.pos.x, c.pos.y, w, h, c.glow * c.alpha);
}

void GameScene::renderCards() {
  // ordem: tabuleiro, mão da lua, wildcards, mão do jogador, carta a ser arrastada por cima
  std::vector<const CardVis*> order;
  for (auto& kv : cards_)
    if (game_->card(kv.first).tile >= 0 && !kv.second.dragging) order.push_back(&kv.second);
  for (int id : hand_[1]) order.push_back(&cards_[id]);
  for (auto& w : wild_) order.push_back(&w);
  for (int id : hand_[0])
    if (!cards_[id].dragging) order.push_back(&cards_[id]);
  for (auto& kv : cards_)
    if (game_->card(kv.first).tile < 0 && std::find(hand_[0].begin(), hand_[0].end(), kv.first) == hand_[0].end() &&
        std::find(hand_[1].begin(), hand_[1].end(), kv.first) == hand_[1].end())
      order.push_back(&kv.second);  // cartas em trânsito (a ser destruídas, etc.)
  for (auto& kv : cards_)
    if (kv.second.dragging) order.push_back(&kv.second);
  for (auto* c : order) renderCard(*c);
}

void GameScene::renderHud() {
  if (!loaded_) return;
  const Theme& th = assets_.theme();
  auto& f = app::font(FontKind::Light);
  if (showHud_) {
    for (int o = 0; o < 2; o++) {
      Vec2 s = layout_.scoreStar[o];
      float pulse = scorePulse_[o];
      float size = 28 * (1 + 0.2f * pulse);
      gfx::star(s.x, s.y, size, o == 0 ? Color(255, 255, 255) : Color(0, 0, 0),
                o == 0 ? th.starWhiteStroke : th.starBlackStroke);
      char buf[16];
      snprintf(buf, sizeof buf, "%02d", std::max(0, shownScore_[o]));
      float px = 32 * (1 + 0.3f * pulse) * scoreScale_[o];
      bool right = layout_.scoreAlign[o] == Align::Right;
      float tx = right ? s.x - 14 - 8 - 19 : s.x + 14 + 8 + 19;
      if (scoreAlpha_[o] > 0.01f) f.draw(buf, tx, s.y - px * 0.55f, px, th.text.withAlpha(scoreAlpha_[o]), Align::Center);
      // indicador de vez (triângulo)
      if (turnAlpha_[o] > 0.01f) {
        float ax = right ? tx - 19 - 8 - 9 : tx + 19 + 8 + 9;
        Color c = (o == 0 ? th.turnPlayer : th.turnMoon).withAlpha(turnAlpha_[o]);
        if (o == 0 || !right) gfx::fillTriangle(ax + 9, s.y - 10, ax - 9, s.y, ax + 9, s.y + 10, c);
        else gfx::fillTriangle(ax - 9, s.y - 10, ax + 9, s.y, ax - 9, s.y + 10, c);
      }
    }
  }
  // botões: som, regras e sair
  {
    RectF r = muteBtn_.rect;
    Color c = Color::hex(0xE49B71);
    float x = r.x + 12, y = r.cy();
    gfx::fillRect({x, y - 6, 8, 12}, c);
    gfx::fillTriangle(x + 6, y - 6, x + 18, y - 15, x + 18, y + 15, c);
    gfx::fillTriangle(x + 6, y - 6, x + 6, y + 6, x + 18, y + 15, c);
    if (app::settings().muted) {
      gfx::line(x + 24, y - 8, x + 36, y + 8, 3, c);
      gfx::line(x + 24, y + 8, x + 36, y - 8, 3, c);
    } else {
      gfx::strokeCircle(x + 20, y, 8, 2, c);
      gfx::fillRect({x + 12, y - 12, 8, 24}, Color(0, 0, 0, 0));
      gfx::line(x + 24, y - 7, x + 27, y, 2, c);
      gfx::line(x + 27, y, x + 24, y + 7, 2, c);
      gfx::line(x + 29, y - 11, x + 33, y, 2, c);
      gfx::line(x + 33, y, x + 29, y + 11, 2, c);
    }
  }
  if (showRulesButton_) {
    RectF r = rulesBtn_.rect.inset(4);
    gfx::strokeRoundRect(r, 12, 2, th.text);
    app::font(FontKind::Regular).draw("?", r.cx(), r.y + 6, 30, th.text, Align::Center);
  }
  {
    RectF r = quitBtn_.rect.inset(12);
    Color c = Color::hex(0xE49B71);
    gfx::line(r.x, r.y, r.x + r.w, r.y + r.h, 3, c);
    gfx::line(r.x + r.w, r.y, r.x, r.y + r.h, 3, c);
  }
  (void)f;
}

// Texto com partes destacadas entre {chavetas} (span.ddl-match-dialog-highlight do original:
// negrito, maiúsculas e branco). Quebra as linhas tendo em conta os dois estilos.
namespace {
struct RichFrag {
  std::string text;
  bool hi;
};
using RichWord = std::vector<RichFrag>;

std::vector<RichWord> richWords(const std::string& s) {
  std::vector<RichWord> words(1);
  bool hi = false;
  std::string cur;
  auto flush = [&] {
    if (!cur.empty()) words.back().push_back({hi ? text::upper(cur) : cur, hi});
    cur.clear();
  };
  for (char ch : s) {
    if (ch == '{' || ch == '}') {
      flush();
      hi = ch == '{';
    } else if (ch == ' ' || ch == 10) {
      flush();
      if (!words.back().empty()) words.emplace_back();
    } else {
      cur += ch;
    }
  }
  flush();
  if (words.back().empty()) words.pop_back();
  return words;
}

float richWidth(const RichWord& w, float px) {
  float x = 0;
  for (auto& f : w) x += app::font(f.hi ? FontKind::Bold : FontKind::Light).measure(f.text, px);
  return x;
}

std::vector<std::vector<RichWord>> richWrap(const std::vector<RichWord>& words, float px, float maxW) {
  std::vector<std::vector<RichWord>> lines(1);
  float space = app::font(FontKind::Light).measure(" ", px), x = 0;
  for (auto& w : words) {
    float ww = richWidth(w, px);
    if (!lines.back().empty() && x + space + ww > maxW) {
      lines.emplace_back();
      x = 0;
    }
    x += (lines.back().empty() ? 0 : space) + ww;
    lines.back().push_back(w);
  }
  return lines;
}
}  // namespace

void GameScene::renderDialog() {
  if (dialogAlpha_ <= 0.01f || dialogText_.empty()) return;
  const Theme& th = assets_.theme();
  RectF r = layout_.dialog;
  auto& light = app::font(FontKind::Light);
  auto words = richWords(dialogText_);
  // K(texto, 28, 432, altura, 20): reduz a letra até caber
  float px = layout_.dialogFont, maxW = r.w - 20;
  auto lines = richWrap(words, px, maxW);
  float lh = light.lineHeight(px) * 1.15f;
  while (lines.size() * lh > r.h + 60 && px > 18) {
    lines = richWrap(words, --px, maxW);
    lh = light.lineHeight(px) * 1.15f;
  }
  float need = lines.size() * lh + 24;
  if (need > r.h) {
    // No original a caixa cresce para cima (fica entre a Lua e o tabuleiro); na horizontal cresce para baixo.
    if (layout_.portrait) r.y -= (need - r.h);
    r.h = need;
  }
  gfx::fillRoundRect(r, 16, th.dialogBg.withAlpha(0.92f * dialogAlpha_));
  if (const Texture* b = assets_.tex("border.png")) gfx::nineSlice(b, r.inset(-9), 30, 21, dialogAlpha_);
  float space = light.measure(" ", px);
  float y = r.y + (r.h - lines.size() * lh) * 0.5f + (lh - light.lineHeight(px)) * 0.5f;
  for (auto& line : lines) {
    float w = 0;
    for (size_t i = 0; i < line.size(); i++) w += (i ? space : 0) + richWidth(line[i], px);
    float x = r.cx() - w / 2;
    for (size_t i = 0; i < line.size(); i++) {
      if (i) x += space;
      for (auto& f : line[i]) {
        Font& font = app::font(f.hi ? FontKind::Bold : FontKind::Light);
        font.draw(f.text, x, y, px, f.hi ? Color(255, 255, 255) : th.text, Align::Left, dialogAlpha_);
        x += font.measure(f.text, px);
      }
    }
    y += lh;
  }
}

void GameScene::renderModal() {
  if (modal_.alpha <= 0.01f || modal_.wild < 0) return;
  const Theme& th = assets_.theme();
  float a = modal_.alpha;
  float W = layout_.W, H = layout_.H;
  gfx::fillRect({0, 0, W, H}, Color(0, 0, 0, (uint8_t)(140 * a)));
  auto& wd = assets_.wildcards()[modal_.wild];
  auto& f = app::font(FontKind::Light);
  if (layout_.portrait) {
    RectF p{45, 210, 450, 560};
    gfx::fillRect(p, th.modalBg.withAlpha(a));
    if (auto* b = assets_.tex("border-card.png")) gfx::nineSlice(b, p.inset(-27), 80, 35, a);
    assets_.drawFrame(wd.front, W / 2, p.y + 10, 200, 200, a);
    f.drawBox(assets_.t(wd.title), {p.x + 25, p.y + 120, 400, 52}, 45, 30, th.modalTitle, Align::Center, a);
    f.drawBox(assets_.t("win_subtitle"), {p.x + 25, p.y + 175, 400, 40}, 35, 25, th.modalSubtitle, Align::Center, a);
    f.drawBox(assets_.t(wd.description), {p.x + 25, p.y + 222, 400, 150}, 26, 18, Color(255, 255, 255), Align::Center,
              a);
    if (modal_.info)
      app::font(FontKind::LightItalic).drawBox(assets_.t(wd.info), {p.x + 25, p.y + 372, 400, 100}, 22, 15, th.modalTitle, Align::Center, a);
    else
      f.drawBox(assets_.t("wildcard_warning"), {p.x + 25, p.y + 380, 400, 60}, 26, 18, th.modalTitle, Align::Center, a);
    modal_.a.rect = modal_.info ? RectF{W / 2 - 110, p.y + p.h - 86, 220, 62} : RectF{W / 2 - 200, p.y + p.h - 86, 190, 62};
    modal_.b.rect = {W / 2 + 10, p.y + p.h - 86, 190, 62};
  } else {
    RectF p{150, 52, 660, 440};
    gfx::fillRect(p, th.modalBg.withAlpha(a));
    if (auto* b = assets_.tex("border-card.png")) gfx::nineSlice(b, p.inset(-20), 80, 26, a);
    assets_.drawFrame(wd.front, p.x + 130, p.y + 150, 200, 200, a);
    RectF tx{p.x + 250, p.y + 24, 390, 0};
    f.drawBox(assets_.t(wd.title), {tx.x, p.y + 24, tx.w, 48}, 38, 24, th.modalTitle, Align::Center, a);
    f.drawBox(assets_.t("win_subtitle"), {tx.x, p.y + 74, tx.w, 34}, 28, 18, th.modalSubtitle, Align::Center, a);
    f.drawBox(assets_.t(wd.description), {tx.x, p.y + 112, tx.w, 120}, 24, 16, Color(255, 255, 255), Align::Center, a);
    if (modal_.info)
      app::font(FontKind::LightItalic).drawBox(assets_.t(wd.info), {tx.x, p.y + 236, tx.w, 100}, 20, 14, th.modalTitle, Align::Center, a);
    else
      f.drawBox(assets_.t("wildcard_warning"), {tx.x, p.y + 246, tx.w, 70}, 24, 16, th.modalTitle, Align::Center, a);
    modal_.a.rect = modal_.info ? RectF{p.cx() - 110, p.y + p.h - 84, 220, 62} : RectF{p.cx() - 200, p.y + p.h - 84, 190, 62};
    modal_.b.rect = {p.cx() + 10, p.y + p.h - 84, 190, 62};
  }
  ui::drawPill(modal_.a, 24, th.modalBg, Color::hex(0xE49B71), Color(255, 255, 255));
  if (!modal_.info) ui::drawPill(modal_.b, 24, th.modalBg, Color::hex(0xE49B71), Color(255, 255, 255));
}

void GameScene::renderQuitConfirm() {
  float W = layout_.W, H = layout_.H;
  gfx::fillRect({0, 0, W, H}, Color(0, 0, 0, 180));
  RectF p{W / 2 - 220, H / 2 - 120, 440, 230};
  ui::drawPanel(p, Color::hex(0x2B1729), ui::kAccent, 18);
  app::font(FontKind::Regular).drawBox(i18n::tr("quit_question"),
                                       {p.x + 24, p.y + 20, p.w - 48, 100}, 24, 16, ui::kInk);
  ui::drawPill(quitYes_, 22);
  ui::drawPill(quitNo_, 22);
}

std::unique_ptr<Scene> makeGameScene(const GameDef& g) { return std::make_unique<GameScene>(g); }

// Menu principal (sempre na horizontal): os 8 jogos, língua, orientação e descarga.
#include <cmath>
#include <memory>

#include "audio.h"
#include "catalog.h"
#include "downloader.h"
#include "i18n.h"
#include "platform.h"
#include "scenes.h"
#include "ui.h"
#include "version.h"

namespace {

using namespace ui;

class MenuScene : public Scene {
 public:
  MenuScene() {
    for (auto& g : catalog::games()) {
      Card c;
      c.game = g;
      cards_.push_back(c);
    }
    refresh();
    layout();
  }

  void update(float) override {
    if (dl_ && dl_->finished()) {
      status_ = dl_->status();
      dl_.reset();
      refresh();
      if (pendingPlay_ >= 0) {
        auto& g = catalog::games()[pendingPlay_];
        pendingPlay_ = -1;
        if (catalog::isAvailable(g)) startGame(g);
      }
    }
    downloadBtn_.label = dl_ ? i18n::tr("cancel") + "  " + std::to_string((int)(dl_->progress() * 100)) + "%"
                             : i18n::tr("download_update");
    landBtn_.label = i18n::tr("landscape");
    portBtn_.label = flipBtn_.label = i18n::tr("portrait");
  }

  void render() override {
    gfx::verticalGradient({0, 0, 960, 544}, Color::hex(0x140B19), Color::hex(0x4E2635));
    auto& bold = app::font(FontKind::Bold);
    auto& light = app::font(FontKind::Light);
    bold.draw("Rise of the Half Moon", 28, 18, 34, kInk);
    std::string sub = dl_ ? statusText(dl_->status()) : status_.empty() ? i18n::tr("subtitle") : statusText(status_);
    light.draw(sub, 30, 58, 18, kInkMuted);

    drawLanguageButton();
    drawPill(landBtn_, 19);
    drawOrientation(portBtn_, true);
    drawOrientation(flipBtn_, false);
    drawPill(downloadBtn_, 19, Color::hex(0x6A3550), Color::hex(0x854263));
    drawPill(aboutBtn_, 24);
    if (dl_) {
      RectF r = downloadBtn_.rect;
      gfx::fillRoundRect({r.x + 18, r.y + r.h - 7, (r.w - 36) * dl_->progress(), 3}, 1.5f, kAccent);
    }

    for (auto& c : cards_) drawCard(c);

    if (langOpen_) drawLanguagePicker();
    if (askGame_ >= 0) drawAskDownload();
    if (aboutOpen_) drawAbout();
  }

  void touch(const Touch& t) override {
    if (aboutOpen_) {
      aboutSet_.touch(t);
      return;
    }
    if (langOpen_) {
      langSet_.touch(t);
      if (t.kind == Touch::Up && !langPanel_.contains(t.x, t.y) && !langSet_.buttons.empty()) {
        bool onButton = false;
        for (auto* b : langSet_.buttons) onButton |= b->hit(t.x, t.y);
        if (!onButton) langOpen_ = false;
      }
      return;
    }
    if (askGame_ >= 0) {
      askSet_.touch(t);
      return;
    }
    if (top_.touch(t)) return;
    // Cartões dos jogos
    if (t.kind == Touch::Down) {
      pressedCard_ = -1;
      for (size_t i = 0; i < cards_.size(); i++)
        if (cards_[i].rect.contains(t.x, t.y)) pressedCard_ = (int)i;
    } else if (t.kind == Touch::Up && pressedCard_ >= 0) {
      int i = pressedCard_;
      pressedCard_ = -1;
      if (cards_[i].rect.contains(t.x, t.y)) openGame(i);
    } else if (t.kind == Touch::Move && pressedCard_ >= 0 && !cards_[pressedCard_].rect.contains(t.x, t.y)) {
      pressedCard_ = -1;
    }
  }

  void back() override {
    if (aboutOpen_) aboutOpen_ = false;
    else if (langOpen_) langOpen_ = false;
    else if (askGame_ >= 0) askGame_ = -1;
  }

 private:
  struct Card {
    GameDef game;
    RectF rect;
    TexPtr thumb;
    bool available = false;
  };

  void refresh() {
    for (auto& c : cards_) {
      c.available = catalog::isAvailable(c.game);
      if (!c.thumb && plat::fileExists(c.game.thumbPath())) c.thumb = gfx::loadTexture(c.game.thumbPath());
    }
  }

  void layout() {
    float y = 92, h = 50;
    langBtn_ = Button{{28, y, 196, h}, ""};
    langBtn_.onTap = [this] { openLanguages(); };
    landBtn_ = Button{{232, y, 128, h}, ""};
    landBtn_.onTap = [this] { setOrientation(Orientation::Landscape); };
    // Vertical: o jogo fica com o topo do lado esquerdo do ecrã (rodar a consola no sentido dos ponteiros)
    portBtn_ = Button{{368, y, 140, h}, ""};
    portBtn_.onTap = [this] { setOrientation(Orientation::Portrait); };
    // Vertical invertido: topo do jogo do lado direito (rodar no sentido contrário)
    flipBtn_ = Button{{516, y, 140, h}, ""};
    flipBtn_.onTap = [this] { setOrientation(Orientation::PortraitFlipped); };
    downloadBtn_ = Button{{664, y, 208, h}, ""};
    downloadBtn_.onTap = [this] {
      if (dl_) dl_->cancel();
      else startDownload(catalog::games(), -1);
    };
    aboutBtn_ = Button{{880, y, 52, h}, "?"};
    aboutBtn_.onTap = [this] { openAbout(); };
    top_.buttons = {&langBtn_, &landBtn_, &portBtn_, &flipBtn_, &downloadBtn_, &aboutBtn_};
    updateLabels();

    float gx = 24, gy = 160, gap = 14;
    float cw = (960 - 2 * gx - 3 * gap) / 4, ch = (544 - gy - 16 - gap) / 2;
    for (size_t i = 0; i < cards_.size(); i++) {
      int col = (int)i % 4, row = (int)i / 4;
      cards_[i].rect = {gx + col * (cw + gap), gy + row * (ch + gap), cw, ch};
    }
  }

  // Botão "Portrait" com uma PS Vita desenhada na vertical: numa ponta a cruz e o analógico esquerdo,
  // na outra os botões de ação e o analógico direito. "dpadTop" = cruz em cima.
  void drawOrientation(const Button& b, bool dpadTop) {
    Button copy = b;
    copy.label = "";
    drawPill(copy, 19);
    RectF r = b.pressed ? b.rect.inset(2) : b.rect;
    Color c = b.selected ? kInk : kInkMuted;
    float cx = r.x + 30, cy = r.cy();
    float bw = 20, bh = 40;
    gfx::strokeRoundRect({cx - bw / 2, cy - bh / 2, bw, bh}, 8, 1.5f, c);
    gfx::strokeRoundRect({cx - 6, cy - 9, 12, 18}, 1.5f, 1.2f, c);  // ecrã
    float dy = dpadTop ? -1.f : 1.f;
    // ponta da cruz direcional + analógico esquerdo
    float py = cy + dy * 14.5f;
    gfx::fillRect({cx - 4.5f, py - 1, 4, 2}, c);
    gfx::fillRect({cx - 3.5f, py - 2, 2, 4}, c);
    gfx::fillCircle(cx + 3.5f, py, 1.6f, c);
    // ponta dos botões de ação + analógico direito
    float qy = cy - dy * 14.5f;
    gfx::fillCircle(cx - 4, qy, 1.6f, c);
    gfx::fillCircle(cx + 2.5f, qy - 1.6f, 1, c);
    gfx::fillCircle(cx + 2.5f, qy + 1.6f, 1, c);
    gfx::fillCircle(cx + 1, qy, 1, c);
    gfx::fillCircle(cx + 4, qy, 1, c);
    drawFitted(b.label, {r.x + 48, r.y, r.w - 56, r.h}, 18, b.selected ? kInk : kInkMuted, 1.f, Align::Left);
  }

  void updateLabels() {
    auto& s = app::settings();
    for (auto& l : catalog::languages())
      if (l.code == s.language) langBtn_.label = l.shortName.empty() ? l.name : l.shortName;
    landBtn_.selected = s.orientation == Orientation::Landscape;
    portBtn_.selected = s.orientation == Orientation::Portrait;
    flipBtn_.selected = s.orientation == Orientation::PortraitFlipped;
  }

  void setOrientation(Orientation o) {
    app::settings().orientation = o;
    app::settings().save();
    updateLabels();
  }

  void drawCard(const Card& c) {
    bool pressed = pressedCard_ >= 0 && &cards_[pressedCard_] == &c;
    RectF r = pressed ? c.rect.inset(3) : c.rect;
    drawPanel(r, pressed ? kCardHi : kCard, pressed ? kAccent : kBorder, 14);
    RectF th{r.x + 10, r.y + 10, r.w - 20, r.h * 0.5f};
    gfx::fillRoundRect(th, 8, Color::hex(0xF7F2EE));
    if (c.thumb) {
      float s = std::min((th.w - 8) / c.thumb->w, (th.h - 8) / c.thumb->h);
      gfx::drawCentered(c.thumb.get(), nullptr, th.cx(), th.cy(), c.thumb->w * s, c.thumb->h * s);
    } else {
      gfx::fillCircle(th.cx(), th.cy(), th.h * 0.32f, Color::hex(0xE8DCD0));
      gfx::fillRect({th.cx(), th.cy() - th.h * 0.32f, th.h * 0.32f, th.h * 0.64f}, Color::hex(0xF7F2EE));
      gfx::strokeCircle(th.cx(), th.cy(), th.h * 0.32f, 2, Color::hex(0xC9A9A6));
    }
    app::font(FontKind::Regular).draw(i18n::gameTitle(c.game.index), r.x + 12, th.y + th.h + 12, 21, kInk);
    std::string badge;
    Color bc;
    if (!c.game.playable()) {
      badge = i18n::tr("coming_soon");
      bc = kInkMuted;
    } else if (c.available) {
      badge = i18n::tr("available");
      bc = kGood;
    } else {
      badge = i18n::tr("not_downloaded");
      bc = kWarn;
    }
    auto& f = app::font(FontKind::Bold);
    float bw = f.measure(badge, 14) + 22;
    RectF br{r.x + 12, r.y + r.h - 36, bw, 24};
    gfx::fillRoundRect(br, 12, bc);
    f.draw(badge, br.cx(), br.y + 4, 14, Color::hex(0x1A0F1F), Align::Center);
  }

  void openLanguages() {
    langOpen_ = true;
    langButtons_.clear();
    langButtons_.reserve(catalog::languages().size());
    langPanel_ = {130, 60, 700, 424};
    int i = 0;
    for (auto& l : catalog::languages()) {
      Button b{{langPanel_.x + 30 + (i % 2) * 330, langPanel_.y + 70 + (i / 2) * 76, 310, 60}, l.name};
      b.selected = l.code == app::settings().language;
      std::string code = l.code;
      b.onTap = [this, code] {
        app::settings().language = code;
        app::settings().save();
        updateLabels();  // o menu muda logo de língua
        status_.clear();
        langOpen_ = false;
      };
      langButtons_.push_back(b);
      i++;
    }
    langSet_.buttons.clear();
    for (auto& b : langButtons_) langSet_.buttons.push_back(&b);
  }

  void drawLanguagePicker() {
    gfx::fillRect({0, 0, 960, 544}, Color(0, 0, 0, 170));
    drawPanel(langPanel_, Color::hex(0x2B1729), kAccent, 18);
    app::font(FontKind::Bold).draw(i18n::tr("language"), langPanel_.cx(), langPanel_.y + 22, 26, kInk, Align::Center);
    for (auto& b : langButtons_) drawPill(b, 21);
  }

  void openGame(int i) {
    auto& g = cards_[i].game;
    if (!g.playable()) {
      status_ = i18n::fmt("not_supported", i18n::gameTitle(g.index));
      return;
    }
    if (dl_) return;
    if (cards_[i].available) {
      startGame(g);
      return;
    }
    askGame_ = i;
    askYes_ = Button{{290, 330, 180, 56}, i18n::tr("download")};
    askNo_ = Button{{490, 330, 180, 56}, i18n::tr("cancel")};
    askYes_.onTap = [this] {
      int gi = askGame_;
      askGame_ = -1;
      startDownload({catalog::games()[gi]}, gi);
    };
    askNo_.onTap = [this] { askGame_ = -1; };
    askSet_.buttons = {&askYes_, &askNo_};
  }

  void drawAskDownload() {
    gfx::fillRect({0, 0, 960, 544}, Color(0, 0, 0, 170));
    RectF p{230, 150, 500, 270};
    drawPanel(p, Color::hex(0x2B1729), kAccent, 18);
    auto& g = cards_[askGame_].game;
    app::font(FontKind::Bold).draw(i18n::gameTitle(g.index), p.cx(), p.y + 26, 26, kInk, Align::Center);
    app::font(FontKind::Light).drawBox(i18n::tr("ask_download"), {p.x + 30, p.y + 66, p.w - 60, 100}, 20, 14, kInkMuted);
    drawPill(askYes_, 21, Color::hex(0x6A3550), Color::hex(0x854263));
    drawPill(askNo_, 21);
  }

  void startDownload(const std::vector<GameDef>& games, int playAfter) {
    dl_ = std::make_unique<Downloader>();
    pendingPlay_ = playAfter;
    status_.clear();
    dl_->start(games);
  }

  void startGame(const GameDef& g) { app::setScene(makeGameScene(g)); }

  // Mensagens do descarregador: "@chave" (fim) ou "<índice do jogo>|<ficheiro>" (em curso).
  static std::string statusText(const std::string& s) {
    if (s.empty()) return s;
    if (s[0] == '@') return i18n::tr(s.c_str() + 1);
    auto bar = s.find('|');
    if (bar != std::string::npos && bar > 0 && isdigit((unsigned char)s[0]))
      return i18n::gameTitle(atoi(s.c_str())) + ": " + s.substr(bar + 1);
    return s;
  }

  // Botão da língua: globo + nome da língua.
  void drawLanguageButton() {
    Button copy = langBtn_;
    copy.label = "";
    drawPill(copy, 19);
    RectF r = langBtn_.pressed ? langBtn_.rect.inset(2) : langBtn_.rect;
    float cx = r.x + 26, cy = r.cy(), rad = 10;
    gfx::strokeCircle(cx, cy, rad, 1.5f, kInk);
    gfx::line(cx - rad, cy, cx + rad, cy, 1.2f, kInk);
    gfx::line(cx, cy - rad, cx, cy + rad, 1.2f, kInk);
    for (int i = 0; i < 12; i++) {  // meridiano em elipse
      float a0 = 6.2831853f * i / 12, a1 = 6.2831853f * (i + 1) / 12;
      gfx::line(cx + std::cos(a0) * rad * 0.45f, cy + std::sin(a0) * rad, cx + std::cos(a1) * rad * 0.45f,
                cy + std::sin(a1) * rad, 1.2f, kInk);
    }
    drawFitted(langBtn_.label, {r.x + 44, r.y, r.w - 52, r.h}, 18, kInk, 1.f, Align::Left);
  }

  // ---------------------------------------------------------------- About
  void openAbout() {
    aboutOpen_ = true;
    aboutClose_ = Button{{400, 458, 160, 46}, i18n::tr("close")};
    aboutClose_.onTap = [this] { aboutOpen_ = false; };
    aboutSet_.buttons = {&aboutClose_};
  }

  void drawAbout() {
    gfx::fillRect({0, 0, 960, 544}, Color(0, 0, 0, 180));
    RectF p{80, 18, 800, 508};
    drawPanel(p, Color::hex(0x2B1729), kAccent, 18);
    if (auto icon = app::uiTexture(plat::assetDir() + "icon.png"))
      gfx::draw(icon.get(), nullptr, {p.x + 28, p.y + 24, 64, 64});
    auto& bold = app::font(FontKind::Bold);
    auto& reg = app::font(FontKind::Regular);
    auto& light = app::font(FontKind::Light);
    bold.draw("Rise of the Half Moon", p.x + 112, p.y + 22, 28, kInk);
    light.draw(i18n::tr("about_port") + "  ·  v" RHM_VERSION "", p.x + 114, p.y + 58, 18, kInkMuted);
    reg.draw("Port by MDashK / AI assisted by Claude", p.x + 114, p.y + 82, 19, kAccent);
    light.drawBox(i18n::tr("about_google"), {p.x + 28, p.y + 112, p.w - 56, 64}, 17, 12, kInk, Align::Left);
    light.drawBox(i18n::tr("about_unofficial"), {p.x + 28, p.y + 180, p.w - 56, 46}, 17, 12, kInk, Align::Left);
    bold.draw(i18n::tr("third_party"), p.x + 28, p.y + 236, 17, kInk);
    static const char* lines[] = {
        "Josefin Sans © The Josefin Sans Project Authors — SIL Open Font License 1.1",
        "SDL2 © Sam Lantinga — zlib License",
        "libcurl © Daniel Stenberg — curl License   ·   Mbed TLS — Apache License 2.0",
        "stb_image, stb_truetype, stb_vorbis © Sean Barrett — public domain",
        "zlib © Jean-loup Gailly & Mark Adler   ·   Zstandard © Meta Platforms — BSD License",
        "CA certificates © Mozilla — MPL 2.0   ·   Built with VitaSDK",
        "Google and Google Doodle are trademarks of Google LLC.",
        "PlayStation and PS Vita are trademarks of Sony Interactive Entertainment Inc.",
    };
    float y = p.y + 262;
    for (auto* l : lines) {
      light.drawBox(l, {p.x + 28, y, p.w - 56, 20}, 14, 10, kInkMuted, Align::Left);
      y += 20;
    }
    drawPill(aboutClose_, 20);
  }

  std::vector<Card> cards_;
  Button langBtn_, landBtn_, portBtn_, flipBtn_, downloadBtn_, aboutBtn_;
  bool aboutOpen_ = false;
  Button aboutClose_;
  ButtonSet aboutSet_;
  ButtonSet top_;
  bool langOpen_ = false;
  RectF langPanel_;
  std::vector<Button> langButtons_;
  ButtonSet langSet_;
  int askGame_ = -1;
  Button askYes_, askNo_;
  ButtonSet askSet_;
  int pressedCard_ = -1;
  std::unique_ptr<Downloader> dl_;
  int pendingPlay_ = -1;
  std::string status_;
};

}  // namespace

std::unique_ptr<Scene> makeMenuScene() { return std::make_unique<MenuScene>(); }

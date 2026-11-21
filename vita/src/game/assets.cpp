#include "assets.h"

#include <cstdlib>
#include <cstring>

#include "platform.h"

Frame frameFromJson(const Json& j) {
  Frame f;
  if (!j.isArray() || j.size() < 5) return f;
  f.sheet = j[0].integer();
  f.x = j[1].integer();
  f.y = j[2].integer();
  f.w = j[3].integer();
  f.h = j[4].integer();
  return f;
}

static const char* kPhaseIds[8] = {"new_moon",      "waxing_crescent", "first_quarter", "waxing_gibbous",
                                   "full_moon",     "waning_gibbous",  "third_quarter", "waning_crescent"};

bool MonthAssets::load(const GameDef& g, const std::string& language) {
  game_ = g;
  std::string text;
  if (!plat::readText(g.dataJson(), text)) return false;
  data_ = Json::parse(text);
  std::string dir = g.localDir();

  for (auto& s : data_["sheets"].items()) {
    const std::string& f = s["file"].str();
    bool early = f == "background-sprite.png" || f == "moon-sprite-light-moon-fade-in.png" || f == "spinner-sprite.png";
    sheets_.push_back(early ? gfx::loadTexture(dir + f) : nullptr);
  }
  for (const char* n : {"GoogleFrame.png", "border.png", "border-card.png", "button_border.png", "cta.png"})
    extra_[n] = gfx::loadTexture(dir + n);

  // Traduções: língua escolhida, com inglês como alternativa.
  if (plat::readText(dir + "messages.en.nocache.json", text)) messagesEn_ = Json::parse(text);
  if (language != "en" && plat::readText(dir + "messages." + language + ".nocache.json", text))
    messages_ = Json::parse(text);
  else
    messages_ = messagesEn_;

  for (auto& s : data_["sounds"].items())
    sounds_.push_back(SoundRef{(float)s["start"].num(), (float)s["dur"].num()});
  // Outubro não tem o som de "próximo nível" (63 sons): os restantes ficam na mesma ordem.
  if (sounds_.size() == S_COUNT - 1) sounds_.insert(sounds_.begin() + S_NEXT_LEVEL, SoundRef());

  for (auto& kv : data_["anims"].members()) {
    Anim a;
    for (auto& f : kv.second["frames"].items()) a.frames.push_back(frameFromJson(f));
    anims_[kv.first] = a;
  }
  for (int v = 0; v < 8; v++) {
    const Json& c = data_["cards"][kPhaseIds[v]];
    face_[v] = frameFromJson(c["face"]);
    light_[v] = frameFromJson(c["light"]);
    dark_[v] = frameFromJson(c["dark"]);
  }
  const Json& fr = data_["frames"];
  // O verso vem da tabela das cartas (em abril/maio a folha mudou e o "card_back" genérico é outra carta).
  cardBack_ = frameFromJson(data_["cards"]["new_moon"].has("back") ? data_["cards"]["new_moon"]["back"] : fr["card_back"]);
  tile_ = frameFromJson(fr["tile"]);
  stars_ = frameFromJson(fr["stars"]);
  moonDark_ = frameFromJson(fr["bg_moons"][0]);
  moonLight_ = frameFromJson(fr["bg_moons"][1]);
  for (auto& f : fr["progress_empty"].items()) progressEmpty_.push_back(frameFromJson(f));
  for (auto& kv : fr["rules"].members()) rules_[kv.first] = frameFromJson(kv.second);

  for (auto& w : data_["wildcards"].items()) {
    WildcardDef d;
    d.id = w["id"].str();
    d.front = frameFromJson(w["front"]);
    d.back = frameFromJson(w["back"]);
    const Json& t = w["texts"];
    d.title = t[0].str();
    d.description = t[1].str();
    d.popup = t[2].str();
    d.info = t[3].str();
    d.query = t[4].str();
    wildcards_.push_back(d);
  }
  for (auto& kv : data_["boards"].members()) boards_[kv.first] = rules::BoardDef::fromJson(kv.second);

  for (auto& l : data_["intro"].items()) {
    IntroLine il;
    il.type = l["type"].integer();
    il.start = (float)l["startTime"].num();
    il.y = 600;  // o nome minificado do campo "y" muda de mês para mês (Pa, Ua, ...)
    for (auto& kv : l.members())
      if (kv.first != "type" && kv.first != "startTime" && kv.second.isNumber()) il.y = (float)kv.second.num();
    il.text = l["text"].str();
    intro_.push_back(il);
  }
  css_ = data_["css"].str();
  parseTheme();
  // O fundo tem de ter carregado (em outubro/novembro a folha 0 é "main-sprite.png", por isso procura-se pelo nome).
  bool bg = false;
  for (size_t i = 0; i < sheets_.size(); i++)
    if (data_["sheets"][i]["file"].str() == "background-sprite.png") bg = sheets_[i] != nullptr;
  return sheets_.size() > 13 && bg && plat::fileExists(dir + "cards-sprite.png");
}

void MonthAssets::loadDeferred() {
  std::string dir = game_.localDir();
  const Json& list = data_["sheets"];
  for (size_t i = 0; i < sheets_.size() && i < list.size(); i++)
    if (!sheets_[i]) sheets_[i] = gfx::loadTexture(dir + list[i]["file"].str());
  if (!cardsDim_)
    // filter: contrast(.25) -> cor = (c - 0,5) * 0,25 + 0,5
    cardsDim_ = gfx::loadTextureTransformed(dir + "cards-sprite.png", [](uint8_t* px, int n) {
      for (int i = 0; i < n; i++)
        for (int k = 0; k < 3; k++) px[i * 4 + k] = (uint8_t)((px[i * 4 + k] - 128) / 4 + 128);
    });
}

void MonthAssets::drawFrameDim(const Frame& f, float cx, float cy, float w, float h, float alpha, float angle) const {
  if (!cardsDim_ || f.sheet != 12) return drawFrame(f, cx, cy, w, h, alpha, angle);
  SDL_Rect src{f.x, f.y, f.w, f.h};
  gfx::drawCentered(cardsDim_.get(), &src, cx, cy, w, h, alpha, angle);
}

const Texture* MonthAssets::tex(const std::string& name) const {
  auto it = extra_.find(name);
  return it == extra_.end() ? nullptr : it->second.get();
}

void MonthAssets::drawFrame(const Frame& f, float cx, float cy, float w, float h, float alpha, float angle,
                            Color tint) const {
  const Texture* t = sheet(f.sheet);
  if (!t || !f.valid()) return;
  SDL_Rect src{f.x, f.y, f.w, f.h};
  gfx::drawCentered(t, &src, cx, cy, w, h, alpha, angle, tint);
}

const std::string& MonthAssets::t(const std::string& key) const {
  const Json& v = messages_[key];
  if (v.isString()) return v.str();
  const Json& e = messagesEn_[key];
  if (e.isString()) return e.str();
  static std::string empty;
  return empty;
}

const Anim* MonthAssets::anim(const std::string& name) const {
  auto it = anims_.find(name);
  return it == anims_.end() ? nullptr : &it->second;
}

Frame MonthAssets::cardFace(int v) const { auto it = face_.find(v); return it == face_.end() ? Frame() : it->second; }
Frame MonthAssets::cardLight(int v) const { auto it = light_.find(v); return it == light_.end() ? Frame() : it->second; }
Frame MonthAssets::cardDark(int v) const { auto it = dark_.find(v); return it == dark_.end() ? Frame() : it->second; }
Frame MonthAssets::rules(const std::string& k) const {
  auto it = rules_.find(k);
  return it == rules_.end() ? Frame() : it->second;
}

const rules::BoardDef* MonthAssets::board(const std::string& id) const {
  auto it = boards_.find(id);
  return it == boards_.end() ? nullptr : &it->second;
}

std::string MonthAssets::monthName() const { return t(game_.monthKey); }

// ---------------------------------------------------------------- CSS → cores

static bool parseColor(const std::string& v, Color& out) {
  size_t h = v.find('#');
  size_t r = v.find("rgb");
  if (h != std::string::npos && (r == std::string::npos || h < r)) {
    std::string hex;
    for (size_t i = h + 1; i < v.size() && isxdigit((unsigned char)v[i]); i++) hex += v[i];
    if (hex.size() == 3) hex = {hex[0], hex[0], hex[1], hex[1], hex[2], hex[2]};
    if (hex.size() != 6) return false;
    out = Color::hex((uint32_t)strtoul(hex.c_str(), nullptr, 16));
    return true;
  }
  if (r != std::string::npos) {
    int a, b, c;
    if (sscanf(v.c_str() + v.find('(', r) + 1, "%d,%d,%d", &a, &b, &c) == 3) {
      out = Color((uint8_t)a, (uint8_t)b, (uint8_t)c);
      return true;
    }
  }
  if (v.find("white") != std::string::npos) { out = Color(255, 255, 255); return true; }
  return false;
}

Color MonthAssets::css(const std::string& selector, const std::string& prop, Color def) const {
  size_t pos = 0;
  while ((pos = css_.find(selector, pos)) != std::string::npos) {
    size_t open = css_.find('{', pos);
    size_t close = css_.find('}', open);
    // O seletor tem de terminar logo antes de '{' ou de ','.
    std::string sel = css_.substr(pos, open - pos);
    if (open == std::string::npos) break;
    if (sel == selector || sel.rfind(selector + ",", 0) == 0) {
      std::string block = css_.substr(open + 1, close - open - 1);
      size_t p = 0;
      while ((p = block.find(prop + ":", p)) != std::string::npos) {
        if (p == 0 || block[p - 1] == ';' || block[p - 1] == '{') {
          size_t end = block.find(';', p);
          Color c;
          if (parseColor(block.substr(p + prop.size() + 1, end - p - prop.size() - 1), c)) return c;
        }
        p++;
      }
    }
    pos = open;
  }
  return def;
}

void MonthAssets::parseTheme() {
  Theme& t = theme_;
  t.text = css(".ddl-score-text", "color", t.text);
  t.dialogBg = css(".ddl-match-dialog", "background", t.dialogBg);
  t.modalBg = css(".ddl-card-modal", "background", t.modalBg);
  t.modalTitle = css(".ddl-card-modal-card-title", "color", t.modalTitle);
  t.modalSubtitle = css(".ddl-card-modal-card-subtitle", "color", t.modalSubtitle);
  t.button = css(".ddl-button-default", "background-color", t.button);
  t.buttonHi = css(".ddl-button-default:hover", "background-color", t.buttonHi);
  t.connection = css(".ddl-connection-empty", "stroke", t.connection);
  t.starWhiteStroke = css(".ddl-star-white", "stroke", t.starWhiteStroke);
  t.starBlackStroke = css(".ddl-star-black", "stroke", t.starBlackStroke);
  t.turnPlayer = css(".ddl-score-turn-left", "fill", t.turnPlayer);
  t.turnMoon = css(".ddl-score-turn-right", "fill", t.turnMoon);
  t.progressText = css(".ddl-progress-top-message", "color", t.progressText);
  t.lineEmpty = css("line.ddl-progress-line-empty", "stroke", t.lineEmpty);
  t.rulesInner = css(".ddl-rules-matches", "background", t.rulesInner);
  t.rulesPoints = css(".ddl-rules-match-points", "color", t.rulesPoints);

  // Gradientes: "linear-gradient(#a,#b)" das regras e "-90deg, rgb() x%, ..." do céu.
  auto block = [&](const std::string& sel) {
    size_t p = css_.find(sel + "{");
    if (p == std::string::npos) return std::string();
    size_t o = css_.find('{', p), c = css_.find('}', o);
    return css_.substr(o + 1, c - o - 1);
  };
  std::string rb = block(".ddl-rules");
  size_t lg = rb.rfind("linear-gradient(");
  if (lg != std::string::npos) {
    std::string args = rb.substr(lg + 16);
    size_t comma = args.find(',');
    parseColor(args.substr(0, comma), t.rulesTop);
    parseColor(args.substr(comma + 1), t.rulesBottom);
  }
  std::string bg = block(".ddl-background-inner");
  lg = bg.rfind("linear-gradient(");
  if (lg != std::string::npos) {
    std::string args = bg.substr(lg + 16);
    // Lista de "rgb(r,g,b) p%" depois do ângulo; o ângulo -90deg vai da direita para a esquerda.
    size_t p = 0;
    std::vector<std::pair<float, Color>> stops;
    while ((p = args.find("rgb(", p)) != std::string::npos) {
      size_t close = args.find(')', p);
      Color c;
      parseColor(args.substr(p, close - p + 1), c);
      float pct = (float)atof(args.c_str() + close + 1) / 100.f;
      stops.push_back({pct, c});
      p = close;
    }
    // Converte para posições da esquerda para a direita.
    for (auto it = stops.rbegin(); it != stops.rend(); ++it) t.sky.push_back({1.f - it->first, it->second});
    if (!t.sky.empty() && t.sky.front().first > 0) t.sky.insert(t.sky.begin(), {0.f, t.sky.front().second});
    if (!t.sky.empty() && t.sky.back().first < 1) t.sky.push_back({1.f, t.sky.back().second});
  }
  if (t.sky.empty()) t.sky = {{0.f, Color::hex(0xAF8288)}, {1.f, Color(0, 0, 0)}};
}

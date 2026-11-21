#include "gfx.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "platform.h"
#include "stb_image.h"
#include "stb_truetype.h"

Texture::~Texture() {
  if (tex) SDL_DestroyTexture(tex);
}

namespace gfx {

static SDL_Renderer* g_r = nullptr;
bool dbgNoGeometry = false, dbgNoTarget = false, dbgNoText = false, dbgNoTextures = false, dbgTrace = false, dbgDrawTrace = false,
     dbgNoExCopy = false;

std::vector<std::string> traceLines;
int traceWarnings = 0;

void trace(const char* fmt, ...) {
  if (!dbgTrace) return;
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  if (strstr(buf, "WARN")) traceWarnings++;
  if (!dbgDrawTrace) {
    traceLines.push_back(buf);
    if (traceLines.size() > 14) traceLines.erase(traceLines.begin());
  }
  static FILE* f = nullptr;
  if (!f) f = fopen((plat::dataDir() + "trace.txt").c_str(), "w");
  if (!f) return;
  fputs(buf, f);
  fputc(10, f);
  fflush(f);
}

bool init(SDL_Renderer* r) {
  g_r = r;
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  return true;
}
void shutdown() { g_r = nullptr; }
SDL_Renderer* renderer() { return g_r; }

TexPtr loadTextureMem(const void* data, size_t size) {
  int w, h, n;
  unsigned char* px = stbi_load_from_memory((const stbi_uc*)data, (int)size, &w, &h, &n, 4);
  if (!px) return nullptr;
  SDL_Texture* t = SDL_CreateTexture(g_r, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, w, h);
  if (!t) {
    trace("texture create failed %dx%d: %s", w, h, SDL_GetError());
    stbi_image_free(px);
    return nullptr;
  }
  SDL_UpdateTexture(t, nullptr, px, w * 4);
  SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
  stbi_image_free(px);
  return std::make_shared<Texture>(t, w, h);
}

TexPtr loadTextureTransformed(const std::string& path, void (*fn)(uint8_t* rgba, int count)) {
  std::vector<uint8_t> buf;
  if (!plat::readFile(path, buf)) return nullptr;
  int w, h, n;
  unsigned char* px = stbi_load_from_memory(buf.data(), (int)buf.size(), &w, &h, &n, 4);
  if (!px) return nullptr;
  fn(px, w * h);
  SDL_Texture* t = SDL_CreateTexture(g_r, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, w, h);
  if (t) {
    SDL_UpdateTexture(t, nullptr, px, w * 4);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
  }
  stbi_image_free(px);
  return t ? std::make_shared<Texture>(t, w, h) : nullptr;
}

TexPtr loadTexture(const std::string& path) {
  std::vector<uint8_t> buf;
  if (!plat::readFile(path, buf)) return nullptr;
  return loadTextureMem(buf.data(), buf.size());
}

static bool g_additive = false;
void setAdditive(bool on) { g_additive = on; }

void setTarget(SDL_Texture* t) { SDL_SetRenderTarget(g_r, t); }

void clear(Color c) {
  SDL_SetRenderDrawColor(g_r, c.r, c.g, c.b, c.a);
  SDL_RenderClear(g_r);
}

void draw(const Texture* t, const SDL_Rect* src, const RectF& dst, float alpha, float angleDeg, Color tint,
          bool flipH) {
  if (!t || alpha <= 0.001f || dbgNoTextures) return;
  SDL_SetTextureAlphaMod(t->tex, (uint8_t)(255 * (alpha > 1 ? 1 : alpha) * tint.a / 255));
  SDL_SetTextureColorMod(t->tex, tint.r, tint.g, tint.b);
  SDL_FRect d{dst.x, dst.y, dst.w, dst.h};
  if (dbgDrawTrace) trace("draw tex %p %dx%d src=%d,%d,%d,%d dst=%.1f,%.1f,%.1f,%.1f a=%.2f", (void*)t->tex, t->w, t->h, src ? src->x : -1,
        src ? src->y : -1, src ? src->w : -1, src ? src->h : -1, d.x, d.y, d.w, d.h, alpha);
  if (g_additive) SDL_SetTextureBlendMode(t->tex, SDL_BLENDMODE_ADD);
  if (dbgNoExCopy && angleDeg == 0 && !flipH) SDL_RenderCopyF(g_r, t->tex, src, &d);
  else SDL_RenderCopyExF(g_r, t->tex, src, &d, angleDeg, nullptr, flipH ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
  if (g_additive) SDL_SetTextureBlendMode(t->tex, SDL_BLENDMODE_BLEND);
}

void drawCentered(const Texture* t, const SDL_Rect* src, float cx, float cy, float w, float h, float alpha,
                  float angleDeg, Color tint) {
  draw(t, src, RectF{cx - w * 0.5f, cy - h * 0.5f, w, h}, alpha, angleDeg, tint);
}

static SDL_Color sc(Color c) { return SDL_Color{c.r, c.g, c.b, c.a}; }

static void geometry(const std::vector<SDL_Vertex>& v, const std::vector<int>& idx) {
  if (dbgNoGeometry) return;
  if (dbgDrawTrace) trace("geom %d verts %d idx", (int)v.size(), (int)idx.size());
  SDL_RenderGeometry(g_r, nullptr, v.data(), (int)v.size(), idx.empty() ? nullptr : idx.data(), (int)idx.size());
}

void fillRect(const RectF& r, Color c) {
  SDL_SetRenderDrawColor(g_r, c.r, c.g, c.b, c.a);
  SDL_FRect f{r.x, r.y, r.w, r.h};
  SDL_RenderFillRectF(g_r, &f);
}

static void addFan(std::vector<SDL_Vertex>& v, std::vector<int>& idx, float cx, float cy, float rad, float a0,
                   float a1, int segs, Color c) {
  int center = (int)v.size();
  v.push_back({{cx, cy}, sc(c), {0, 0}});
  for (int i = 0; i <= segs; i++) {
    float a = a0 + (a1 - a0) * i / segs;
    v.push_back({{cx + std::cos(a) * rad, cy + std::sin(a) * rad}, sc(c), {0, 0}});
    if (i > 0) {
      idx.push_back(center);
      idx.push_back(center + i);
      idx.push_back(center + i + 1);
    }
  }
}

static void addQuad(std::vector<SDL_Vertex>& v, std::vector<int>& idx, SDL_FPoint p[4], Color c0, Color c1,
                    Color c2, Color c3) {
  int b = (int)v.size();
  v.push_back({p[0], sc(c0), {0, 0}});
  v.push_back({p[1], sc(c1), {0, 0}});
  v.push_back({p[2], sc(c2), {0, 0}});
  v.push_back({p[3], sc(c3), {0, 0}});
  int q[6] = {b, b + 1, b + 2, b, b + 2, b + 3};
  idx.insert(idx.end(), q, q + 6);
}

void fillRoundRect(const RectF& r, float rad, Color c) {
  rad = std::min(rad, std::min(r.w, r.h) * 0.5f);
  if (rad < 0.5f) return fillRect(r, c);
  std::vector<SDL_Vertex> v;
  std::vector<int> idx;
  const float PI = 3.14159265f;
  SDL_FPoint mid[4] = {{r.x + rad, r.y}, {r.x + r.w - rad, r.y}, {r.x + r.w - rad, r.y + r.h}, {r.x + rad, r.y + r.h}};
  addQuad(v, idx, mid, c, c, c, c);
  SDL_FPoint left[4] = {{r.x, r.y + rad}, {r.x + rad, r.y + rad}, {r.x + rad, r.y + r.h - rad}, {r.x, r.y + r.h - rad}};
  addQuad(v, idx, left, c, c, c, c);
  SDL_FPoint right[4] = {{r.x + r.w - rad, r.y + rad}, {r.x + r.w, r.y + rad}, {r.x + r.w, r.y + r.h - rad},
                         {r.x + r.w - rad, r.y + r.h - rad}};
  addQuad(v, idx, right, c, c, c, c);
  int segs = 8;
  addFan(v, idx, r.x + rad, r.y + rad, rad, PI, 1.5f * PI, segs, c);
  addFan(v, idx, r.x + r.w - rad, r.y + rad, rad, 1.5f * PI, 2 * PI, segs, c);
  addFan(v, idx, r.x + r.w - rad, r.y + r.h - rad, rad, 0, 0.5f * PI, segs, c);
  addFan(v, idx, r.x + rad, r.y + r.h - rad, rad, 0.5f * PI, PI, segs, c);
  geometry(v, idx);
}

void line(float x1, float y1, float x2, float y2, float width, Color c) {
  float dx = x2 - x1, dy = y2 - y1;
  float len = std::sqrt(dx * dx + dy * dy);
  if (len < 0.001f) return;
  float nx = -dy / len * width * 0.5f, ny = dx / len * width * 0.5f;
  std::vector<SDL_Vertex> v;
  std::vector<int> idx;
  SDL_FPoint p[4] = {{x1 + nx, y1 + ny}, {x2 + nx, y2 + ny}, {x2 - nx, y2 - ny}, {x1 - nx, y1 - ny}};
  addQuad(v, idx, p, c, c, c, c);
  geometry(v, idx);
}

void strokeRoundRect(const RectF& r, float rad, float w, Color c) {
  // Aproximação: segmentos retos e cantos em polilinha.
  const float PI = 3.14159265f;
  std::vector<SDL_FPoint> pts;
  auto arc = [&](float cx, float cy, float a0, float a1) {
    for (int i = 0; i <= 6; i++) {
      float a = a0 + (a1 - a0) * i / 6;
      pts.push_back({cx + std::cos(a) * rad, cy + std::sin(a) * rad});
    }
  };
  arc(r.x + rad, r.y + rad, PI, 1.5f * PI);
  arc(r.x + r.w - rad, r.y + rad, 1.5f * PI, 2 * PI);
  arc(r.x + r.w - rad, r.y + r.h - rad, 0, 0.5f * PI);
  arc(r.x + rad, r.y + r.h - rad, 0.5f * PI, PI);
  for (size_t i = 0; i < pts.size(); i++) {
    auto a = pts[i], b = pts[(i + 1) % pts.size()];
    line(a.x, a.y, b.x, b.y, w, c);
  }
}

void fillCircle(float cx, float cy, float radius, Color c) {
  std::vector<SDL_Vertex> v;
  std::vector<int> idx;
  addFan(v, idx, cx, cy, radius, 0, 6.2831853f, std::max(12, (int)(radius * 0.8f)), c);
  geometry(v, idx);
}

void strokeCircle(float cx, float cy, float radius, float width, Color c) {
  int segs = std::max(16, (int)radius);
  std::vector<SDL_Vertex> v;
  std::vector<int> idx;
  for (int i = 0; i < segs; i++) {
    float a0 = 6.2831853f * i / segs, a1 = 6.2831853f * (i + 1) / segs;
    float ri = radius - width * 0.5f, ro = radius + width * 0.5f;
    SDL_FPoint p[4] = {{cx + std::cos(a0) * ri, cy + std::sin(a0) * ri},
                       {cx + std::cos(a0) * ro, cy + std::sin(a0) * ro},
                       {cx + std::cos(a1) * ro, cy + std::sin(a1) * ro},
                       {cx + std::cos(a1) * ri, cy + std::sin(a1) * ri}};
    addQuad(v, idx, p, c, c, c, c);
  }
  geometry(v, idx);
}

void fillTriangle(float x1, float y1, float x2, float y2, float x3, float y3, Color c) {
  std::vector<SDL_Vertex> v = {{{x1, y1}, sc(c), {0, 0}}, {{x2, y2}, sc(c), {0, 0}}, {{x3, y3}, sc(c), {0, 0}}};
  geometry(v, {});
}

void verticalGradient(const RectF& r, Color top, Color bottom) {
  std::vector<SDL_Vertex> v;
  std::vector<int> idx;
  SDL_FPoint p[4] = {{r.x, r.y}, {r.x + r.w, r.y}, {r.x + r.w, r.y + r.h}, {r.x, r.y + r.h}};
  addQuad(v, idx, p, top, top, bottom, bottom);
  geometry(v, idx);
}

void horizontalGradient(const RectF& r, const std::vector<std::pair<float, Color>>& stops) {
  std::vector<SDL_Vertex> v;
  std::vector<int> idx;
  for (size_t i = 0; i + 1 < stops.size(); i++) {
    float x0 = r.x + r.w * stops[i].first, x1 = r.x + r.w * stops[i + 1].first;
    SDL_FPoint p[4] = {{x0, r.y}, {x1, r.y}, {x1, r.y + r.h}, {x0, r.y + r.h}};
    addQuad(v, idx, p, stops[i].second, stops[i + 1].second, stops[i + 1].second, stops[i].second);
  }
  geometry(v, idx);
}

void nineSlice(const Texture* t, const RectF& d, int s, float b, float alpha) {
  if (!t) return;
  int W = t->w, H = t->h;
  int sx[4] = {0, s, W - s, W}, sy[4] = {0, s, H - s, H};
  float dx[4] = {d.x, d.x + b, d.x + d.w - b, d.x + d.w}, dy[4] = {d.y, d.y + b, d.y + d.h - b, d.y + d.h};
  for (int j = 0; j < 3; j++)
    for (int i = 0; i < 3; i++) {
      if (i == 1 && j == 1) continue;  // o centro do border-image não é desenhado
      SDL_Rect src{sx[i], sy[j], sx[i + 1] - sx[i], sy[j + 1] - sy[j]};
      if (src.w <= 0 || src.h <= 0) continue;
      draw(t, &src, RectF{dx[i], dy[j], dx[i + 1] - dx[i], dy[j + 1] - dy[j]}, alpha);
    }
}

void star(float cx, float cy, float size, Color fill, Color stroke) {
  // Estrela de 5 pontas (mesma forma do SVG do jogo).
  const float PI = 3.14159265f;
  std::vector<SDL_FPoint> p;
  float R = size * 0.5f, r = R * 0.45f;
  for (int i = 0; i < 10; i++) {
    float a = -PI / 2 + i * PI / 5;
    float rad = (i % 2 == 0) ? R : r;
    p.push_back({cx + std::cos(a) * rad, cy + std::sin(a) * rad * 0.95f + size * 0.03f});
  }
  std::vector<SDL_Vertex> v;
  std::vector<int> idx;
  v.push_back({{cx, cy + size * 0.03f}, sc(fill), {0, 0}});
  for (auto& q : p) v.push_back({q, sc(fill), {0, 0}});
  for (int i = 0; i < 10; i++) {
    idx.push_back(0);
    idx.push_back(1 + i);
    idx.push_back(1 + (i + 1) % 10);
  }
  geometry(v, idx);
  for (int i = 0; i < 10; i++) line(p[i].x, p[i].y, p[(i + 1) % 10].x, p[(i + 1) % 10].y, 1.2f, stroke);
}

}  // namespace gfx

// ------------------------------------------------------------------ Texto

namespace text {

uint32_t decodeUtf8(const std::string& s, size_t& i) {
  unsigned char c = (unsigned char)s[i++];
  if (c < 0x80) return c;
  int extra = (c >= 0xF0) ? 3 : (c >= 0xE0) ? 2 : (c >= 0xC0) ? 1 : 0;
  uint32_t cp = c & (0x3F >> extra);
  for (int k = 0; k < extra && i < s.size(); k++) cp = (cp << 6) | ((unsigned char)s[i++] & 0x3F);
  return cp;
}

static void appendUtf8(std::string& o, uint32_t cp) {
  if (cp < 0x80) o += (char)cp;
  else if (cp < 0x800) { o += (char)(0xC0 | (cp >> 6)); o += (char)(0x80 | (cp & 0x3F)); }
  else if (cp < 0x10000) { o += (char)(0xE0 | (cp >> 12)); o += (char)(0x80 | ((cp >> 6) & 0x3F)); o += (char)(0x80 | (cp & 0x3F)); }
  else { o += (char)(0xF0 | (cp >> 18)); o += (char)(0x80 | ((cp >> 12) & 0x3F)); o += (char)(0x80 | ((cp >> 6) & 0x3F)); o += (char)(0x80 | (cp & 0x3F)); }
}

std::string upper(const std::string& s) {
  std::string o;
  for (size_t i = 0; i < s.size();) {
    uint32_t cp = decodeUtf8(s, i);
    if (cp >= 'a' && cp <= 'z') cp -= 32;
    else if ((cp >= 0xE0 && cp <= 0xFE && cp != 0xF7)) cp -= 32;
    else if (cp >= 0x100 && cp <= 0x17F && (cp & 1)) cp -= 1;
    appendUtf8(o, cp);
  }
  return o;
}

}  // namespace text


// Os atlas são criados fora do desenho de um frame: o Vita3K falha se uma textura for criada a meio
// de uma cena em que já foram usadas outras texturas.
void Font::newAtlas() {
  SDL_Texture* t = SDL_CreateTexture(gfx::renderer(), SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, atlasSize_, atlasSize_);
  if (!t) gfx::trace("atlas create failed: %s", SDL_GetError());
  SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
  Atlas a{t, 0, 0, 0};
  a.pixels.assign(atlasSize_ * atlasSize_, 0x00FFFFFF);
  SDL_UpdateTexture(t, nullptr, a.pixels.data(), atlasSize_ * 4);
  atlases_.push_back(std::move(a));
}

// Envia para a GPU os glifos novos. Chamado no início de cada frame: atualizar uma textura a meio do
// desenho obriga o SDL a submeter os comandos pendentes, e isso faz o Vita3K falhar.
void Font::flushUploads() {
  for (auto& a : atlases_) {
    if (a.dirtyY1 <= a.dirtyY0) continue;
    // Envia o atlas inteiro: as atualizações parciais (só algumas linhas) corrompem glifos na Vita/Vita3K.
    SDL_UpdateTexture(a.tex, nullptr, a.pixels.data(), atlasSize_ * 4);
    a.dirtyY0 = atlasSize_;
    a.dirtyY1 = 0;
  }
}

bool Font::load(const std::string& path, int atlasSize) {
  atlasSize_ = atlasSize;
  if (!plat::readFile(path, data_)) return false;
  auto* info = new stbtt_fontinfo;
  if (!stbtt_InitFont(info, data_.data(), stbtt_GetFontOffsetForIndex(data_.data(), 0))) {
    delete info;
    data_.clear();
    return false;
  }
  info_ = info;
  int a, d, g;
  stbtt_GetFontVMetrics(info, &a, &d, &g);
  float s = stbtt_ScaleForMappingEmToPixels(info, 1.f);
  ascent_ = a * s;
  descent_ = d * s;
  lineGap_ = g * s;
  newAtlas();
  return true;
}

float Font::lineHeight(float px) const { return (ascent_ - descent_) * px; }

const Font::Glyph& Font::glyph(uint32_t cp, int px) {
  uint64_t key = ((uint64_t)px << 32) | cp;
  auto it = glyphs_.find(key);
  if (it != glyphs_.end()) return it->second;
  auto* info = (stbtt_fontinfo*)info_;
  Glyph g;
  float scale = stbtt_ScaleForMappingEmToPixels(info, (float)px);
  int adv, lsb;
  stbtt_GetCodepointHMetrics(info, cp, &adv, &lsb);
  g.advance = adv * scale;
  int x0, y0, x1, y1;
  stbtt_GetCodepointBitmapBox(info, cp, scale, scale, &x0, &y0, &x1, &y1);
  int w = x1 - x0, h = y1 - y0;
  g.xoff = (float)x0;
  g.yoff = (float)y0;
  if (w > 0 && h > 0) {
    if (atlases_.empty()) newAtlas();
    if (atlases_[cur_].x + w + 1 > atlasSize_) {
      auto& A = atlases_[cur_];
      A.x = 0;
      A.y += A.rowH + 1;
      A.rowH = 0;
    }
    if (atlases_[cur_].y + h + 1 > atlasSize_) {
      cur_++;
      if (cur_ >= (int)atlases_.size()) newAtlas();
    }
    auto& A = atlases_[cur_];
    std::vector<uint8_t> bmp(w * h);
    stbtt_MakeCodepointBitmap(info, bmp.data(), w, h, w, scale, scale, cp);
    std::vector<uint32_t> rgba(w * h);
    for (int i = 0; i < w * h; i++) rgba[i] = ((uint32_t)bmp[i] << 24) | 0x00FFFFFF;
    SDL_Rect r{A.x, A.y, w, h};
    for (int yy = 0; yy < h; yy++)
      memcpy(&A.pixels[(r.y + yy) * atlasSize_ + r.x], &rgba[yy * w], w * 4);
    A.dirtyY0 = std::min(A.dirtyY0, r.y);
    A.dirtyY1 = std::max(A.dirtyY1, r.y + h);
    g.atlas = cur_;
    g.rect = r;
    A.x += w + 1;
    A.rowH = std::max(A.rowH, h);
  }
  return glyphs_.emplace(key, g).first->second;
}

float Font::kern(uint32_t a, uint32_t b, float px) const {
  auto* info = (stbtt_fontinfo*)info_;
  return stbtt_GetCodepointKernAdvance(info, a, b) * stbtt_ScaleForMappingEmToPixels(info, px);
}

float Font::measure(const std::string& s, float px) const {
  if (!info_) return 0;
  auto* info = (stbtt_fontinfo*)info_;
  float scale = stbtt_ScaleForMappingEmToPixels(info, px);
  float w = 0;
  uint32_t prev = 0;
  for (size_t i = 0; i < s.size();) {
    uint32_t cp = text::decodeUtf8(s, i);
    int adv, lsb;
    stbtt_GetCodepointHMetrics(info, cp, &adv, &lsb);
    if (prev) w += kern(prev, cp, px);
    w += adv * scale;
    prev = cp;
  }
  return w;
}

void Font::draw(const std::string& s, float x, float y, float px, Color c, Align align, float alpha) {
  if (!info_ || alpha <= 0.001f || gfx::dbgNoText) return;
  int ipx = (int)std::lround(px);
  if (ipx < 4) return;
  float w = measure(s, (float)ipx);
  if (align == Align::Center) x -= w * 0.5f;
  else if (align == Align::Right) x -= w;
  float baseline = y + ascent_ * ipx;
  uint32_t prev = 0;
  uint8_t a = (uint8_t)(c.a * (alpha > 1 ? 1 : alpha));
  for (size_t i = 0; i < s.size();) {
    uint32_t cp = text::decodeUtf8(s, i);
    if (prev) x += kern(prev, cp, (float)ipx);
    const Glyph& g = glyph(cp, ipx);
    if (g.atlas >= 0) {
      SDL_Texture* t = atlases_[g.atlas].tex;
      SDL_SetTextureColorMod(t, c.r, c.g, c.b);
      SDL_SetTextureAlphaMod(t, a);
      SDL_FRect d{x + g.xoff, baseline + g.yoff, (float)g.rect.w, (float)g.rect.h};
      if (gfx::dbgDrawTrace) gfx::trace("glyph %u atlas %d src=%d,%d,%d,%d", cp, g.atlas, g.rect.x, g.rect.y, g.rect.w, g.rect.h);
      SDL_RenderCopyF(gfx::renderer(), t, &g.rect, &d);
    }
    x += g.advance;
    prev = cp;
  }
}

std::vector<std::string> Font::wrap(const std::string& s, float px, float maxWidth) const {
  std::vector<std::string> lines;
  size_t start = 0;
  while (start <= s.size()) {
    size_t nl = s.find('\n', start);
    std::string para = s.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
    std::string cur;
    size_t i = 0;
    while (i < para.size()) {
      size_t sp = para.find(' ', i);
      std::string word = para.substr(i, sp == std::string::npos ? std::string::npos : sp - i);
      std::string trial = cur.empty() ? word : cur + " " + word;
      if (!cur.empty() && measure(trial, px) > maxWidth) {
        lines.push_back(cur);
        cur = word;
      } else {
        cur = trial;
      }
      if (sp == std::string::npos) break;
      i = sp + 1;
    }
    lines.push_back(cur);
    if (nl == std::string::npos) break;
    start = nl + 1;
  }
  return lines;
}

float Font::drawBox(const std::string& s, const RectF& box, float px, float minPx, Color c, Align align,
                    float alpha, float spacing) {
  std::vector<std::string> lines;
  for (;;) {
    lines = wrap(s, px, box.w);
    float maxW = 0;
    for (auto& l : lines) maxW = std::max(maxW, measure(l, px));
    float h = lines.size() * lineHeight(px) * spacing;
    if ((h <= box.h && maxW <= box.w) || px <= minPx) break;
    px -= 1;
  }
  float lh = lineHeight(px) * spacing;
  float total = lines.size() * lh;
  float y = box.y + (box.h - total) * 0.5f + (lh - lineHeight(px)) * 0.5f;
  for (auto& l : lines) {
    float x = align == Align::Center ? box.cx() : align == Align::Right ? box.x + box.w : box.x;
    draw(l, x, y, px, c, align, alpha);
    y += lh;
  }
  return px;
}

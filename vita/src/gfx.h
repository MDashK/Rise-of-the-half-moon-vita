#pragma once
#include <SDL2/SDL.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct Color {
  uint8_t r = 255, g = 255, b = 255, a = 255;
  constexpr Color() = default;
  constexpr Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255) : r(r_), g(g_), b(b_), a(a_) {}
  static Color hex(uint32_t rgb, uint8_t a = 255) { return Color((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, a); }
  Color withAlpha(float f) const { return Color(r, g, b, (uint8_t)(a * (f < 0 ? 0 : f > 1 ? 1 : f))); }
};

struct RectF {
  float x = 0, y = 0, w = 0, h = 0;
  bool contains(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; }
  float cx() const { return x + w * 0.5f; }
  float cy() const { return y + h * 0.5f; }
  RectF inset(float d) const { return {x + d, y + d, w - 2 * d, h - 2 * d}; }
};

class Texture {
 public:
  Texture(SDL_Texture* t, int w, int h) : tex(t), w(w), h(h) {}
  ~Texture();
  SDL_Texture* tex;
  int w, h;
};
using TexPtr = std::shared_ptr<Texture>;

// Frame de uma folha de sprites, no formato do jogo original: [folha, x, y, w, h].
struct Frame {
  int sheet = -1, x = 0, y = 0, w = 0, h = 0;
  bool valid() const { return sheet >= 0 && w > 0; }
};

enum class Align { Left, Center, Right };

namespace gfx {

// Diagnóstico (ux0:/data/rise_half_moon/debug.txt): desliga partes do desenho.
extern bool dbgNoGeometry, dbgNoTarget, dbgNoText, dbgNoTextures, dbgTrace, dbgNoExCopy, dbgDrawTrace;
void trace(const char* fmt, ...);
extern std::vector<std::string> traceLines;
extern int traceWarnings;  // últimas linhas (mostradas no ecrã)

bool init(SDL_Renderer* r);
void shutdown();
SDL_Renderer* renderer();

TexPtr loadTexture(const std::string& path);   // PNG/JPEG via stb_image
// Mistura aditiva (para simular o "brightness()" do CSS).
void setAdditive(bool on);
TexPtr loadTextureMem(const void* data, size_t size);
// Carrega uma imagem aplicando uma transformação aos píxeis RGBA (ex.: filtro de contraste).
TexPtr loadTextureTransformed(const std::string& path, void (*fn)(uint8_t* rgba, int count));

void setTarget(SDL_Texture* t);
void clear(Color c);

// Desenha a região src da textura no retângulo dst (coordenadas do ecrã virtual).
void draw(const Texture* t, const SDL_Rect* src, const RectF& dst, float alpha = 1.f,
          float angleDeg = 0.f, Color tint = Color(), bool flipH = false);
// Desenha centrado em (cx, cy) com escala.
void drawCentered(const Texture* t, const SDL_Rect* src, float cx, float cy, float w, float h,
                  float alpha = 1.f, float angleDeg = 0.f, Color tint = Color());
void fillRect(const RectF& r, Color c);
void fillRoundRect(const RectF& r, float radius, Color c);
void strokeRoundRect(const RectF& r, float radius, float width, Color c);
void line(float x1, float y1, float x2, float y2, float width, Color c);
void fillCircle(float cx, float cy, float radius, Color c);
void strokeCircle(float cx, float cy, float radius, float width, Color c);
void fillTriangle(float x1, float y1, float x2, float y2, float x3, float y3, Color c);
void verticalGradient(const RectF& r, Color top, Color bottom);
void horizontalGradient(const RectF& r, const std::vector<std::pair<float, Color>>& stops);
// Moldura com imagem (border-image do CSS): corta a imagem em 9 partes.
void nineSlice(const Texture* t, const RectF& dst, int slice, float border, float alpha = 1.f);
void star(float cx, float cy, float size, Color fill, Color stroke);

}  // namespace gfx

// ------------------------------------------------------------------ Texto

class Font {
 public:
  bool load(const std::string& path, int atlasSize = 1024);
  bool loaded() const { return !data_.empty(); }
  void flushUploads();
  float measure(const std::string& utf8, float px) const;
  float lineHeight(float px) const;
  void draw(const std::string& utf8, float x, float y, float px, Color c, Align align = Align::Left,
            float alpha = 1.f);
  // Quebra o texto em linhas que cabem em maxWidth.
  std::vector<std::string> wrap(const std::string& utf8, float px, float maxWidth) const;
  // Escreve texto com quebras de linha centrado no retângulo; reduz o tamanho até caber
  // (como a função K() do jogo original). Devolve o tamanho usado.
  float drawBox(const std::string& utf8, const RectF& box, float px, float minPx, Color c,
                Align align = Align::Center, float alpha = 1.f, float lineSpacing = 1.15f);

 private:
  struct Glyph {
    int atlas = -1;
    SDL_Rect rect{};
    float xoff = 0, yoff = 0, advance = 0;
  };
  const Glyph& glyph(uint32_t cp, int px);
  float kern(uint32_t a, uint32_t b, float px) const;
  void newAtlas();

  std::vector<uint8_t> data_;
  void* info_ = nullptr;  // stbtt_fontinfo
  std::unordered_map<uint64_t, Glyph> glyphs_;
  struct Atlas {
    SDL_Texture* tex;
    int x, y, rowH;
    std::vector<uint32_t> pixels;
    int dirtyY0 = 1 << 30, dirtyY1 = 0;
  };
  std::vector<Atlas> atlases_;
  int cur_ = 0;
  int atlasSize_ = 1024;
  float ascent_ = 0, descent_ = 0, lineGap_ = 0;
};

namespace text {
uint32_t decodeUtf8(const std::string& s, size_t& i);
std::string upper(const std::string& utf8);
}  // namespace text

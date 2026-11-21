#pragma once
#include <functional>
#include <memory>
#include <string>

#include "gfx.h"

struct Touch {
  enum Kind { Down, Move, Up } kind;
  float x, y;  // coordenadas do ecrã virtual (canvas)
  long long id;
};

class Scene {
 public:
  virtual ~Scene() = default;
  virtual void update(float dtMs) = 0;
  virtual void render() = 0;
  virtual void touch(const Touch&) {}
  virtual void back() {}                 // botão "círculo" / Escape
  virtual bool usesGameOrientation() const { return false; }  // false: sempre horizontal (menu)
};

enum class Orientation { Landscape = 0, Portrait = 1, PortraitFlipped = 2 };

struct Settings {
  std::string language = "en";
  Orientation orientation = Orientation::Landscape;
  bool muted = false;
  void load();
  void save() const;
};

enum class FontKind { Light, Regular, Bold, LightItalic };

// Opções de diagnóstico (ux0:/data/rise_half_moon/debug.txt).
namespace dbg {
extern bool autoplay;    // o jogador também é controlado pela IA
extern float timeScale;  // acelera o tempo do jogo
extern std::string flags;
inline bool has(const char* f) { return flags.find(f) != std::string::npos; }
}  // namespace dbg

namespace app {

Settings& settings();
Font& font(FontKind k = FontKind::Light);

// Tamanho do canvas da cena atual: 960x544 (horizontal) ou 540x960 (vertical).
float width();
float height();
bool portrait();

void setScene(std::unique_ptr<Scene> s);  // aplicado no início do próximo frame
void quit();

// Executa uma tarefa numa thread em segundo plano (ex.: descodificar áudio).
void runAsync(std::function<void()> fn);

// Texturas partilhadas da interface.
TexPtr uiTexture(const std::string& path);

}  // namespace app

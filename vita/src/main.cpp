// Rise of the Half Moon — versão nativa para PS Vita (e PC, para testes).
#include <SDL2/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>

#include "app.h"
#include "audio.h"
#include "json.h"
#include "platform.h"
#include "scenes.h"
#include "stb_image_write.h"

namespace dbg {
bool autoplay = false;
float timeScale = 1.f;
std::string flags;
}  // namespace dbg

namespace {

const int SCREEN_W = 960, SCREEN_H = 544;

SDL_Window* g_window = nullptr;
SDL_Renderer* g_renderer = nullptr;
SDL_Texture* g_canvas = nullptr;
int g_canvasW = 0, g_canvasH = 0;
std::unique_ptr<Scene> g_scene, g_next;
bool g_quit = false;
Settings g_settings;
Font g_fonts[4];
std::map<std::string, TexPtr> g_ui;

Orientation currentOrientation() {
  if (!g_scene || !g_scene->usesGameOrientation()) return Orientation::Landscape;
  return g_settings.orientation;
}

void ensureCanvas() {
  bool p = currentOrientation() != Orientation::Landscape;
  int w = p ? 540 : SCREEN_W, h = p ? 960 : SCREEN_H;
  if (g_canvas && w == g_canvasW && h == g_canvasH) return;
  if (g_canvas) SDL_DestroyTexture(g_canvas);
  g_canvas = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, w, h);
  SDL_SetTextureBlendMode(g_canvas, SDL_BLENDMODE_NONE);
  g_canvasW = w;
  g_canvasH = h;
}

// Converte coordenadas do ecrã físico (960x544) para o canvas da cena.
void screenToCanvas(float sx, float sy, float& cx, float& cy) {
  switch (currentOrientation()) {
    case Orientation::Landscape:
      cx = sx * g_canvasW / SCREEN_W;
      cy = sy * g_canvasH / SCREEN_H;
      break;
    case Orientation::Portrait:  // topo do jogo à esquerda do ecrã
      cy = sx;
      cx = 540.f - (sy - 2.f);
      break;
    case Orientation::PortraitFlipped:  // topo do jogo à direita do ecrã
      cy = 960.f - sx;
      cx = sy - 2.f;
      break;
  }
}

void present() {
  gfx::setTarget(nullptr);
  SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
  SDL_RenderClear(g_renderer);
  Orientation o = currentOrientation();
  if (o == Orientation::Landscape) {
    SDL_RenderCopy(g_renderer, g_canvas, nullptr, nullptr);
  } else {
    SDL_Rect dst{SCREEN_W / 2 - 270, SCREEN_H / 2 - 480, 540, 960};
    SDL_RenderCopyEx(g_renderer, g_canvas, nullptr, &dst, o == Orientation::Portrait ? -90.0 : 90.0, nullptr,
                     SDL_FLIP_NONE);
  }
}

void dispatchTouch(Touch::Kind kind, float sx, float sy, long long id) {
  if (!g_scene) return;
  Touch t{kind, 0, 0, id};
  screenToCanvas(sx, sy, t.x, t.y);
  g_scene->touch(t);
}

// ---- Testes automáticos no PC: RHM_SCRIPT=ficheiro com comandos
//   wait <ms> | tap <x> <y> | down/move/up <x> <y> | shot <ficheiro.png> | quit
struct ScriptCmd {
  std::string op;
  float a = 0, b = 0;
  std::string s;
};
std::vector<ScriptCmd> g_script;
size_t g_scriptPos = 0;
float g_scriptWait = 0;

void loadScript() {
  const char* p = getenv("RHM_SCRIPT");
  if (!p) return;
  std::ifstream in(p);
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream ss(line);
    ScriptCmd c;
    ss >> c.op;
    if (c.op.empty() || c.op[0] == '#') continue;
    if (c.op == "shot") ss >> c.s;
    else ss >> c.a >> c.b;
    g_script.push_back(c);
  }
}

void screenshot(const std::string& file) {
  std::vector<uint8_t> px(SCREEN_W * SCREEN_H * 4);
  SDL_RenderReadPixels(g_renderer, nullptr, SDL_PIXELFORMAT_ABGR8888, px.data(), SCREEN_W * 4);
  stbi_write_png(file.c_str(), SCREEN_W, SCREEN_H, 4, px.data(), SCREEN_W * 4);
}

void runScript(float dt) {
  if (g_scriptPos >= g_script.size()) return;
  if (g_scriptWait > 0) {
    g_scriptWait -= dt;
    return;
  }
  auto& c = g_script[g_scriptPos++];
  if (c.op == "wait") g_scriptWait = c.a;
  else if (c.op == "tap") {
    dispatchTouch(Touch::Down, c.a, c.b, 1);
    dispatchTouch(Touch::Up, c.a, c.b, 1);
  } else if (c.op == "down") dispatchTouch(Touch::Down, c.a, c.b, 1);
  else if (c.op == "move") dispatchTouch(Touch::Move, c.a, c.b, 1);
  else if (c.op == "up") dispatchTouch(Touch::Up, c.a, c.b, 1);
  else if (c.op == "shot") screenshot(c.s);
  else if (c.op == "quit") g_quit = true;
}

}  // namespace

// ------------------------------------------------------------------ API da app

void Settings::load() {
  std::string t;
  if (!plat::readText(plat::dataDir() + "settings.json", t)) return;
  Json j = Json::parse(t);
  language = j["language"].str("en");
  orientation = (Orientation)std::max(0, std::min(2, j["orientation"].integer(0)));
  muted = j["muted"].boolean(false);
}

void Settings::save() const {
  Json j = Json::object();
  j.set("language", Json(language));
  j.set("orientation", Json((double)(int)orientation));
  j.set("muted", Json(muted));
  std::string s = j.dump();
  plat::writeFile(plat::dataDir() + "settings.json", s.data(), s.size());
}

namespace app {
Settings& settings() { return g_settings; }
Font& font(FontKind k) { return g_fonts[(int)k]; }
float width() { return (float)g_canvasW; }
float height() { return (float)g_canvasH; }
bool portrait() { return g_canvasH > g_canvasW; }
void setScene(std::unique_ptr<Scene> s) { g_next = std::move(s); }
void quit() { g_quit = true; }

static int asyncThunk(void* p) {
  auto* fn = (std::function<void()>*)p;
  (*fn)();
  delete fn;
  return 0;
}
void runAsync(std::function<void()> fn) {
  SDL_Thread* t = SDL_CreateThreadWithStackSize(asyncThunk, "rhm-async", 256 * 1024,
                                                new std::function<void()>(std::move(fn)));
  if (t) SDL_DetachThread(t);
}

TexPtr uiTexture(const std::string& path) {
  auto it = g_ui.find(path);
  if (it != g_ui.end()) return it->second;
  auto t = gfx::loadTexture(path);
  if (t) g_ui[path] = t;
  return t;
}
}  // namespace app

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS) != 0) {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return 1;
  }
  plat::init();
  g_settings.load();
  {
    std::string dbg;
    if (plat::readText(plat::dataDir() + "debug.txt", dbg)) {
      gfx::dbgNoGeometry = dbg.find("nogeometry") != std::string::npos;
      gfx::dbgNoTarget = dbg.find("notarget") != std::string::npos;
      gfx::dbgNoText = dbg.find("notext") != std::string::npos;
      gfx::dbgNoTextures = dbg.find("notextures") != std::string::npos;
      gfx::dbgTrace = dbg.find("trace") != std::string::npos;
      gfx::dbgNoExCopy = dbg.find("noexcopy") != std::string::npos;
      gfx::dbgDrawTrace = dbg.find("drawtrace") != std::string::npos;
      dbg::flags = dbg;
      dbg::autoplay = dbg.find("autoplay") != std::string::npos;
      if (dbg.find("fast") != std::string::npos) dbg::timeScale = 4.f;
    }
  }
  audio::setMuted(g_settings.muted);

  g_window = SDL_CreateWindow("Rise of the Half Moon", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W,
                              SCREEN_H, 0);
  g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC |
                                                    SDL_RENDERER_TARGETTEXTURE);
  if (!g_renderer) g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE | SDL_RENDERER_TARGETTEXTURE);
  gfx::init(g_renderer);
  audio::init();
  if (SDL_NumJoysticks() > 0) SDL_JoystickOpen(0);

  g_fonts[0].load(plat::assetDir() + "fonts/JosefinSans-200.ttf");
  g_fonts[1].load(plat::assetDir() + "fonts/JosefinSans-400.ttf");
  g_fonts[2].load(plat::assetDir() + "fonts/JosefinSans-600.ttf");
  g_fonts[3].load(plat::assetDir() + "fonts/JosefinSans-200italic.ttf", 512);

  loadScript();
  g_scene = makeMenuScene();
  // Testes: "game=<mês>" no debug.txt abre logo esse jogo.
  if (auto p = dbg::flags.find("game="); p != std::string::npos) {
    std::string id = dbg::flags.substr(p + 5, 3);
    for (auto& g : catalog::games())
      if (g.id == id && catalog::isAvailable(g)) g_scene = makeGameScene(g);
  }
  uint64_t last = plat::nowMs();

  while (!g_quit) {
    if (g_next) g_scene = std::move(g_next);
    ensureCanvas();

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_QUIT: g_quit = true; break;
        case SDL_FINGERDOWN:
        case SDL_FINGERMOTION:
        case SDL_FINGERUP:
          if (gfx::dbgDrawTrace) gfx::trace("finger type=%x touch=%lld x=%.3f y=%.3f", e.type, (long long)e.tfinger.touchId, e.tfinger.x,
                     e.tfinger.y);
#ifdef RHM_VITA
          // Ignora o painel traseiro (no SDL da Vita o ecrã frontal é o primeiro dispositivo de toque).
          if (SDL_GetNumTouchDevices() > 1 && e.tfinger.touchId != SDL_GetTouchDevice(0)) break;
#endif
          dispatchTouch(e.type == SDL_FINGERDOWN ? Touch::Down : e.type == SDL_FINGERUP ? Touch::Up : Touch::Move,
                        e.tfinger.x * SCREEN_W, e.tfinger.y * SCREEN_H, (long long)e.tfinger.fingerId);
          break;
#ifndef RHM_VITA
        case SDL_MOUSEBUTTONDOWN: dispatchTouch(Touch::Down, (float)e.button.x, (float)e.button.y, -1); break;
        case SDL_MOUSEBUTTONUP: dispatchTouch(Touch::Up, (float)e.button.x, (float)e.button.y, -1); break;
        case SDL_MOUSEMOTION:
          if (e.motion.state & SDL_BUTTON_LMASK) dispatchTouch(Touch::Move, (float)e.motion.x, (float)e.motion.y, -1);
          break;
        case SDL_KEYDOWN:
          if (e.key.keysym.sym == SDLK_ESCAPE && g_scene) g_scene->back();
          break;
#endif
        case SDL_JOYBUTTONDOWN:
          if (e.jbutton.button == 11 && g_scene) g_scene->back();  // só o Start (o círculo não faz nada)
          break;
      }
    }

    uint64_t now = plat::nowMs();
    float dt = (float)(now - last);
    last = now;
    if (dt > 100) dt = 100;
    if (getenv("RHM_SCRIPT")) dt = 1000.f / 60.f;  // tempo determinístico nos testes
    dt *= dbg::timeScale;

    if (g_scene) g_scene->update(dt);
    if (g_next) continue;

    if (gfx::dbgDrawTrace) gfx::trace("--- frame start");
    for (auto& f : g_fonts) f.flushUploads();
    if (gfx::dbgNoTarget) {
      gfx::setTarget(nullptr);
      gfx::clear(Color(0, 0, 0));
      if (g_scene) g_scene->render();
    } else {
      gfx::setTarget(g_canvas);
      gfx::clear(Color(0, 0, 0));
      if (g_scene) g_scene->render();
      present();
    }
    if (gfx::dbgTrace && !gfx::traceLines.empty()) {
      gfx::setTarget(nullptr);
      float y = 4;
      gfx::fillRect({0, 0, 640, 4 + 17.f * gfx::traceLines.size()}, Color(0, 0, 0, 170));
      for (auto& l : gfx::traceLines) {
        g_fonts[1].draw(l, 6, y, 15, Color(255, 255, 120));
        y += 17;
      }
      if (gfx::traceWarnings)
        g_fonts[2].draw("WARNINGS: " + std::to_string(gfx::traceWarnings), 660, 6, 20, Color(255, 80, 80));
    }
    runScript(dt);
    SDL_RenderPresent(g_renderer);
  }

  g_scene.reset();
  g_ui.clear();
  audio::shutdown();
  plat::shutdown();
  SDL_Quit();
  return 0;
}

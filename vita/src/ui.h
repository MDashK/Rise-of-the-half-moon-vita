#pragma once
#include <functional>
#include <string>
#include <vector>

#include "app.h"
#include "gfx.h"

namespace ui {

// Paleta da interface (menu).
const Color kInk = Color::hex(0xF6E7DA);
const Color kInkMuted = Color::hex(0xC9A9A6);
const Color kAccent = Color::hex(0xE8B48C);
const Color kCard = Color::hex(0x3A2236);
const Color kCardHi = Color::hex(0x4A2B44);
const Color kBorder = Color::hex(0x7A4A5C);
const Color kButton = Color::hex(0x55304A);
const Color kButtonHi = Color::hex(0x6D3D5C);
const Color kGood = Color::hex(0xB8E0B0);
const Color kWarn = Color::hex(0xE8B48C);

// Botão tocável. O toque só "conta" se começar e acabar dentro do botão.
struct Button {
  RectF rect;
  std::string label;
  bool pressed = false;
  bool enabled = true;
  bool selected = false;
  std::function<void()> onTap;
  float padHit = 8;  // área de toque extra à volta (dedos!)

  bool hit(float x, float y) const { return enabled && rect.inset(-padHit).contains(x, y); }
};

// Gere uma lista de botões: chama onTap no toque e mantém o estado "premido".
class ButtonSet {
 public:
  std::vector<Button*> buttons;
  bool touch(const Touch& t);  // devolve true se algum botão tratou o toque
 private:
  Button* active_ = nullptr;
};

void drawPill(const Button& b, float fontPx = 20, Color fill = kButton, Color fillHi = kButtonHi,
              Color text = kInk);
void drawPanel(const RectF& r, Color fill, Color border, float radius = 16);
// Uma só linha: reduz o tamanho da letra até o texto caber na largura.
void drawFitted(const std::string& s, const RectF& r, float px, Color c, float alpha = 1.f, Align align = Align::Center,
                FontKind kind = FontKind::Regular);

}  // namespace ui

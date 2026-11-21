#include "ui.h"

#include <algorithm>
#include <cmath>

namespace ui {

bool ButtonSet::touch(const Touch& t) {
  switch (t.kind) {
    case Touch::Down:
      active_ = nullptr;
      for (auto* b : buttons)
        if (b->hit(t.x, t.y)) {
          active_ = b;
          b->pressed = true;
          return true;
        }
      return false;
    case Touch::Move:
      if (active_) {
        active_->pressed = active_->hit(t.x, t.y);
        return true;
      }
      return false;
    case Touch::Up:
      if (active_) {
        Button* b = active_;
        active_ = nullptr;
        bool inside = b->hit(t.x, t.y);
        b->pressed = false;
        if (inside && b->onTap) b->onTap();
        return true;
      }
      return false;
  }
  return false;
}

void drawPill(const Button& b, float px, Color fill, Color fillHi, Color text) {
  float a = b.enabled ? 1.f : 0.45f;
  Color f = (b.pressed || b.selected) ? fillHi : fill;
  RectF r = b.rect;
  if (b.pressed) r = r.inset(2);
  gfx::fillRoundRect(r, r.h * 0.5f, f.withAlpha(a));
  gfx::strokeRoundRect(r, r.h * 0.5f, 1.5f, (b.selected ? kAccent : kBorder).withAlpha(a));
  drawFitted(b.label, r.inset(8), px, text, a);
}

void drawFitted(const std::string& s, const RectF& r, float px, Color c, float alpha, Align align, FontKind kind) {
  Font& f = app::font(kind);
  float w = f.measure(s, px);
  if (w > r.w && w > 0) px = std::max(10.f, std::floor(px * r.w / w));
  float y = r.cy() - f.lineHeight(px) * 0.5f;
  float x = align == Align::Center ? r.cx() : align == Align::Right ? r.x + r.w : r.x;
  f.draw(s, x, y, px, c, align, alpha);
}

void drawPanel(const RectF& r, Color fill, Color border, float radius) {
  gfx::fillRoundRect(r, radius, fill);
  gfx::strokeRoundRect(r, radius, 1.5f, border);
}

}  // namespace ui

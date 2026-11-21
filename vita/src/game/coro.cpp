#include "coro.h"

#include <algorithm>
#include <cmath>

namespace co {

Sched*& current() {
  static Sched* s = nullptr;
  return s;
}

void Sched::tick(float dtMs) {
  Sched* prev = current();
  current() = this;
  now_ += dtMs;
  dt_ = dtMs;
  std::vector<std::coroutine_handle<>> ready;
  ready.swap(frames_);
  for (size_t i = 0; i < timed_.size();) {
    if (timed_[i].wake <= now_) {
      ready.push_back(timed_[i].h);
      timed_.erase(timed_.begin() + i);
    } else {
      i++;
    }
  }
  for (size_t i = 0; i < preds_.size();) {
    if (preds_[i].first()) {
      ready.push_back(preds_[i].second);
      preds_.erase(preds_.begin() + i);
    } else {
      i++;
    }
  }
  for (auto h : ready) {
    if (clearing_) break;
    h.resume();
  }
  roots_.erase(std::remove_if(roots_.begin(), roots_.end(), [](const Task<>& t) { return t.done(); }), roots_.end());
  current() = prev;
}

void Sched::spawn(Task<> t) {
  if (!t.h_) return;
  Sched* prev = current();
  current() = this;
  auto h = t.h_;
  roots_.push_back(std::move(t));
  h.resume();
  current() = prev;
}

void Sched::clear() {
  clearing_ = true;
  timed_.clear();
  frames_.clear();
  preds_.clear();
  roots_.clear();  // destrói as corrotinas (e as filhas que estavam a aguardar)
  clearing_ = false;
}

float linear(float t) { return t; }
float easeOut(float t) { return 1 - (1 - t) * (1 - t) * (1 - t); }
float easeIn(float t) { return t * t * t; }
float easeInOut(float t) { return t < 0.5f ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3.f) / 2; }
float backOut(float t) {
  const float c1 = 1.70158f, c3 = c1 + 1;
  return 1 + c3 * std::pow(t - 1, 3.f) + c1 * std::pow(t - 1, 2.f);
}

Task<> tween(float ms, std::function<void(float)> f, Ease e) {
  if (ms <= 0) {
    f(1.f);
    co_return;
  }
  float t = 0;
  f(e(0.f));
  while (t < ms) {
    t += co_await frame();
    f(e(std::min(1.f, t / ms)));
  }
}

static Task<> counted(Task<> t, std::shared_ptr<int> n) {
  co_await t;
  (*n)--;
}

Task<> all(std::vector<Task<>> tasks) {
  auto n = std::make_shared<int>((int)tasks.size());
  for (auto& t : tasks) spawn(counted(std::move(t), n));
  co_await until([n] { return *n <= 0; });
}

}  // namespace co

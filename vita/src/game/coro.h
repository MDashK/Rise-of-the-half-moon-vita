#pragma once
// Corrotinas C++20 para reproduzir os fluxos do jogo original (geradores JS com "yield"):
//   co_await sleep(500); co_await tween(300, f); co_await all({a(), b()}); spawn(c());
#include <coroutine>
#include <exception>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace co {

class Sched;
Sched*& current();  // agendador ativo (um por cena de jogo)

struct PromiseBase {
  std::coroutine_handle<> continuation;
  bool finished = false;
  std::suspend_always initial_suspend() noexcept { return {}; }
  struct Final {
    bool await_ready() noexcept { return false; }
    template <class P>
    std::coroutine_handle<> await_suspend(std::coroutine_handle<P> h) noexcept {
      auto& p = h.promise();
      p.finished = true;
      if (p.continuation) return p.continuation;
      return std::noop_coroutine();
    }
    void await_resume() noexcept {}
  };
  Final final_suspend() noexcept { return {}; }
  void unhandled_exception() { std::terminate(); }
};

template <class T = void>
class Task {
 public:
  struct promise_type : PromiseBase {
    T value{};
    Task get_return_object() { return Task(std::coroutine_handle<promise_type>::from_promise(*this)); }
    void return_value(T v) { value = std::move(v); }
  };
  using handle = std::coroutine_handle<promise_type>;
  explicit Task(handle h) : h_(h) {}
  Task(Task&& o) noexcept : h_(std::exchange(o.h_, {})) {}
  Task& operator=(Task&& o) noexcept {
    if (h_) h_.destroy();
    h_ = std::exchange(o.h_, {});
    return *this;
  }
  Task(const Task&) = delete;
  ~Task() {
    if (h_) h_.destroy();
  }
  bool await_ready() const noexcept { return !h_ || h_.promise().finished; }
  std::coroutine_handle<> await_suspend(std::coroutine_handle<> c) noexcept {
    h_.promise().continuation = c;
    return h_;
  }
  T await_resume() { return std::move(h_.promise().value); }
  handle h_;
};

template <>
class Task<void> {
 public:
  struct promise_type : PromiseBase {
    Task get_return_object() { return Task(std::coroutine_handle<promise_type>::from_promise(*this)); }
    void return_void() {}
  };
  using handle = std::coroutine_handle<promise_type>;
  Task() = default;
  explicit Task(handle h) : h_(h) {}
  Task(Task&& o) noexcept : h_(std::exchange(o.h_, {})) {}
  Task& operator=(Task&& o) noexcept {
    if (h_) h_.destroy();
    h_ = std::exchange(o.h_, {});
    return *this;
  }
  Task(const Task&) = delete;
  ~Task() {
    if (h_) h_.destroy();
  }
  bool await_ready() const noexcept { return !h_ || h_.promise().finished; }
  std::coroutine_handle<> await_suspend(std::coroutine_handle<> c) noexcept {
    h_.promise().continuation = c;
    return h_;
  }
  void await_resume() {}
  bool done() const { return !h_ || h_.promise().finished; }
  handle h_;
};

class Sched {
 public:
  ~Sched() { clear(); }
  // Avança o tempo e retoma as corrotinas cujas esperas terminaram.
  void tick(float dtMs);
  // Lança uma corrotina "solta" (como chamar uma função async sem await).
  void spawn(Task<> t);
  void clear();
  float now() const { return now_; }
  float dt() const { return dt_; }

  void waitTime(float ms, std::coroutine_handle<> h) { timed_.push_back({now_ + ms, h}); }
  void waitFrame(std::coroutine_handle<> h) { frames_.push_back(h); }
  void waitUntil(std::function<bool()> p, std::coroutine_handle<> h) { preds_.push_back({std::move(p), h}); }

 private:
  struct Timed {
    float wake;
    std::coroutine_handle<> h;
  };
  float now_ = 0, dt_ = 0;
  std::vector<Timed> timed_;
  std::vector<std::coroutine_handle<>> frames_;
  std::vector<std::pair<std::function<bool()>, std::coroutine_handle<>>> preds_;
  std::vector<Task<>> roots_;
  bool clearing_ = false;
};

// ---- Esperas
struct Sleep {
  float ms;
  bool await_ready() const noexcept { return ms <= 0; }
  void await_suspend(std::coroutine_handle<> h) { current()->waitTime(ms, h); }
  void await_resume() {}
};
inline Sleep sleep(float ms) { return Sleep{ms}; }

struct NextFrame {
  bool await_ready() const noexcept { return false; }
  void await_suspend(std::coroutine_handle<> h) { current()->waitFrame(h); }
  float await_resume() { return current()->dt(); }
};
inline NextFrame frame() { return {}; }

struct Until {
  std::function<bool()> pred;
  bool await_ready() const { return pred(); }
  void await_suspend(std::coroutine_handle<> h) { current()->waitUntil(pred, h); }
  void await_resume() {}
};
inline Until until(std::function<bool()> p) { return Until{std::move(p)}; }

// ---- Curvas de animação (aproximações das cubic-bezier do jogo)
using Ease = float (*)(float);
float linear(float t);
float easeOut(float t);
float easeIn(float t);
float easeInOut(float t);
float backOut(float t);

// Interpola de 0 a 1 em "ms" milissegundos, chamando f(t) a cada frame.
Task<> tween(float ms, std::function<void(float)> f, Ease e = linear);
// Executa várias corrotinas em paralelo e espera que todas acabem (Promise.all).
Task<> all(std::vector<Task<>> tasks);
inline void spawn(Task<> t) { current()->spawn(std::move(t)); }

}  // namespace co

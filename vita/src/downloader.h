#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include "catalog.h"

// Descarrega os ficheiros dos jogos para <dados>/games/<pasta>/ numa thread própria.
class Downloader {
 public:
  ~Downloader();
  void start(const std::vector<GameDef>& games);
  void cancel() { cancel_ = true; }
  bool running() const { return running_; }
  bool finished() const { return finished_; }
  float progress() const { return total_ ? (float)done_ / total_ : 0.f; }
  std::string status();
  int failures() const { return failures_; }

 private:
  void run();
  bool fetch(const std::string& url, const std::string& dest, bool optional);
  void setStatus(const std::string& s);

  std::vector<GameDef> games_;
  std::atomic<bool> running_{false}, finished_{false}, cancel_{false};
  std::atomic<int> done_{0}, total_{0}, failures_{0};
  std::mutex mx_;
  std::string status_;
  void* curl_ = nullptr;
};

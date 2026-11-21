#include "downloader.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "platform.h"

#ifdef RHM_HAVE_CURL
#include <curl/curl.h>
#endif

Downloader::~Downloader() {
  cancel_ = true;
  while (running_) SDL_Delay(10);
}

std::string Downloader::status() {
  std::lock_guard<std::mutex> lk(mx_);
  return status_;
}

void Downloader::setStatus(const std::string& s) {
  std::lock_guard<std::mutex> lk(mx_);
  status_ = s;
}

void Downloader::start(const std::vector<GameDef>& games) {
  if (running_) return;
  games_ = games;
  running_ = true;
  finished_ = false;
  cancel_ = false;
  done_ = 0;
  failures_ = 0;
  total_ = 0;
  for (auto& g : games_) total_ += (int)catalog::downloadFiles(g).size() + 1;
  SDL_Thread* t = SDL_CreateThreadWithStackSize(
      [](void* p) -> int {
        ((Downloader*)p)->run();
        return 0;
      },
      "rhm-download", 256 * 1024, this);
  if (t) SDL_DetachThread(t);
  else running_ = false;
}

#ifdef RHM_HAVE_CURL
static size_t writeCb(void* data, size_t size, size_t n, void* user) {
  return fwrite(data, size, n, (FILE*)user) * size;
}
static int progressCb(void* user, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
  return ((std::atomic<bool>*)user)->load() ? 1 : 0;  // cancela
}
#endif

bool Downloader::fetch(const std::string& url, const std::string& dest, bool optional) {
  if (plat::fileExists(dest) && plat::fileSize(dest) > 0) return true;
  std::string part = dest + ".part";
  plat::makeDirs(dest.substr(0, dest.find_last_of('/')));
#ifdef RHM_HAVE_CURL
  FILE* f = fopen(part.c_str(), "wb");
  if (!f) return false;
  CURL* c = (CURL*)curl_;
  curl_easy_setopt(c, CURLOPT_URL, url.c_str());
  curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, writeCb);
  curl_easy_setopt(c, CURLOPT_WRITEDATA, f);
  curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, progressCb);
  curl_easy_setopt(c, CURLOPT_XFERINFODATA, &cancel_);
  curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
  CURLcode rc = curl_easy_perform(c);
  long code = 0;
  curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
  fclose(f);
  if (rc != CURLE_OK || code != 200) {
    plat::removeFile(part);
    if (!optional && !cancel_) {
      failures_++;
      fprintf(stderr, "download failed %s: %d %ld %s\n", url.c_str(), (int)rc, code, curl_easy_strerror(rc));
    }
    return false;
  }
  return plat::renameFile(part, dest);
#else
  // PC: copia de um espelho local ($RHM_MIRROR aponta para a pasta "www.google.com" do espelho).
  const char* mirror = getenv("RHM_MIRROR");
  std::string path = url.substr(url.find("/logos/"));
  std::vector<uint8_t> buf;
  if (!mirror || !plat::readFile(std::string(mirror) + path, buf)) {
    if (!optional) failures_++;
    return false;
  }
  SDL_Delay(5);
  return plat::writeFile(dest, buf.data(), buf.size());
#endif
}

void Downloader::run() {
#ifdef RHM_HAVE_CURL
  curl_global_init(CURL_GLOBAL_DEFAULT);
  CURL* c = curl_easy_init();
  curl_ = c;
  curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(c, CURLOPT_CAINFO, (plat::assetDir() + "cacert.pem").c_str());
  curl_easy_setopt(c, CURLOPT_USERAGENT,
                   "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124 Safari/537.36");
  curl_easy_setopt(c, CURLOPT_REFERER, "https://www.google.com/");
  curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 20L);
  curl_easy_setopt(c, CURLOPT_LOW_SPEED_LIMIT, 1L);
  curl_easy_setopt(c, CURLOPT_LOW_SPEED_TIME, 30L);
  curl_easy_setopt(c, CURLOPT_ACCEPT_ENCODING, "");
#endif
  for (auto& g : games_) {
    if (cancel_) break;
    std::string dir = g.localDir();
    plat::makeDirs(dir);
    setStatus(std::to_string(g.index) + "|logo");
    fetch("https://www.google.com" + g.thumbUrl, g.thumbPath(), true);
    done_++;
    auto req = catalog::requiredFiles(g);
    for (auto& f : catalog::downloadFiles(g)) {
      if (cancel_) break;
      setStatus(std::to_string(g.index) + "|" + f);
      bool optional = std::find(req.begin(), req.end(), f) == req.end();
      fetch(g.baseUrl() + f, dir + f, optional);
      done_++;
    }
  }
#ifdef RHM_HAVE_CURL
  curl_easy_cleanup(c);
  curl_ = nullptr;
  curl_global_cleanup();
#endif
  setStatus(cancel_ ? "@dl_cancelled" : failures_ ? "@dl_failed" : "@dl_complete");
  finished_ = true;
  running_ = false;
}

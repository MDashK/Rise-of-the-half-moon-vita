#include "platform.h"

#include <SDL2/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>

#ifdef RHM_VITA
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>
#include <psp2/apputil.h>
#include <psp2/power.h>
// Memória para o heap da newlib (o áudio descodificado ocupa algumas dezenas de MB).
int _newlib_heap_size_user = 128 * 1024 * 1024;  // deixa memória livre para o descodificador de vídeo
// Enchimento no fim do código: o vita-elf-create precisa de ~3,5 KB livres antes do segmento de dados.
extern "C" __attribute__((used, section(".text.zz_pad"))) const unsigned char rhm_text_pad[8192] = {1};
#else
#include <unistd.h>
#endif

namespace plat {

static std::string g_data, g_assets;

const std::string& dataDir() { return g_data; }
const std::string& assetDir() { return g_assets; }

void init() {
#ifdef RHM_VITA
  g_data = "ux0:/data/rise_half_moon/";
  g_assets = "app0:/assets/";
  sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
  static char netMemory[1024 * 1024];
  SceNetInitParam param;
  param.memory = netMemory;
  param.size = sizeof(netMemory);
  param.flags = 0;
  sceNetInit(&param);
  sceNetCtlInit();
  SceAppUtilInitParam ip = {};
  SceAppUtilBootParam bp = {};
  sceAppUtilInit(&ip, &bp);
  // Desempenho máximo: o jogo é leve, mas a descodificação do áudio beneficia.
  scePowerSetArmClockFrequency(444);
  scePowerSetBusClockFrequency(222);
  scePowerSetGpuClockFrequency(222);
#else
  const char* d = getenv("RHM_DATA");
  const char* a = getenv("RHM_ASSETS");
  g_data = d ? d : "userdata/";
  g_assets = a ? a : "assets/";
  if (g_data.back() != '/') g_data += '/';
  if (g_assets.back() != '/') g_assets += '/';
#endif
  makeDirs(g_data);
}

void shutdown() {
#ifdef RHM_VITA
  sceNetCtlTerm();
  sceNetTerm();
  sceAppUtilShutdown();
#endif
}

bool fileExists(const std::string& path) {
  struct stat st;
  return stat(path.c_str(), &st) == 0;
}

int64_t fileSize(const std::string& path) {
  struct stat st;
  if (stat(path.c_str(), &st) != 0) return -1;
  return (int64_t)st.st_size;
}

bool readFile(const std::string& path, std::vector<uint8_t>& out) {
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) return false;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  out.resize(n > 0 ? (size_t)n : 0);
  size_t got = n > 0 ? fread(out.data(), 1, (size_t)n, f) : 0;
  fclose(f);
  return got == out.size();
}

bool readText(const std::string& path, std::string& out) {
  std::vector<uint8_t> b;
  if (!readFile(path, b)) return false;
  out.assign(b.begin(), b.end());
  return true;
}

bool writeFile(const std::string& path, const void* data, size_t size) {
  auto slash = path.find_last_of('/');
  if (slash != std::string::npos) makeDirs(path.substr(0, slash));
  std::string tmp = path + ".tmp";
  FILE* f = fopen(tmp.c_str(), "wb");
  if (!f) return false;
  bool ok = fwrite(data, 1, size, f) == size;
  ok = (fclose(f) == 0) && ok;
  if (!ok) { remove(tmp.c_str()); return false; }
  return renameFile(tmp, path);
}

bool makeDirs(const std::string& path) {
  std::string cur;
  for (size_t i = 0; i < path.size(); i++) {
    cur += path[i];
    if ((path[i] == '/' || i + 1 == path.size()) && cur.size() > 1 && cur.back() != ':' &&
        !(cur.size() >= 2 && cur[cur.size() - 2] == ':')) {
      std::string p = cur;
      if (p.back() == '/') p.pop_back();
      if (p.empty() || p.back() == ':') continue;
#ifdef RHM_VITA
      sceIoMkdir(p.c_str(), 0777);
#else
      mkdir(p.c_str(), 0777);
#endif
    }
  }
  return fileExists(path);
}

bool removeFile(const std::string& path) { return remove(path.c_str()) == 0; }

bool renameFile(const std::string& from, const std::string& to) {
  remove(to.c_str());  // a Vita não substitui ficheiros existentes ao renomear
  return rename(from.c_str(), to.c_str()) == 0;
}

uint64_t nowMs() { return SDL_GetTicks64(); }

}  // namespace plat

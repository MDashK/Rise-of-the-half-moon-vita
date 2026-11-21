#include "video.h"

#include "app.h"
#include "audio.h"
#include "gfx.h"
#include "platform.h"

#ifdef RHM_VITA
#include <psp2/avplayer.h>
#include <psp2/gxm.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/sysmodule.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <map>
#include <vector>
#include <mutex>

namespace {

std::mutex g_mx;
std::map<void*, SceUID> g_blocks;

void* memAlloc(void*, uint32_t alignment, uint32_t size) { return memalign(alignment, size); }
void memFree(void*, void* p) { free(p); }

// Memória para os frames descodificados. Tenta primeiro a memória de vídeo (CDRAM); como as texturas do
// jogo a ocupam quase toda, recorre depois a memória principal física contígua e, por fim, não-cache.
void* gpuAlloc(void*, uint32_t alignment, uint32_t size) {
  if (alignment < 0x40000) alignment = 0x40000;
  struct Try {
    SceKernelMemBlockType type;
    uint32_t align;
  } tries[] = {
      {SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, 0x40000},
      {SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW, 0x100000},
      {SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, 0x1000},
  };
  for (auto& t : tries) {
    uint32_t al = t.align > alignment ? t.align : alignment;
    uint32_t sz = (size + al - 1) & ~(al - 1);
    SceUID uid = sceKernelAllocMemBlock("rhm_video", t.type, sz, nullptr);
    if (uid < 0) {
      gfx::trace("video: frame alloc %u in mem type 0x%x not possible (0x%08x), trying next", sz, (unsigned)t.type, uid);
      continue;
    }
    void* addr = nullptr;
    sceKernelGetMemBlockBase(uid, &addr);
    sceGxmMapMemory(addr, sz, (SceGxmMemoryAttribFlags)(SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE));
    std::lock_guard<std::mutex> lk(g_mx);
    g_blocks[addr] = uid;
    return addr;
  }
  return nullptr;
}

void gpuFree(void*, void* p) {
  SceUID uid = -1;
  {
    std::lock_guard<std::mutex> lk(g_mx);
    auto it = g_blocks.find(p);
    if (it == g_blocks.end()) return;
    uid = it->second;
    g_blocks.erase(it);
  }
  sceGxmUnmapMemory(p);
  sceKernelFreeMemBlock(uid);
}

}  // namespace

bool VideoPlayer::open(const std::string& path) {
  close();
  if (!plat::fileExists(path)) {
    gfx::trace("video: missing %s", path.c_str());
    return false;
  }
  static bool moduleLoaded = false;
  if (!moduleLoaded) {
    sceSysmoduleLoadModule(SCE_SYSMODULE_AVPLAYER);
    moduleLoaded = true;
  }
  SceAvPlayerInitData init;
  memset(&init, 0, sizeof init);
  init.memoryReplacement.allocate = memAlloc;
  init.memoryReplacement.deallocate = memFree;
  init.memoryReplacement.allocateTexture = gpuAlloc;
  init.memoryReplacement.deallocateTexture = gpuFree;
  init.basePriority = 0xA0;
  init.numOutputVideoFrameBuffers = 2;
  init.autoStart = SCE_TRUE;
  init.debugLevel = 0;
  init.defaultLanguage = "en";
  handle_ = sceAvPlayerInit(&init);
  gfx::trace("video: sceAvPlayerInit -> %d", handle_);
  // O handle é um endereço (pode ser "negativo" como int); os erros são da forma 0x806Axxxx.
  if (handle_ == 0 || ((uint32_t)handle_ & 0xFFFF0000u) == 0x806A0000u) {
    handle_ = 0;
    return false;
  }
  int rc = sceAvPlayerAddSource(handle_, path.c_str());
  gfx::trace("video: AddSource(%s) -> 0x%08x", path.c_str(), rc);
  if (((uint32_t)rc & 0xFFFF0000u) == 0x806A0000u || ((uint32_t)rc & 0x80000000u && rc != 0 && (uint32_t)rc < 0x81000000u)) {
    close();
    return false;
  }
  ended_ = false;
  gotFrame_ = false;  waitFrames_ = 0;
  started_ = false;
  audio::streamClear();
  return true;
}

void VideoPlayer::close() {
  if (handle_) {
    sceAvPlayerStop(handle_);
    sceAvPlayerClose(handle_);
    handle_ = 0;
  }
  if (tex_) SDL_DestroyTexture(tex_);
  tex_ = nullptr;
  audio::streamClear();
}

void VideoPlayer::update() {
  if (!handle_) return;
  if (!sceAvPlayerIsActive(handle_)) {
    // terminou, ou nunca chegou a arrancar (desiste ao fim de ~3 s)
    if (started_ || ++waitFrames_ > 180) {
      if (!started_) gfx::trace("video: player never became active");
      ended_ = true;
    }
    return;
  }
  if (!started_) gfx::trace("video: playing");
  started_ = true;
  SceAvPlayerFrameInfo f;
  // áudio: tira os blocos disponíveis (o descodificador gere a sincronização)
  for (int i = 0; i < 8 && audio::streamQueued() < 24000; i++) {
    memset(&f, 0, sizeof f);
    if (!sceAvPlayerGetAudioData(handle_, &f) || !f.pData) break;
    int ch = f.details.audio.channelCount ? f.details.audio.channelCount : 2;
    if (!gotAudio_) {
      gotAudio_ = true;
      gfx::trace("video: audio %d ch %u Hz %u bytes", ch, f.details.audio.sampleRate, f.details.audio.size);
    }
    int frames = (int)(f.details.audio.size / (2 * ch));
    if (ch == 2) audio::streamPush((const int16_t*)f.pData, frames, (int)f.details.audio.sampleRate);
    else {
      std::vector<int16_t> st(frames * 2);
      const int16_t* s = (const int16_t*)f.pData;
      for (int k = 0; k < frames; k++) st[k * 2] = st[k * 2 + 1] = s[k * ch];
      audio::streamPush(st.data(), frames, (int)f.details.audio.sampleRate);
    }
  }
  memset(&f, 0, sizeof f);
  if (sceAvPlayerGetVideoData(handle_, &f) && f.pData) {
    int w = (int)f.details.video.width, h = (int)f.details.video.height;
    // A largura vem alinhada a 16 (ex.: 544 para um vídeo de 540); a proporção dá a largura visível.
    float ar = f.details.video.aspectRatio;
    int vis = w;
    if (ar > 0.1f && ar < 4.f) vis = std::min(w, (int)(h * ar + 0.5f));
    // Sem proporção válida: os vídeos de introdução são verticais 9:16 (540x960).
    if (vis == w && h > w && w % 16 == 0) vis = std::min(w, (h * 9 + 8) / 16);
    // Menos 1 px quando há enchimento: a filtragem bilinear iria buscar a cor da coluna seguinte (verde).
    visibleW_ = vis < w ? vis - 1 : vis;
    if (!gotFrame_) gfx::trace("video: first frame %dx%d (visible %d)", w, h, visibleW_);
    if (!tex_ || w != w_ || h != h_) {
      if (tex_) SDL_DestroyTexture(tex_);
      // Imagem semi-planar (Y + crominância intercalada). Na consola só fica com as cores certas como NV12
      // (com NV21 o azul aparece laranja: o SDL troca os dois formatos da GXM por causa de um erro da Sony).
      // O Vita3K ignora a ordem U/V, por isso não mostra a diferença; "nv21" no debug.txt repõe o antigo.
      // Se o SDL não aceitar YUV, converte-se no CPU.
      const bool nv21 = dbg::has("nv21");
      tex_ = SDL_CreateTexture(gfx::renderer(), nv21 ? SDL_PIXELFORMAT_NV21 : SDL_PIXELFORMAT_NV12,
                               SDL_TEXTUREACCESS_STREAMING, w, h);
      cpuConvert_ = false;
      if (!tex_) {
        gfx::trace("video: YUV texture failed (%s), using CPU conversion", SDL_GetError());
        tex_ = SDL_CreateTexture(gfx::renderer(), SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, w, h);
        cpuConvert_ = true;
      }
      w_ = w;
      h_ = h;
    }
    if (tex_) {
      const uint8_t* y = f.pData;
      const uint8_t* vu = f.pData + (size_t)w * h;
      if (!cpuConvert_) {
        SDL_UpdateNVTexture(tex_, nullptr, y, w, vu, w);
      } else {
        void* px;
        int pitch;
        if (SDL_LockTexture(tex_, nullptr, &px, &pitch) == 0) {
          const int vOff = dbg::has("nv21") ? 0 : 1, uOff = 1 - vOff;  // NV12: U primeiro; NV21: V primeiro
          for (int j = 0; j < h; j++) {
            uint32_t* out = (uint32_t*)((uint8_t*)px + j * pitch);
            const uint8_t* yr = y + j * w;
            const uint8_t* cr = vu + (j / 2) * w;
            for (int i = 0; i < w; i++) {
              int Y = (yr[i] - 16) * 298, V = cr[(i & ~1) + vOff] - 128, U = cr[(i & ~1) + uOff] - 128;
              int R = (Y + 409 * V + 128) >> 8, G = (Y - 100 * U - 208 * V + 128) >> 8, B = (Y + 516 * U + 128) >> 8;
              R = R < 0 ? 0 : R > 255 ? 255 : R;
              G = G < 0 ? 0 : G > 255 ? 255 : G;
              B = B < 0 ? 0 : B > 255 ? 255 : B;
              out[i] = 0xFF000000u | (B << 16) | (G << 8) | R;
            }
          }
          SDL_UnlockTexture(tex_);
        }
      }
      gotFrame_ = true;
    }
    timeMs_ = (float)f.timeStamp;
  }
}

void VideoPlayer::setVolume(float v) { audio::setStreamGain(v); }

#else  // ---------------------------------------------------------------- outras plataformas

bool VideoPlayer::open(const std::string&) { return false; }
void VideoPlayer::close() {}
void VideoPlayer::update() {}
void VideoPlayer::setVolume(float) {}

#endif

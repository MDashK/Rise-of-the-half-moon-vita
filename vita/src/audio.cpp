#include "audio.h"

#include <SDL2/SDL.h>

#include <cmath>
#include <cstring>
#include <mutex>

#include "platform.h"

extern "C" {
int stb_vorbis_decode_filename(const char* filename, int* channels, int* sample_rate, short** output);
}

namespace audio {

namespace {

const int OUT_RATE = 48000;
const int MAX_VOICES = 24;

struct Voice {
  bool active = false;
  int id = 0;
  double pos = 0;          // posição em amostras da fonte
  double start = 0, end = 0;
  int64_t delay = 0;       // amostras de saída até começar
  bool loop = false;
  float gain = 1, target = 1, step = 0;  // ganho atual, alvo e variação por amostra
  bool stopAtTarget = false;
};

SDL_AudioDeviceID g_dev = 0;
std::mutex g_mx;
std::vector<int16_t> g_pcm;  // mono
int g_rate = 32000;
Voice g_voices[MAX_VOICES];
int g_nextId = 1;
int g_music = 0;
bool g_muted = false;

// fluxo estéreo (vídeo)
std::vector<int16_t> g_stream;  // anel estéreo
size_t g_sRead = 0, g_sWrite = 0, g_sCount = 0;
int g_sRate = 48000;
double g_sFrac = 0;
float g_sGain = 1;
const size_t STREAM_FRAMES = 48000;  // 1 s

void mix(void*, Uint8* stream, int len) {
  int16_t* out = (int16_t*)stream;
  int frames = len / 4;
  std::vector<float> acc(frames, 0.f), accR;
  {
    std::lock_guard<std::mutex> lk(g_mx);
    double ratio = (double)g_rate / OUT_RATE;
    for (auto& v : g_voices) {
      if (!v.active || g_pcm.empty()) continue;
      for (int i = 0; i < frames; i++) {
        if (v.delay > 0) {
          v.delay--;
          continue;
        }
        if (v.pos >= v.end) {
          if (v.loop) v.pos = v.start + (v.pos - v.end);
          else { v.active = false; break; }
        }
        size_t i0 = (size_t)v.pos;
        double f = v.pos - i0;
        float s0 = g_pcm[std::min(i0, g_pcm.size() - 1)];
        float s1 = g_pcm[std::min(i0 + 1, g_pcm.size() - 1)];
        acc[i] += (float)(s0 + (s1 - s0) * f) * v.gain;
        v.pos += ratio;
        if (v.step != 0) {
          v.gain += v.step;
          if ((v.step > 0 && v.gain >= v.target) || (v.step < 0 && v.gain <= v.target)) {
            v.gain = v.target;
            v.step = 0;
            if (v.stopAtTarget) { v.active = false; break; }
          }
        }
      }
    }
  }
  // fluxo estéreo do vídeo
  accR = acc;
  {
    std::lock_guard<std::mutex> lk(g_mx);
    if (g_sCount > 0) {
      double step = (double)g_sRate / OUT_RATE;
      for (int i = 0; i < frames && g_sCount > 0; i++) {
        acc[i] += g_stream[g_sRead * 2] * g_sGain;
        accR[i] += g_stream[g_sRead * 2 + 1] * g_sGain;
        g_sFrac += step;
        while (g_sFrac >= 1 && g_sCount > 0) {
          g_sFrac -= 1;
          g_sRead = (g_sRead + 1) % STREAM_FRAMES;
          g_sCount--;
        }
      }
    }
  }
  float master = g_muted ? 0.f : 0.9f;
  for (int i = 0; i < frames; i++) {
    // Limitador suave para evitar distorção quando vários sons se somam.
    float l = 32767.f * std::tanh(acc[i] * master / 32767.f);
    float r = 32767.f * std::tanh(accR[i] * master / 32767.f);
    out[i * 2] = (int16_t)l;
    out[i * 2 + 1] = (int16_t)r;
  }
}

Voice* find(int id) {
  for (auto& v : g_voices)
    if (v.active && v.id == id) return &v;
  return nullptr;
}

}  // namespace

bool init() {
  SDL_AudioSpec want{}, have{};
  want.freq = OUT_RATE;
  want.format = AUDIO_S16SYS;
  want.channels = 2;
  want.samples = 1024;
  want.callback = mix;
  g_dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
  if (!g_dev) return false;
  SDL_PauseAudioDevice(g_dev, 0);
  return true;
}

void shutdown() {
  if (g_dev) SDL_CloseAudioDevice(g_dev);
  g_dev = 0;
}

bool loadGameAudio(const std::string& ogg, const std::string& cache, volatile float* progress) {
  unloadGameAudio();
  std::vector<int16_t> pcm;
  int rate = 0;
  std::vector<uint8_t> raw;
  // Cache: cabeçalho "RHMPCM1\0" + taxa (uint32) + amostras mono int16.
  if (plat::readFile(cache, raw) && raw.size() > 12 && memcmp(raw.data(), "RHMPCM1", 7) == 0) {
    memcpy(&rate, raw.data() + 8, 4);
    pcm.resize((raw.size() - 12) / 2);
    memcpy(pcm.data(), raw.data() + 12, pcm.size() * 2);
  } else {
    if (progress) *progress = 0.1f;
    int ch = 0;
    short* data = nullptr;
    int n = stb_vorbis_decode_filename(ogg.c_str(), &ch, &rate, &data);
    if (n <= 0 || !data) return false;
    pcm.resize(n);
    for (int i = 0; i < n; i++) {
      if (ch == 1) pcm[i] = data[i];
      else {
        int s = 0;
        for (int c = 0; c < ch; c++) s += data[i * ch + c];
        pcm[i] = (int16_t)(s / ch);
      }
    }
    free(data);
    if (progress) *progress = 0.9f;
    std::vector<uint8_t> out(12 + pcm.size() * 2);
    memcpy(out.data(), "RHMPCM1", 8);
    memcpy(out.data() + 8, &rate, 4);
    memcpy(out.data() + 12, pcm.data(), pcm.size() * 2);
    plat::writeFile(cache, out.data(), out.size());
  }
  std::lock_guard<std::mutex> lk(g_mx);
  g_pcm.swap(pcm);
  g_rate = rate > 0 ? rate : 32000;
  if (progress) *progress = 1.f;
  return true;
}

void unloadGameAudio() {
  std::lock_guard<std::mutex> lk(g_mx);
  for (auto& v : g_voices) v.active = false;
  std::vector<int16_t>().swap(g_pcm);
  g_music = 0;
}

bool loaded() {
  std::lock_guard<std::mutex> lk(g_mx);
  return !g_pcm.empty();
}

VoiceId play(const SoundRef& s, float delayMs, bool loop, float gain) {
  if (!s.valid()) return 0;
  std::lock_guard<std::mutex> lk(g_mx);
  if (g_pcm.empty()) return 0;
  Voice* slot = nullptr;
  for (auto& v : g_voices)
    if (!v.active) { slot = &v; break; }
  if (!slot) slot = &g_voices[0];
  Voice& v = *slot;
  v = Voice();
  v.active = true;
  v.id = g_nextId++;
  v.start = s.startMs * g_rate / 1000.0;
  v.end = std::min((double)g_pcm.size(), (s.startMs + s.durMs) * g_rate / 1000.0);
  v.pos = v.start;
  v.delay = (int64_t)(delayMs * OUT_RATE / 1000.0);
  v.loop = loop;
  v.gain = v.target = gain;
  return v.id;
}

void fade(VoiceId id, float toGain, float ms) {
  std::lock_guard<std::mutex> lk(g_mx);
  Voice* v = find(id);
  if (!v) return;
  v->target = toGain;
  float n = std::max(1.f, ms * OUT_RATE / 1000.f);
  v->step = (toGain - v->gain) / n;
  if (v->step == 0) v->gain = toGain;
}

void stop(VoiceId id, float fadeMs) {
  if (fadeMs <= 0) {
    std::lock_guard<std::mutex> lk(g_mx);
    if (Voice* v = find(id)) v->active = false;
    return;
  }
  fade(id, 0.f, fadeMs);
  std::lock_guard<std::mutex> lk(g_mx);
  if (Voice* v = find(id)) {
    v->stopAtTarget = true;
    if (v->step == 0) v->active = false;
  }
}

void stopAll() {
  std::lock_guard<std::mutex> lk(g_mx);
  for (auto& v : g_voices) v.active = false;
  g_music = 0;
}

void playMusic(const SoundRef& s, float fadeInMs, float delayMs) {
  stopMusic(fadeInMs);
  g_music = play(s, delayMs, true, fadeInMs > 0 ? 0.01f : 1.f);
  if (fadeInMs > 0) fade(g_music, 1.f, fadeInMs + delayMs);
}

void stopMusic(float fadeMs) {
  if (g_music) stop(g_music, fadeMs);
  g_music = 0;
}

void streamPush(const int16_t* stereo, int frames, int rate) {
  std::lock_guard<std::mutex> lk(g_mx);
  if (g_stream.empty()) g_stream.assign(STREAM_FRAMES * 2, 0);
  g_sRate = rate > 0 ? rate : 48000;
  for (int i = 0; i < frames; i++) {
    if (g_sCount >= STREAM_FRAMES) break;
    g_stream[g_sWrite * 2] = stereo[i * 2];
    g_stream[g_sWrite * 2 + 1] = stereo[i * 2 + 1];
    g_sWrite = (g_sWrite + 1) % STREAM_FRAMES;
    g_sCount++;
  }
}

void streamClear() {
  std::lock_guard<std::mutex> lk(g_mx);
  g_sRead = g_sWrite = g_sCount = 0;
  g_sFrac = 0;
}

int streamQueued() {
  std::lock_guard<std::mutex> lk(g_mx);
  return (int)g_sCount;
}

void setStreamGain(float g) { g_sGain = g; }

void setMuted(bool m) { g_muted = m; }
bool muted() { return g_muted; }

}  // namespace audio

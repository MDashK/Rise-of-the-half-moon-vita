#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Um trecho do "audio sprite" do jogo (um único ficheiro com todos os sons e músicas).
struct SoundRef {
  float startMs = 0, durMs = 0;
  bool valid() const { return durMs > 0; }
};

namespace audio {

bool init();
void shutdown();

// Carrega o áudio de um jogo. Se existir cache PCM usa-a, senão descodifica o OGG e cria a cache.
// Pode demorar alguns segundos na primeira vez; progress (0..1) é atualizado durante a operação.
bool loadGameAudio(const std::string& oggPath, const std::string& pcmCachePath, volatile float* progress);
void unloadGameAudio();
bool loaded();

using VoiceId = int;
VoiceId play(const SoundRef& s, float delayMs = 0, bool loop = false, float gain = 1.f);
void stop(VoiceId v, float fadeMs = 0);
void fade(VoiceId v, float toGain, float ms);
void stopAll();

// Música de fundo (só uma de cada vez), com transições suaves.
void playMusic(const SoundRef& s, float fadeInMs = 500, float delayMs = 0);
void stopMusic(float fadeMs = 500);

// Fluxo de áudio externo (som do vídeo de introdução): amostras estéreo intercaladas.
void streamPush(const int16_t* stereo, int frames, int rate);
void streamClear();
int streamQueued();  // amostras (por canal) ainda por tocar
void setStreamGain(float g);

void setMuted(bool m);
bool muted();

}  // namespace audio

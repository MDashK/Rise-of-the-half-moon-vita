#pragma once
#include <SDL2/SDL.h>

#include <string>

// Reprodução do vídeo de introdução (intro.mp4, H.264 + AAC).
// Na Vita usa o descodificador de hardware (SceAvPlayer); noutras plataformas não está disponível.
class VideoPlayer {
 public:
  ~VideoPlayer() { close(); }
  bool open(const std::string& path);
  void close();
  // Vai buscar o frame atual e o áudio pendente. Chamar uma vez por frame.
  void update();
  bool active() const { return handle_ != 0 && !ended_; }
  bool ended() const { return ended_; }
  bool hasFrame() const { return tex_ != nullptr && gotFrame_; }
  SDL_Texture* texture() const { return tex_; }
  int width() const { return visibleW_ > 0 ? visibleW_ : w_; }
  int height() const { return h_; }
  float timeMs() const { return timeMs_; }
  void setVolume(float v);

 private:
  int handle_ = 0;
  SDL_Texture* tex_ = nullptr;
  int w_ = 0, h_ = 0;
  bool ended_ = false, gotFrame_ = false, started_ = false;
  float timeMs_ = 0;
  int waitFrames_ = 0;
  int visibleW_ = 0;
  bool cpuConvert_ = false, gotAudio_ = false;
};

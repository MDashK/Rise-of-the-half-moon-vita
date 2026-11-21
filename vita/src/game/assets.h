#pragma once
#include <map>
#include <string>
#include <vector>

#include "audio.h"
#include "catalog.h"
#include "gfx.h"
#include "json.h"
#include "rules.h"

// Sons do "audio sprite" (mesma ordem em todos os meses de novembro a maio).
enum Snd {
  S_CANCEL, S_SNAP, S_BUTTON, S_MOON_RUN3, S_MOON_RUN4, S_MOON_RUN5, S_MOON_RUN6, S_MOON_RUN7, S_MOON_RUN8,
  S_MOON_PHASE_NEW, S_MOON_PHASE_CRESCENT, S_MOON_PHASE_QUARTER, S_MOON_PHASE_GIBBOUS, S_MOON_PHASE_FULL,
  S_MOON_FULLPAIR, S_MOON_BONUS, S_MOON_PAIR, S_MOON_STAR_NEW, S_MOON_STAR_CRESCENT, S_MOON_STAR_QUARTER,
  S_MOON_STAR_GIBBOUS, S_MOON_STAR_FULL, S_DEAL, S_MUSIC_WILDCARD, S_WILDCARD_REVEAL, S_WILDCARD_PLAY,
  S_PLAYER_FULLPAIR, S_MUSIC_GAME1, S_MUSIC_GAME2, S_MUSIC_GAME3, S_MUSIC_LOSE, S_PLAYER_RUN3, S_PLAYER_RUN4,
  S_PLAYER_RUN5, S_PLAYER_RUN6, S_PLAYER_RUN7, S_PLAYER_RUN8, S_PLAYER_PAIR, S_PICKUP, S_PLAYER_PHASE_NEW,
  S_PLAYER_PHASE_CRESCENT, S_PLAYER_PHASE_QUARTER, S_PLAYER_PHASE_GIBBOUS, S_PLAYER_PHASE_FULL, S_PLAYER_BONUS,
  S_PLAYER_STAR_NEW, S_PLAYER_STAR_CRESCENT, S_PLAYER_STAR_QUARTER, S_PLAYER_STAR_GIBBOUS, S_PLAYER_STAR_FULL,
  S_PLACE, S_RULES_CLOSE, S_RULES_OPEN, S_MUSIC_TUTORIAL, S_WILDCARD_SELECT, S_PROGRESS, S_PROGRESS_CONTINUE,
  S_STAR_APPEAR, S_MUSIC_PROGRESS, S_STAR_FULL1, S_STAR_FULL2, S_STAR_FULL3, S_NEXT_LEVEL, S_RESULT_COUNT,
  S_COUNT
};

struct Anim {
  std::vector<Frame> frames;
};

struct WildcardDef {
  std::string id;
  Frame front, back;
  std::string title, description, popup, info, query;  // chaves de tradução
};

struct Theme {
  Color text = Color::hex(0xFDBC8C);        // texto principal do jogo
  Color dialogBg = Color::hex(0x271A26);
  Color modalBg = Color::hex(0x782F45);
  Color modalTitle = Color::hex(0xF981C4);
  Color modalSubtitle = Color::hex(0xFACC93);
  Color button = Color::hex(0x271A26);
  Color buttonHi = Color::hex(0x7E3251);
  Color connection = Color::hex(0x6D5277);
  Color starWhiteStroke = Color::hex(0xFDBC8C);
  Color starBlackStroke = Color::hex(0xD5527A);
  Color turnPlayer = Color::hex(0xFFFFFF);
  Color turnMoon = Color::hex(0xDE557F);
  Color progressText = Color::hex(0xE49B71);
  Color lineEmpty = Color::hex(0x9885A6);
  Color rulesTop = Color::hex(0x804B4A), rulesBottom = Color::hex(0x4E2731), rulesInner = Color::hex(0x130B12);
  Color rulesPoints = Color::hex(0xD08F8E);
  std::vector<std::pair<float, Color>> sky;  // gradiente do fundo (da esquerda para a direita)
};

class MonthAssets {
 public:
  // Carrega os dados e só as folhas necessárias para a introdução (céu, Lua e spinner); as restantes ficam
  // para loadDeferred(), depois do vídeo, para não esgotar a memória de vídeo da Vita durante a reprodução.
  bool load(const GameDef& g, const std::string& language);
  void loadDeferred();
  const GameDef& game() const { return game_; }

  const Texture* sheet(int i) const { return i >= 0 && i < (int)sheets_.size() ? sheets_[i].get() : nullptr; }
  void drawFrame(const Frame& f, float cx, float cy, float w, float h, float alpha = 1.f, float angle = 0.f,
                 Color tint = Color()) const;
  const Texture* tex(const std::string& name) const;  // GoogleFrame.png, border.png, ...
  // Igual a drawFrame, mas com a versão "contrast(.25)" da folha das cartas (wildcards já usados).
  void drawFrameDim(const Frame& f, float cx, float cy, float w, float h, float alpha = 1.f, float angle = 0.f) const;

  const std::string& t(const std::string& key) const;  // tradução
  bool hasText(const std::string& key) const { return messages_.has(key); }

  SoundRef snd(Snd s) const { return s < (int)sounds_.size() ? sounds_[s] : SoundRef(); }
  const Anim* anim(const std::string& name) const;

  Frame cardFace(int value) const;
  Frame cardLight(int value) const;
  Frame cardDark(int value) const;
  Frame cardBack() const { return cardBack_; }
  Frame tile() const { return tile_; }
  Frame stars() const { return stars_; }
  Frame moonDark() const { return moonDark_; }
  Frame moonLight() const { return moonLight_; }
  Frame rules(const std::string& k) const;
  Frame progressEmpty(int i) const { return i < (int)progressEmpty_.size() ? progressEmpty_[i] : Frame(); }

  const std::vector<WildcardDef>& wildcards() const { return wildcards_; }
  const std::map<std::string, rules::BoardDef>& boards() const { return boards_; }
  const rules::BoardDef* board(const std::string& id) const;
  const Json& data() const { return data_; }
  const Theme& theme() const { return theme_; }
  std::string monthName() const;

  struct IntroLine {
    int type;  // 0 mostrar, 1 esconder
    float start;
    float y;
    std::string text;
  };
  const std::vector<IntroLine>& intro() const { return intro_; }

 private:
  Color css(const std::string& selector, const std::string& prop, Color def) const;
  void parseTheme();

  GameDef game_;
  Json data_;
  Json messages_, messagesEn_;
  std::vector<TexPtr> sheets_;
  TexPtr cardsDim_;
  std::map<std::string, TexPtr> extra_;
  std::vector<SoundRef> sounds_;
  std::map<std::string, Anim> anims_;
  std::map<int, Frame> face_, light_, dark_;
  Frame cardBack_, tile_, stars_, moonDark_, moonLight_;
  std::vector<Frame> progressEmpty_;
  std::map<std::string, Frame> rules_;
  std::vector<WildcardDef> wildcards_;
  std::map<std::string, rules::BoardDef> boards_;
  std::vector<IntroLine> intro_;
  Theme theme_;
  std::string css_;
};

Frame frameFromJson(const Json& j);

#pragma once
#include <string>
#include <vector>

// Progresso guardado por mês (equivalente ao localStorage do jogo original).
struct Save {
  bool seenTutorial = false;
  bool seenWildcardTutorial = false;
  bool playedWildcardTutorial = false;
  std::vector<std::string> unlockedWildcards;

  void load(const std::string& gameId);
  void save() const;
  // Quantos meses já tiveram o tutorial visto (o original usa isto para escolher o 1.º tabuleiro).
  static int monthsWithTutorialSeen();

 private:
  std::string id_;
};

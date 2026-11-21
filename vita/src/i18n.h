#pragma once
#include <string>

// Textos do launcher (menu, transferências, "About", sair do jogo) nas 8 línguas do menu.
// A língua é a das definições e pode mudar a qualquer momento.
namespace i18n {

const std::string& tr(const char* key);
std::string fmt(const char* key, const std::string& arg);  // substitui "{0}"
std::string monthName(int gameIndex);                       // nome do mês do jogo (0 = outubro de 2024)
std::string gameTitle(int gameIndex);                       // "Outubro 2024"

}  // namespace i18n

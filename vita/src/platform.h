#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace plat {

// Pasta de dados do utilizador (jogos descarregados, definições, progresso).
// Vita: ux0:/data/rise_half_moon/   PC: ./userdata/ (ou $RHM_DATA)
const std::string& dataDir();
// Recursos incluídos na app. Vita: app0:/assets/   PC: ./assets/ (ou $RHM_ASSETS)
const std::string& assetDir();

void init();
void shutdown();

bool fileExists(const std::string& path);
bool readFile(const std::string& path, std::vector<uint8_t>& out);
bool readText(const std::string& path, std::string& out);
bool writeFile(const std::string& path, const void* data, size_t size);
bool makeDirs(const std::string& path);   // cria todas as pastas do caminho
bool removeFile(const std::string& path);
bool renameFile(const std::string& from, const std::string& to);
int64_t fileSize(const std::string& path);

uint64_t nowMs();

}  // namespace plat

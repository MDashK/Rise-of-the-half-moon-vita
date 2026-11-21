#pragma once
#include <memory>

#include "app.h"
#include "catalog.h"

std::unique_ptr<Scene> makeMenuScene();
std::unique_ptr<Scene> makeGameScene(const GameDef& game);

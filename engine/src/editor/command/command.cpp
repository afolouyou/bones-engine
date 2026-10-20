// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Bones Engine

#include "command.h"

#include <engine/game_elements/gameplay_manager.h>

Command::Command()
{
	doneInPlayMode = GameplayManager::GetGameState() != GameState::Stopped;
}

// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Bones Engine

#pragma once

#include <imgui/imgui.h>

class Texture;

/**
* @brief Get the ImGui texture id of a texture, without depending on EditorUI
* (EditorUI includes the asset selection menu, so it cannot be included there)
*/
[[nodiscard]] ImTextureID GetEditorTextureId(const Texture& texture);

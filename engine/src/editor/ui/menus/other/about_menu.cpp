// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Bones Engine

#include "about_menu.h"

#include <imgui/imgui.h>
#include <editor/editor.h>

void AboutMenu::Init()
{
}

void AboutMenu::Draw()
{
	ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_FirstUseEver);

	const bool visible = ImGui::Begin("About Bones Engine", &m_isActive, ImGuiWindowFlags_NoCollapse);
	if (visible)
	{
		OnStartDrawing();

		ImGui::Text("Version: %s", ENGINE_VERSION);
		ImGui::Text("Bones Engine, based on Xenity Engine by Fewnity");

		if(ImGui::Button("Source code"))
		{
			Editor::OpenLinkInWebBrowser("https://github.com/afolouyou/bones-engine");
		}

		CalculateWindowValues();
	}
	else
	{
		ResetWindowValues();
	}

	ImGui::End();
}

// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Bones Engine

#include "application.h"

#if defined(_WIN32) || defined(_WIN64)
#include <shobjidl.h>
#endif

#include <engine/game_elements/gameplay_manager.h>
#include <engine/asset_management/project_manager.h>
#include <engine/constants.h>
#include "engine.h"
#include "debug/debug.h"

void Application::OpenURL(const std::string& url)
{
	if (url.empty())
	{
		return;
	}

#if defined(_WIN32) || defined(_WIN64)
	const std::wstring wLink = std::wstring(url.begin(), url.end());
	ShellExecuteW(nullptr, nullptr, wLink.c_str(), nullptr, nullptr, SW_SHOW);
#endif
}

void Application::Quit()
{
#if defined(EDITOR)
	GameplayManager::SetGameState(GameState::Stopped, true);
#else
	Engine::Quit();
#endif
}

Platform Application::GetPlatform()
{
#if defined(__PS3__)
	return Platform::P_PS3;
#elif defined(__LINUX__)
	return Platform::P_Linux;
#elif defined(_WIN32) || defined(_WIN64)
	return Platform::P_Windows;
#else
#error "Platform not supported"
#endif
}

AssetPlatform Application::GetAssetPlatform()
{
	return PlatformToAssetPlatform(GetPlatform());
}

bool Application::IsInEditor()
{
#if defined(EDITOR)
	return true;
#else
	return false;
#endif
}

std::string Application::GetGameDataFolder()
{
	std::string folder = "";
	folder = GetGameFolder();
	return folder;
}

std::string Application::GetGameFolder()
{
	const std::string executableFolder = FileSystem::ConvertWindowsPathToBasicPath(Engine::GetArguments().executableLocation);
	const size_t lastSlash = executableFolder.find_last_of("/");
	const size_t lastBackSlash = executableFolder.find_last_of("\\");
	size_t limit = lastSlash;
	if (limit == -1 || (lastBackSlash != -1 && lastBackSlash > limit))
	{
		limit = lastBackSlash;
	}

	if (limit != -1)
	{
		return executableFolder.substr(0, limit + 1);
	}
	else 
	{
		return "";
	}
}

AssetPlatform Application::PlatformToAssetPlatform(Platform platform)
{
	if (platform == Platform::P_PS3)
		return AssetPlatform::AP_PS3;
	else if (platform == Platform::P_Windows || platform == Platform::P_Linux)
		return AssetPlatform::AP_Standalone;
	else
	{
		XASSERT(false, "[Application::PlatformToAssetPlatform] Platform not supported");
		Debug::PrintError("[Application::PlatformToAssetPlatform] Platform not supported");
		return AssetPlatform::AP_Standalone;
	}
}

std::string Application::GetBonesVersion()
{
	return ENGINE_VERSION;
}

std::string Application::GetGameName()
{
	return ProjectManager::s_projectSettings.gameName;
}

std::string Application::GetCompanyName()
{
	return ProjectManager::s_projectSettings.companyName;
}

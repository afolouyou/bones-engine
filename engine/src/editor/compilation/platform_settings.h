// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Bones Engine

#pragma once
#include <engine/reflection/reflection.h>
#include <engine/event_system/event_system.h>
#include <engine/platform.h>
#include <engine/graphics/ui/icon.h>
#include <engine/graphics/texture/texture.h>

class Texture;

enum class PlatformSettingsErrorPS3
{
	None = 0,
};

enum class PlatformSettingsErrorWindows
{
	None = 0,
};

class PlatformSettings : public Reflective
{
public:
	PlatformSettings() = delete;
	PlatformSettings(Event<>* onChangeEvent)
	{
		XASSERT(onChangeEvent, "onChangeEvent is nullptr");

		this->onChangeEvent = onChangeEvent;
	}

	[[nodiscard]] ReflectiveData GetReflectiveData() override
	{
		ReflectiveData reflectedVariables;
		return reflectedVariables;
	}

	void OnReflectionUpdated() override
	{
		if (onChangeEvent)
		{
			onChangeEvent->Trigger();
		}
	}

	[[nodiscard]] virtual int IsValid() = 0;

	bool isDebugMode = false;
	bool enableOnlineProfiler = false;
	bool enableProfiler = false;
	bool useCompilationCache = true; // Should be removed when the engine will be compiled as a static library

protected:
	Event<>* onChangeEvent = nullptr;
};

class PlatformSettingsPS3 : public PlatformSettings
{
public:
	PlatformSettingsPS3() = delete;
	PlatformSettingsPS3(Event<>* onChangeEvent) : PlatformSettings(onChangeEvent) {}

	[[nodiscard]] ReflectiveData GetReflectiveData() override
	{
		ReflectiveData reflectedVariables;
		Reflective::AddVariable(reflectedVariables, isDebugMode, "isDebugMode");
		Reflective::AddVariable(reflectedVariables, enableProfiler, "enableProfiler");
		//Reflective::AddVariable(reflectedVariables, enableOnlineProfiler, "enableOnlineProfiler", true);
		Reflective::AddVariable(reflectedVariables, useCompilationCache, "useCompilationCache");
		return reflectedVariables;
	}

	[[nodiscard]] int IsValid() override;
private:
};

class PlatformSettingsWindows : public PlatformSettings
{
public:
	PlatformSettingsWindows() = delete;
	PlatformSettingsWindows(Event<>* onChangeEvent) : PlatformSettings(onChangeEvent) {}

	[[nodiscard]] ReflectiveData GetReflectiveData() override
	{
		ReflectiveData reflectedVariables;
		//Reflective::AddVariable(reflectedVariables, isDebugMode, "isDebugMode", true);
		//Reflective::AddVariable(reflectedVariables, enableProfiler, "enableProfiler", true);
		//Reflective::AddVariable(reflectedVariables, enableOnlineProfiler, "enableOnlineProfiler", true);
		Reflective::AddVariable(reflectedVariables, icon, "icon");
		return reflectedVariables;
	}

	[[nodiscard]] int IsValid() override;

	std::shared_ptr<Icon> icon;
private:
};

class BuildPlatform
{
public:
	Platform platform = Platform::P_Windows;
	std::shared_ptr<Texture> icon;
	std::string name;
	bool isSupported = false;
	bool supportBuildAndRun = false;
	bool supportBuildAndRunOnHardware = false;
	std::shared_ptr<PlatformSettings> settings = nullptr;
};
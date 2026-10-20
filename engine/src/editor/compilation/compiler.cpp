// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Xenity Engine

#include "compiler.h"

#include <thread>
#include <filesystem>

// Editor
#include <editor/editor.h>
#include <editor/ui/menus/compilation/docker_config_menu.h>
#include <editor/ui/menus/compilation/build_settings_menu.h>
#include <editor/cooker/cooker.h>
#include <editor/utils/copy_utils.h>
#include <editor/compilation/compiler_cache.h>

// Engine
#include <engine/engine_settings.h>
#include <engine/asset_management/project_manager.h>
#include <engine/debug/debug.h>
#include <engine/scene_management/scene_manager.h>
#include <engine/engine.h>
#include <engine/dynamic_lib/dynamic_lib.h>
#include <engine/class_registry/class_registry.h>
#include <engine/game_interface.h>
#include <engine/file_system/directory.h>
#include <engine/file_system/file.h>
#include <engine/assertions/assertions.h>
#include <engine/file_system/file_system.h>
#include <engine/application.h>
#include <engine/graphics/texture/texture.h>
#include <engine/constants.h>
#include <engine/graphics/frame_limiter/frame_limiter.h>
#include <engine/tools/benchmark.h>

namespace fs = std::filesystem;

// Note: A docker copy command will create a directory of the source folder has not the same name as the dest folder.
// If both folders have the same name, the content of src will be pasted in dst without creating a new folder

Event<CompilerParams, bool> Compiler::s_onCompilationEndedEvent;
Event<CompilerParams> Compiler::s_onCompilationStartedEvent;

std::string Compiler::s_engineFolderLocation = "";
std::string Compiler::s_engineProjectLocation = "";
std::string Compiler::s_compilerExecFileName = "";

CompilationMethod Compiler::s_compilationMethod = CompilationMethod::MSVC;
bool Compiler::s_isCompilationCancelled = false;
CompilationTimings Compiler::s_timings;

std::string MakePathAbsolute(const std::string& path, const std::string& root)
{
	if (!fs::path(path).is_absolute())
	{
		return root + "/" + path;
	}

	return path;
}

CompileResult Compiler::Compile(CompilerParams params)
{
	XASSERT(params.buildPlatform.platform != Platform::P_COUNT, "[Compiler::Compile] Platform is not set");
	XASSERT(!params.tempPath.empty(), "[Compiler::Compile] Temporary path is not set");
	XASSERT(!params.sourcePath.empty(), "[Compiler::Compile] Source path is not set");
	XASSERT(!params.exportPath.empty(), "[Compiler::Compile] Export path is not set");
	XASSERT(!params.libraryName.empty(), "[Compiler::Compile] Library name is not set");

	FrameLimiter::SetIsEnabled(true);
	FrameLimiter::SetWaitTiming(250);

	DeleteTempFiles(params);

	s_isCompilationCancelled = false;

	// Ensure path are absolute
	const std::string root = fs::current_path().string();
	params.tempPath = MakePathAbsolute(params.tempPath, root);
	params.sourcePath = MakePathAbsolute(params.sourcePath, root);
	params.exportPath = MakePathAbsolute(params.exportPath, root);

	// Print parameters
	Debug::Print(
		"[Compiler::Compile] Preparing:"
		"\n- Platform: " + EnumHelper::EnumAsString(params.buildPlatform.platform).substr(2)
		+ "\n- Build Type: " + EnumHelper::EnumAsString(params.buildType)
		+ "\n- Temporary Path: " + params.tempPath
		+ "\n- Source Path: " + params.sourcePath
		+ "\n- Export Path: " + params.exportPath
		+ "\n- Library Name: " + params.libraryName
		+ "\n- Editor DLL: " + params.getEditorDynamicLibraryName()
		+ "\n- Runtime DLL: " + params.getDynamicLibraryName()
		, true);

	const CompilerAvailability availability = CheckCompilerAvailability(params);
	if (availability != CompilerAvailability::AVAILABLE)
	{
		OnCompileEnd(CompileResult::ERROR_COMPILER_AVAILABILITY, params);
		return CompileResult::ERROR_COMPILER_AVAILABILITY;
	}

	// Clean temporary directory
	try
	{
		fs::remove_all(params.tempPath);
		fs::create_directory(params.tempPath);
		fs::create_directory(params.tempPath + "cooked_assets/");
		fs::create_directory(s_engineProjectLocation + "engine/src/game_code/");
	}
	catch (const std::exception&)
	{
		Debug::PrintWarning("[Compiler::Compile] Unable to clear the compilation folder", true);
	}

	// Save project settings of the build
	{
		ProjectSettings projectSettingsCopy = ProjectManager::s_projectSettings;

		ProjectManager::s_projectSettings.compiledLibEngineVersion = ENGINE_VERSION;
		bool isDebugMode = false;
#if defined(DEBUG)
		isDebugMode = true;
#endif
		ProjectManager::s_projectSettings.isLibCompiledForDebug = isDebugMode;

		bool is64Bits = false;
#if defined(_WIN64)
		is64Bits = true;
#endif
		ProjectManager::s_projectSettings.isLibCompiledFor64Bits = is64Bits;

		ProjectManager::SaveProjectSettings(params.tempPath);

		ProjectManager::s_projectSettings = projectSettingsCopy;
	}

	bool cookResult = true;

	// Cook assets
	if (params.buildType != BuildType::EditorHotReloading)
	{
		Benchmark cookBenchmark;
		cookBenchmark.Start();
		CookSettings cookSettings;
		cookSettings.exportPath = params.tempPath + "cooked_assets/";
		cookSettings.assetPlatform = Application::PlatformToAssetPlatform(params.buildPlatform.platform);
		cookSettings.platform = params.buildPlatform.platform;
		if (params.buildType == BuildType::BuildShaders)
		{
			cookSettings.exportShadersOnly = true;
		}
		else
		{
			cookSettings.exportShadersOnly = false;
		}
		cookResult = Cooker::CookAssets(cookSettings);
		cookBenchmark.Stop();
		s_timings.cookTime = cookBenchmark.GetMicroSeconds();
	}

	CleanDestinationFolder(params.exportPath);

	if (!cookResult)
	{
		OnCompileEnd(CompileResult::ERROR_COOK_FAILED, params);
		return CompileResult::ERROR_COOK_FAILED;
	}

	// Compile depending on platform
	CompileResult result = CompileResult::ERROR_UNKNOWN;
	switch (params.buildPlatform.platform)
	{
	case Platform::P_Windows:
		result = CompileWindows(params);
		break;
	case Platform::P_PS3:
		//result = CompileWSL(params); // Deprecated
		result = CompileInDocker(params);
		break;
	default:
		Debug::PrintError("[Compiler::Compile] No compile method for this platform!", true);
		break;
	}

	// Send compile result
	OnCompileEnd(result, params);
	return result;
}

void Compiler::Init()
{
	UpdatePaths();

	CompilerParams params;
	params.buildPlatform = BuildSettingsMenu::GetBuildPlatform(Platform::P_Windows);
	const CompilerAvailability availability = CheckCompilerAvailability(params);
	if (availability == CompilerAvailability::MISSING_COMPILER_SOFTWARE)
	{
		Debug::PrintError("The compiler is not correctly setup. Please check compiler settings at [Window->Engine Settings]", false);
	}
}

void Compiler::UpdatePaths()
{
	const std::string root = FileSystem::ConvertWindowsPathToBasicPath(fs::current_path().string());
	s_engineFolderLocation = root + "/";
	s_engineProjectLocation = s_engineFolderLocation;

	// When the editor runs from a build folder (zig-out/bin, build/...), walk up to the repository root.
	std::string candidate = s_engineFolderLocation;
	for (int i = 0; i < 6; i++)
	{
		if (fs::exists(candidate + "engine/src") && fs::exists(candidate + "third_party"))
		{
			s_engineProjectLocation = candidate;
			break;
		}

		const std::string withoutTrailingSlash = candidate.substr(0, candidate.size() - 1);
		const size_t slashPos = withoutTrailingSlash.find_last_of('/');
		if (slashPos == std::string::npos)
		{
			break;
		}
		candidate = candidate.substr(0, slashPos + 1);
	}
#if defined(_WIN64) 
	s_compilerExecFileName = MSVC_START_FILE_64BITS;
#else
	s_compilerExecFileName = MSVC_START_FILE_32BITS;
#endif
}

CompilerAvailability Compiler::CheckCompilerAvailability(const CompilerParams& params)
{
	UpdatePaths();

	int error = 0;


	if (params.buildPlatform.platform == Platform::P_Windows)
	{
		// Check if the compiler executable exists
		if (!fs::exists(EngineSettings::values.compilerPath + s_compilerExecFileName))
		{
			error |= (int)CompilerAvailability::MISSING_COMPILER_SOFTWARE;
		}

		// Check if the engine compiled library exists
		if (params.buildType == BuildType::EditorHotReloading)
		{
			if (!fs::exists(s_engineFolderLocation + ENGINE_EDITOR_FOLDER + ".lib") ||
				!fs::exists(s_engineFolderLocation + ENGINE_EDITOR_FOLDER + ".dll"))
			{
				error |= (int)CompilerAvailability::MISSING_ENGINE_COMPILED_LIB;
			}
		}
		else
		{
			if (!fs::exists(s_engineFolderLocation + ENGINE_GAME_FOLDER + ".lib") ||
				!fs::exists(s_engineFolderLocation + ENGINE_GAME_FOLDER + ".dll"))
			{
				error |= (int)CompilerAvailability::MISSING_ENGINE_COMPILED_LIB;
			}
		}
	}
	if (error == 0)
	{
		error |= (int)CompilerAvailability::AVAILABLE;
	}
	else
	{
		if (error & (int)CompilerAvailability::MISSING_COMPILER_SOFTWARE)
		{
			Debug::PrintError("Compiler executable " + std::string(s_compilerExecFileName) + " not found in " + EngineSettings::values.compilerPath);
		}
		if (error & (int)CompilerAvailability::MISSING_ENGINE_COMPILED_LIB)
		{
			Debug::PrintError("Compiled engine library not found in " + s_engineFolderLocation);
		}
	}
	return (CompilerAvailability)error;
}

CompileResult Compiler::CompilePlugin(Platform platform, const std::string& pluginPath)
{
	UpdatePaths();

	XASSERT(!pluginPath.empty(), "[Compiler::CompilePlugin] pluginPath is empty");

	if (pluginPath.empty())
		return CompileResult::ERROR_UNKNOWN;

	const std::string plugin_name = fs::path(pluginPath).parent_path().filename().string();

	CompilerParams params{};
	params.libraryName = "plugin_" + plugin_name;
	params.buildPlatform = BuildSettingsMenu::GetBuildPlatform(platform);
	params.buildType = BuildType::EditorHotReloading;
	params.sourcePath = pluginPath;
	params.tempPath = "plugins/.build/";
	params.exportPath = "plugins/";

	const CompileResult result = Compile(params);

	DeleteTempFiles(params);

	return result;
}

DockerState Compiler::CheckDockerState(Event<DockerState>* callback)
{
	// 2>nul Silent error
	// 1>nul Silent standard output
	DockerState result = DockerState::NOT_INSTALLED;

	const int checkDockerInstallResult = system("docker 2>nul 1>nul");
	if (checkDockerInstallResult != 0)
	{
		result = DockerState::NOT_INSTALLED;
	}
	else
	{
		const int checkDockerRunningResult = system("docker ps 2>nul 1>nul");
		if (checkDockerRunningResult != 0)
		{
			result = DockerState::NOT_RUNNING;
		}
		else
		{
			system("docker images -f r=u"); // Workaround for docker image inspect not working when resource saver mode is enabled

			const int checkDockerImageResult = system("docker image inspect xenity_1_0_0 2>nul 1>nul");
			if (checkDockerImageResult != 0)
			{
				result = DockerState::MISSING_IMAGE;
			}
			else
			{
				result = DockerState::RUNNING;
			}
		}
	}

	if (callback)
	{
		callback->Trigger(result);
	}

	return result;
}

bool Compiler::ExportProjectFiles(const std::string& exportPath)
{
	XASSERT(!exportPath.empty(), "[Compiler::ExportProjectFiles] exportPath is empty");
	if (exportPath.empty())
		return false;

	const std::string projectCookedAssetsFolder = ProjectManager::GetProjectFolderPath() + ".build/cooked_assets/" + ASSETS_FOLDER;
	CopyUtils::AddCopyEntry(true, projectCookedAssetsFolder, exportPath + ASSETS_FOLDER);

	const std::string fileDataBasePath = ProjectManager::GetProjectFolderPath() + ".build/cooked_assets/db.xenb";
	CopyUtils::AddCopyEntry(false, fileDataBasePath, exportPath + "db.xenb");

	const std::string binaryFilePath = ProjectManager::GetProjectFolderPath() + ".build/cooked_assets/data.xenb";
	CopyUtils::AddCopyEntry(false, binaryFilePath, exportPath + "data.xenb");

	const std::string projectCookedPublicEngineAssetsFolder = ProjectManager::GetProjectFolderPath() + ".build/cooked_assets/" + PUBLIC_ENGINE_ASSETS_FOLDER;
	CopyUtils::AddCopyEntry(true, projectCookedPublicEngineAssetsFolder, exportPath + PUBLIC_ENGINE_ASSETS_FOLDER);

	CopyUtils::AddCopyEntry(false, ProjectManager::GetProjectFolderPath() + ".build/" + PROJECT_SETTINGS_FILE_NAME, exportPath + PROJECT_SETTINGS_FILE_NAME);

	const bool copyResult = CopyUtils::ExecuteCopyEntries();
	return copyResult;
}

CompileResult Compiler::CompileGame(const BuildPlatform buildPlatform, BuildType buildType, const std::string& exportPath)
{
	Benchmark totalCompilationBenchmark;
	totalCompilationBenchmark.Start();

	if (exportPath == ProjectManager::GetProjectFolderPath())
	{
		Debug::PrintError("[Compiler::CompileGame] Export path is the same as the project path");
		return CompileResult::ERROR_UNKNOWN;
	}

	UpdatePaths();

	XASSERT(!exportPath.empty(), "[Compiler::CompileGame] exportPath is empty");
	if (exportPath.empty())
		return CompileResult::ERROR_UNKNOWN;

	CompilerParams params{};
	params.libraryName = "game";
	params.buildPlatform = buildPlatform;
	params.buildType = buildType;
	params.sourcePath = ProjectManager::GetAssetFolderPath();
	params.tempPath = ProjectManager::GetProjectFolderPath() + ".build/";
	params.exportPath = exportPath;

	s_onCompilationStartedEvent.Trigger(params);

	if (buildType == BuildType::BuildShadersAndGame)
	{
		params.buildType = BuildType::BuildShaders;
		params.tempPath = ProjectManager::GetProjectFolderPath() + ".shaders_build/";

		// Compile shaders
		const CompileResult shaderResult = Compile(params);

		if (shaderResult != CompileResult::SUCCESS)
		{
			DeleteTempFiles(params);
			return shaderResult;
		}
	}

	if (buildType == BuildType::BuildShadersAndGame)
	{
		params.buildType = BuildType::BuildGame;
		params.tempPath = ProjectManager::GetProjectFolderPath() + ".build/";
	}


	// Compile engine and game
	const CompileResult result = Compile(params);

	if (result != CompileResult::SUCCESS)
	{
		DeleteTempFiles(params);
		return result;
	}

	// Copy assets
	if (params.buildType != BuildType::EditorHotReloading)
	{
		const bool copyResult = ExportProjectFiles(params.exportPath);
		if (!copyResult)
		{
			DeleteTempFiles(params);
			return CompileResult::ERROR_FILE_COPY;
		}
	}

	DeleteTempFiles(params);

	// Open build folder if success
	if (params.buildType == BuildType::BuildGame)
	{
		Editor::OpenExplorerWindow(params.exportPath, false);
	}

	// Launch game
	if (params.buildType == BuildType::BuildAndRunGame)
	{
		auto t = std::thread(StartGame, params.buildPlatform.platform, params.exportPath);
		t.detach();
	}

	totalCompilationBenchmark.Stop();
	s_timings.totalCompileTime = totalCompilationBenchmark.GetMicroSeconds();

	PrintTimings();

	return result;
}

void Compiler::DeleteTempFiles(const CompilerParams& params)
{
	// Delete temp compiler folder content
	try
	{
		fs::remove_all(params.tempPath);
	}
	catch (const std::exception&) {}
}

void Compiler::CleanDestinationFolder(const std::string& exportPath)
{
	try
	{
		fs::remove(exportPath + "freetype.dll");
		fs::remove(exportPath + "game.dll");
		fs::remove(exportPath + "SDL3.dll");
		fs::remove(exportPath + "Xenity_Engine.dll");

		fs::remove(exportPath + "data.xenb");
		fs::remove(exportPath + "db.xenb");
		fs::remove(exportPath + "project_settings.json");
	}
	catch (const std::exception&) {}
}

void Compiler::PrintTimings()
{
	Debug::Print("Compilation timings:", true);
	Debug::Print("Cooking time: " + std::to_string(s_timings.cookTime) + " us (" + std::to_string(s_timings.cookTime / 1000000.0f) + " s)", true);
	Debug::Print("Docker preparation time: " + std::to_string(s_timings.prepareDockerTime) + " us" + " us (" + std::to_string(s_timings.prepareDockerTime / 1000000.0f) + " s)", true);
	Debug::Print("Docker code compile time: " + std::to_string(s_timings.dockerCompileTime) + " us" + " us (" + std::to_string(s_timings.dockerCompileTime / 1000000.0f) + " s)", true);
	Debug::Print("Docker shader compile time: " + std::to_string(s_timings.shaderCompileTime) + " us" + " us (" + std::to_string(s_timings.shaderCompileTime / 1000000.0f) + " s)", true);
	Debug::Print("Total compile time: " + std::to_string(s_timings.totalCompileTime) + " us" + " us (" + std::to_string(s_timings.totalCompileTime / 1000000.0f) + " s)", true);
}

void Compiler::CompileGameThreaded(const BuildPlatform buildPlatform, BuildType buildType, const std::string& exportPath)
{
	Debug::ClearDebugLogs();
	// Create the file references on the main thread, the build thread must not modify the asset lists while the editor is running
	Cooker::PrepareCooking();
	std::thread t = std::thread([buildPlatform, buildType, exportPath]()
		{
			CompileGame(buildPlatform, buildType, exportPath);
			// Release the file references prepared for this build
			Cooker::ClearPreparedCooking();
		});
	t.detach();
}

void Compiler::HotReloadGame()
{
#if defined(_WIN32) || defined(_WIN64)
	Engine::s_game.reset();
	Engine::s_game = nullptr;

	// Prepare scene
	SceneManager::SaveScene(SaveSceneType::SaveSceneForHotReloading);
	SceneManager::ClearScene();

	// Reset registery and re-add basic components
	ClassRegistry::Reset();
	ClassRegistry::RegisterEngineComponents();
	ClassRegistry::RegisterEngineFileClasses();

	// Unload library
	DynamicLibrary::UnloadGameLibrary();

	// Compile game
	CompileResult result = Compiler::CompileGame(
		BuildSettingsMenu::GetBuildPlatform(Platform::P_Windows),
		BuildType::EditorHotReloading,
		ProjectManager::GetProjectFolderPath() + "temp/"
	);

	if (result == CompileResult::SUCCESS)
	{
		// Reload game
		DynamicLibrary::LoadGameLibrary(ProjectManager::GetProjectFolderPath() + "temp/" + "game_editor");

		// Create game instance
		Engine::s_game = DynamicLibrary::CreateGame();
		if (Engine::s_game)
		{
			Debug::Print("Game compilation done");
			Engine::s_game->Start();
		}
		else
		{
			// Should not happen here
			Debug::PrintError("[Compiler::HotReloadGame] Game compilation failed");
		}
	}
	else
	{
		Debug::PrintError("[Compiler::HotReloadGame] Game compilation failed");
	}

	SceneManager::RestoreSceneHotReloading();
#endif
}

void Compiler::OnCompileEnd(CompileResult result, CompilerParams& params)
{
	switch (result)
	{
	case CompileResult::SUCCESS:
		Debug::Print("Code compiled successfully!");
		break;
	case CompileResult::ERROR_UNKNOWN:
		Debug::PrintError("Unable to compile (unkown error)");
		break;
	case CompileResult::ERROR_GAME_CODE_COPY:
		Debug::PrintError("Error when copying game's code");
		break;
	case CompileResult::ERROR_FINAL_GAME_FILES_COPY:
		Debug::PrintError("Error when copying game's files");
		break;
	case CompileResult::ERROR_FILE_COPY:
		Debug::PrintError("Error when copying files");
		break;
	case CompileResult::ERROR_COOK_FAILED:
		Debug::PrintError("Error when cooking files");
		break;

		// Specific to WSL
	case CompileResult::ERROR_WSL_COMPILATION:
		Debug::PrintError("Unable to compile on WSL (probably a C++ error)");
		break;
	case CompileResult::ERROR_WSL_ENGINE_CODE_COPY:
		Debug::PrintError("Error when copying engine's code");
		break;
	case CompileResult::ERROR_WSL_ENGINE_LIBS_INCLUDE_COPY:
		Debug::PrintError("Error when copying engine's libraries files");
		break;
	case CompileResult::ERROR_WSL_CMAKELISTS_COPY:
		Debug::PrintError("Error when copying the PS3 make file");
		break;
	case CompileResult::ERROR_COMPILER_AVAILABILITY:
		Debug::PrintError("The compiler is not correctly setup. Please check compiler settings at [Window->Engine Settings]");
		break;

		// Specific to Docker
	case CompileResult::ERROR_DOCKER_COMPILATION:
		Debug::PrintError("Unable to compile on Docker (probably a C++ error)");
		break;
	case CompileResult::ERROR_DOCKER_SHADERS_COMPILATION:
		Debug::PrintError("Unable to compile shaders on Docker (error in shader code)");
		break;
	case CompileResult::ERROR_DOCKER_NOT_FOUND:
		Debug::PrintError("Unable to find Docker");
		break;
	case CompileResult::ERROR_DOCKER_NOT_RUNNING:
		Debug::PrintError("Docker is not running");
		break;
	case CompileResult::ERROR_DOCKER_MISSING_IMAGE:
		Debug::PrintError("Docker image is missing");
		break;
	case CompileResult::ERROR_DOCKER_COULD_NOT_START:
		Debug::PrintError("Docker path is not correctly setup. Please check compiler settings at [Window->Engine Settings]");
		break;

	case CompileResult::ERROR_COMPILATION_CANCELLED:
		Debug::PrintError("The compilation has been cancelled");
		break;

	default:
		Debug::PrintError("Unable to compile (unkown error)");
		break;
	}

	if (params.buildType != BuildType::BuildShaders || result != CompileResult::SUCCESS)
	{
		s_onCompilationEndedEvent.Trigger(params, result == CompileResult::SUCCESS);
	}

	FrameLimiter::SetIsEnabled(false);
}

std::string WindowsPathToWSL(const std::string& path)
{
	XASSERT(!path.empty(), "[Compiler::WindowsPathToWSL] path is empty");
	if (path.empty())
		return "";

	std::string newPath = path;
	newPath[0] = tolower(newPath[0]);
	const int pathSize = (int)path.size();
	for (int i = 1; i < pathSize; i++)
	{
		if (newPath[i] == '/')
		{
			newPath[i] = '/';
		}
	}
	newPath.erase(newPath.begin() + 1);
	return newPath;
}

std::vector<std::string> CopyGameSource(const CompilerParams& params)
{
	std::vector<std::string> sourceDestFolders;

	// Copy source code

	fs::create_directory(params.tempPath + "source/");

	const size_t sourcePathLen = params.sourcePath.size();
	std::shared_ptr<Directory> gameSourceDir = std::make_shared<Directory>(params.sourcePath);
	const std::vector<std::shared_ptr<File>> files = gameSourceDir->GetAllFiles(true);

	const size_t fileCount = files.size();
	for (size_t i = 0; i < fileCount; i++)
	{
		const std::string ext = files[i]->GetFileExtension();

		// Check extension
		if (ext != ".h" && ext != ".cpp")
			continue;

		// Copy file
		const std::string filePathToCopy = files[i]->GetPath();
		const std::string destFolder = params.tempPath + "source/" + files[i]->GetFolderPath().substr(sourcePathLen);
		fs::create_directories(destFolder);

		fs::copy_file(
			filePathToCopy,
			params.tempPath + "source/" + filePathToCopy.substr(sourcePathLen), // substr to remove all folders including the locations of the project/source files
			fs::copy_options::overwrite_existing
		);

		// Check if the destination folder is already in the list
		bool found = false;
		const size_t sourceDestFoldersSize = sourceDestFolders.size();
		for (size_t x = 0; x < sourceDestFoldersSize; x++)
		{
			if (sourceDestFolders[x] == destFolder)
			{
				found = true;
				break;
			}
		}

		// if not, add the destination folder in the list for later use
		if (!found)
			sourceDestFolders.push_back(destFolder);
	}

	return sourceDestFolders;
}

CompileResult Compiler::CompileWindows(const CompilerParams& params)
{
	s_compilationMethod = CompilationMethod::MSVC;

	if (params.buildType == BuildType::EditorHotReloading) // In hot reloading mode:
	{
		const std::string engineLibPath = s_engineFolderLocation + ENGINE_EDITOR_FOLDER + ".lib";

		// Copy engine editor lib to the temp build folder
		CopyUtils::AddCopyEntry(false, engineLibPath, params.tempPath + ENGINE_EDITOR_FOLDER + ".lib");
		// Copy editor header
		CopyUtils::AddCopyEntry(false, s_engineProjectLocation + "engine/src/xenity_editor.h", params.tempPath + "xenity_editor.h");
	}
	else // In build mode:
	{
		const std::string engineLibPath = s_engineFolderLocation + ENGINE_GAME_FOLDER + ".lib";
		const std::string engineDllPath = s_engineFolderLocation + ENGINE_GAME_FOLDER + ".dll";
		const std::string sdlDllPath = s_engineProjectLocation + "third_party/libs/x64/SDL3.dll";
		const std::string freetypeDllPath = s_engineProjectLocation + "third_party/libs/x64/freetype.dll";

		// Copy engine game lib to the temp build folder
		CopyUtils::AddCopyEntry(false, engineLibPath, params.tempPath + ENGINE_GAME_FOLDER + ".lib");
		// Copy all DLLs to the export folder
		CopyUtils::AddCopyEntry(false, engineDllPath, params.exportPath + ENGINE_GAME_FOLDER + ".dll");
		CopyUtils::AddCopyEntry(false, sdlDllPath, params.exportPath + "SDL3.dll");
		CopyUtils::AddCopyEntry(false, freetypeDllPath, params.exportPath + "freetype.dll");
	}

	// Copy engine headers to the temp build folder
	CopyUtils::AddCopyEntry(true, s_engineProjectLocation + "engine/src/engine/", params.tempPath + "engine/");
	CopyUtils::AddCopyEntry(false, s_engineProjectLocation + "engine/src/xenity.h", params.tempPath + "xenity.h");
	CopyUtils::AddCopyEntry(false, s_engineProjectLocation + "app/src/main.cpp", params.tempPath + "main.cpp");
	const bool headerCopyResult = CopyUtils::ExecuteCopyEntries();
	if (!headerCopyResult)
	{
		return CompileResult::ERROR_FILE_COPY;
	}

	std::shared_ptr<PlatformSettingsWindows> platformSettings = std::dynamic_pointer_cast<PlatformSettingsWindows>(params.buildPlatform.settings);
	if (platformSettings && platformSettings->icon)
	{
		// Copy game icon
		CopyUtils::AddCopyEntry(false, platformSettings->icon->m_file->GetPath(), params.tempPath + "logo.ico");
	}
	else
	{
		// Copy default icon
		CopyUtils::AddCopyEntry(false, s_engineProjectLocation + "app/res/logo.ico", params.tempPath + "logo.ico");
	}
	CopyUtils::AddCopyEntry(false, s_engineProjectLocation + "app/res/res.rc", params.tempPath + "res.rc");
	const bool iconCopyResult = CopyUtils::ExecuteCopyEntries();
	if (!iconCopyResult)
	{
		return CompileResult::ERROR_FILE_COPY;
	}

	std::vector<std::string> sourceDestFolders;

	// Copy source code
	try
	{
		sourceDestFolders = CopyGameSource(params);
	}
	catch (const std::exception&)
	{
		return CompileResult::ERROR_GAME_CODE_COPY;
	}

	// Setup compiler command
	std::string command = GetStartCompilerCommand();
	command += GetAddNextCommand();
	command += GetNavToEngineFolderCommand(params);
	command += GetAddNextCommand();
	command += GetCompileGameLibCommand(params, sourceDestFolders);
	if (params.buildType != BuildType::EditorHotReloading)
	{
		command += GetAddNextCommand();
		command += GetCompileIconCommand(params);
		command += GetAddNextCommand();
		command += GetCompileExecutableCommand(params);
	}

	Debug::Print("[Compiler::Compile] Command: " + command, true);
	// Run compilation
	const int buildResult = system(command.c_str());
	if (buildResult != 0)
	{
		return CompileResult::ERROR_UNKNOWN;
	}

	// Copy compiled files to export path
	if (params.buildType == BuildType::EditorHotReloading)
	{
		const std::string editor_dll_name = params.getEditorDynamicLibraryName();
		CopyUtils::AddCopyEntry(false, params.tempPath + editor_dll_name, params.exportPath + editor_dll_name);
	}
	else
	{
		const std::string dll_name = params.getDynamicLibraryName();
		CopyUtils::AddCopyEntry(false, params.tempPath + dll_name, params.exportPath + dll_name);
		CopyUtils::AddCopyEntry(false, params.tempPath + ProjectManager::GetGameName() + ".exe", params.exportPath + ProjectManager::GetGameName() + ".exe");
	}
	const bool gameCopyResult = CopyUtils::ExecuteCopyEntries();
	if (!gameCopyResult)
	{
		return CompileResult::ERROR_FINAL_GAME_FILES_COPY;
	}

	return CompileResult::SUCCESS;
}

// Deprecated
bool Compiler::CreateDockerImage()
{
	const std::string dockerFile = s_engineProjectLocation + "app/packaging/Dockerfile";
	const int result = system(("docker build -f \"" + dockerFile + "\" -t xenity_1_0_0 . 1>nul").c_str());
	return result == 0;
}

void Compiler::CancelCompilation()
{
	if (s_compilationMethod == CompilationMethod::DOCKER)
	{
		system("docker stop -t 0 XenityEngineBuild");
		s_isCompilationCancelled = true;
	}
}

CompileResult Compiler::CompileInDocker(const CompilerParams& params)
{
	s_compilationMethod = CompilationMethod::DOCKER;
	Benchmark dockerPreparationBenchmark;
	dockerPreparationBenchmark.Start();
	DockerState state = CheckDockerState(nullptr);
	if (state == DockerState::NOT_INSTALLED)
	{
		// Open the docker config menu if docker is not installed
		Editor::GetMenu<DockerConfigMenu>()->SetActive(true);
		Editor::GetMenu<DockerConfigMenu>()->Focus();
		return CompileResult::ERROR_DOCKER_NOT_FOUND;
	}
	else if (state == DockerState::NOT_RUNNING)
	{
		const bool startResult = Editor::OpenExecutableFile(EngineSettings::values.dockerExePath);
		if (startResult)
		{
			// Check every 3 seconds if docker is running
			for (int i = 0; i < 10; i++)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(3000));
				state = CheckDockerState(nullptr);
				if (state != DockerState::NOT_RUNNING)
					break;
			}

			if (state == DockerState::NOT_RUNNING)
				return CompileResult::ERROR_DOCKER_NOT_RUNNING;
			else // Wait a little bit to be sure docker is operational
				std::this_thread::sleep_for(std::chrono::milliseconds(5000));
		}
		else
		{
			return CompileResult::ERROR_DOCKER_COULD_NOT_START;
		}
	}

	if (state == DockerState::MISSING_IMAGE)
	{
		Debug::PrintWarning("The docker image is missing, creating image... (This may takes few minutes)");
		if (!CreateDockerImage())
		{
			return CompileResult::ERROR_DOCKER_MISSING_IMAGE;
		}
	}

	// We have to stop and remove the container to recreate it
	[[maybe_unused]] const int stopResult = system("docker stop XenityEngineBuild");
	[[maybe_unused]] const int removeResult = system("docker container remove XenityEngineBuild");

	unsigned int threadNumber = std::thread::hardware_concurrency();
	if (threadNumber == 0) // This function may returns 0, use 1 instead
	{
		threadNumber = 1;
	}

	std::string createCommand;

	if (params.buildType == BuildType::BuildShaders)
	{
		createCommand = "docker create --name XenityEngineBuild xenity_1_0_0 /bin/bash -c -it \"cd /home/XenityBuild/ ; ./compile_shaders.sh\"";
	}
	else if (params.buildPlatform.platform == Platform::P_PS3)
	{
		std::string removeSourceCommand = "rm -r Source/editor/ ; rm Source/gl.c ; rm -r include/glad/ ; rm -r include/imgui/ ; rm -r include/implot/ ; rm -r include/SDL3/ ; rm -r include/KHR/ ;";
		std::string paramsStr = "";
		const std::shared_ptr<const PlatformSettingsPS3> platformSettings = std::dynamic_pointer_cast<PlatformSettingsPS3>(params.buildPlatform.settings);
		const std::string debugDefine = platformSettings->isDebugMode ? " DEBUG=1" : "";
		const std::string profilerDefine = platformSettings->enableProfiler ? " PROFILER=1" : "";
		paramsStr += debugDefine + profilerDefine;
		createCommand = "docker create --name XenityEngineBuild xenity_1_0_0 /bin/bash -c -it \"cd /home/XenityBuild/ ; " + removeSourceCommand + "make -j" + std::to_string(threadNumber) + paramsStr + "\"";
	}

	[[maybe_unused]] const int createResult = system(createCommand.c_str());

	if (params.buildType != BuildType::BuildShaders)
	{
		// Copy source code and libraries
		const std::string copyEngineSourceCommand = "docker cp \"" + s_engineProjectLocation + "engine/src\" XenityEngineBuild:\"/home/XenityBuild/Source\"";
		[[maybe_unused]] const int copyCodeResult = system(copyEngineSourceCommand.c_str()); // Engine's source code + (game's code but to change later)
		const std::string copyEngineLibrariesCommand = "docker cp \"" + s_engineProjectLocation + "third_party\" XenityEngineBuild:\"/home/XenityBuild/include\"";
		[[maybe_unused]] const int copyLibrariesResult = system(copyEngineLibrariesCommand.c_str()); // Engine's libraries
		const std::string copyMainCommand = "docker cp \"" + s_engineProjectLocation + "app/src/main.cpp\" XenityEngineBuild:\"/home/XenityBuild/Source/\"";
		[[maybe_unused]] const int copyMainResult = system(copyMainCommand.c_str()); // main.cpp file

		// Copy the PS3 make file
		const std::string copyMakeFileCommand = "docker cp \"" + s_engineProjectLocation + "app/packaging/Makefile.PS3\" XenityEngineBuild:\"/home/XenityBuild/Makefile\"";
		[[maybe_unused]] const int copyMakeFileCommandResult = system(copyMakeFileCommand.c_str());

		// Copy cache for faster compilation
		if (params.buildPlatform.settings->useCompilationCache)
		{
			if (CompilerCache::CheckIfCacheExists(params.buildPlatform.platform))
			{
				const std::string copyCacheCommand = "docker cp \"" + CompilerCache::GetCachePath(params.buildPlatform.platform) + "build/\" XenityEngineBuild:\"/home/XenityBuild/\"";
				[[maybe_unused]] const int copyCacheResult = system(copyCacheCommand.c_str());
			}
		}

		// Copy source code in the build folder
		try
		{
			CopyGameSource(params);
		}
		catch (const std::exception&)
		{
			return CompileResult::ERROR_GAME_CODE_COPY;
		}

		// Copy game source from the build folder to docker
		const std::string copyGameSourceCommand = "docker cp \"" + params.tempPath + "source\" XenityEngineBuild:\"/home/XenityBuild/Source/game_code/\"";
		[[maybe_unused]] const int copyGameSourceResult = system(copyGameSourceCommand.c_str());
	}
	else
	{
		FixCompileShadersScript(params.tempPath + "compile_shaders.sh");

		const std::string copyCompileShadersCommand = "docker cp \"" + params.tempPath + "compile_shaders.sh\" XenityEngineBuild:\"/home/XenityBuild/compile_shaders.sh\"";
		[[maybe_unused]] const int copyCompileShadersCommandResult = system(copyCompileShadersCommand.c_str()); // compile shaders script file
	}

	if (s_isCompilationCancelled)
	{
		return CompileResult::ERROR_COMPILATION_CANCELLED;
	}

	// Copy XMB/Livearea images
	if (params.buildPlatform.platform == Platform::P_PS3)
	{
		CopyAssetsToDocker(params);
	}

	if (s_isCompilationCancelled)
	{
		return CompileResult::ERROR_COMPILATION_CANCELLED;
	}
	dockerPreparationBenchmark.Stop();
	s_timings.prepareDockerTime = dockerPreparationBenchmark.GetMicroSeconds();

	Benchmark dockerCompilationBenchmark;
	dockerCompilationBenchmark.Start();
	// Start compilation
	const std::string startCommand = "docker start -a XenityEngineBuild";
	[[maybe_unused]] const int startResult = system(startCommand.c_str());
	dockerCompilationBenchmark.Stop();
	s_timings.dockerCompileTime = dockerCompilationBenchmark.GetMicroSeconds();

	if (s_isCompilationCancelled)
	{
		return CompileResult::ERROR_COMPILATION_CANCELLED;
	}


	if (params.buildType == BuildType::BuildShaders)
	{
		if (startResult != 0)
		{
			return CompileResult::ERROR_DOCKER_SHADERS_COMPILATION;
		}

		const std::string copyCompiledShadersCommand = "docker cp XenityEngineBuild:\"/home/XenityBuild/shaders_to_compile/\" \"" + params.tempPath + "cooked_assets/" + "\"";
		[[maybe_unused]] const int copyCompiledShadersCommandResult = system(copyCompiledShadersCommand.c_str());
	}
	else
	{
		if (startResult != 0)
		{
			return CompileResult::ERROR_DOCKER_COMPILATION;
		}

		const std::string fileName = "XenityBuild.self";

		// Copy cache files
		const std::string cacheFolder = params.tempPath + "docker_cache/";
		fs::create_directories(cacheFolder);
		const std::string copyCacheCommand = "docker cp XenityEngineBuild:\"/home/XenityBuild/build/\" \"" + cacheFolder + "\"";
		[[maybe_unused]] const int copyCacheCommandResult = system(copyCacheCommand.c_str());

		CompilerCache::UpdateCache(cacheFolder, params.buildPlatform.platform);

		// Copy final file
		std::string copyGameFileCommand = "docker cp XenityEngineBuild:\"/home/XenityBuild/build/" + fileName + "\" \"" + params.exportPath + fileName + "\"";
		if (params.buildPlatform.platform == Platform::P_PS3)
		{
			copyGameFileCommand = "docker cp XenityEngineBuild:\"/home/XenityBuild/" + fileName + "\" \"" + params.exportPath + fileName + "\"";
		}
		const int copyGameFileResult = system(copyGameFileCommand.c_str()); // Engine's source code + (game's code but to change later)

		const std::string fakeSelfFileName = "XenityBuild.fake.self";
		const std::string copyFakeSelfCommand = "docker cp XenityEngineBuild:\"/home/XenityBuild/" + fakeSelfFileName + "\" \"" + params.exportPath + fakeSelfFileName + "\"";
		[[maybe_unused]] const int copyFakeSelfResult = system(copyFakeSelfCommand.c_str());

		if (copyGameFileResult != 0)
		{
			return CompileResult::ERROR_DOCKER_COMPILATION;
		}
	}

	return CompileResult::SUCCESS;
}

void Compiler::CopyAssetsToDocker(const CompilerParams& params)
{
	if (params.buildPlatform.platform == Platform::P_PS3)
	{
		const std::string copyShadersCommand = "docker cp \"" + params.tempPath + "cooked_assets/shaders_to_compile\" XenityEngineBuild:\"/home/XenityBuild\"";
		[[maybe_unused]] const int copyShadersCommandResult = system(copyShadersCommand.c_str());
	}
}

void Compiler::FixCompileShadersScript(const std::string& outputPath)
{
	// In case the script has windows line endings, remove them to avoid errors on linux
	std::shared_ptr<File> shaderScriptFile = FileSystem::MakeFile(s_engineProjectLocation + "app/packaging/compile_shaders.sh");
	if (!shaderScriptFile->Open(FileMode::ReadOnly)) 
	{
		Debug::PrintError("[Compiler::FixCompileShadersScript] Failed to open compile_shaders.sh");
		return;
	}
	std::string scriptText = shaderScriptFile->ReadAll();
	shaderScriptFile->Close();

	size_t scriptTextSize = scriptText.size();
	for (size_t i = 0; i < scriptTextSize; i++)
	{
		if (scriptText[i] == '\r')
		{
			scriptText.erase(i, 1);
			scriptTextSize--;
		}
	}

	FileSystem::Delete(outputPath);
	// Write the new script
	std::shared_ptr<File> updatedShaderScriptFile = FileSystem::MakeFile(outputPath);
	if (!updatedShaderScriptFile->Open(FileMode::WriteCreateFile))
	{
		Debug::PrintError("[Compiler::FixCompileShadersScript] Failed to create compile_shaders_fixed.sh");
		return;
	}
	updatedShaderScriptFile->Write(scriptText);
	updatedShaderScriptFile->Close();
}

std::string Compiler::GetStartCompilerCommand()
{
	const std::string path = EngineSettings::values.compilerPath;

	std::string command;
	if (fs::path(path).is_absolute())
	{
		command += path.substr(0, 2) + " && "; // Go to the compiler folder
	}
	command += "cd \"" + EngineSettings::values.compilerPath + "\""; // Go to the compiler folder
	command += " && " + s_compilerExecFileName; // Start the compiler
	//command += " >nul";	// Mute output
	return command;
}

std::string Compiler::GetAddNextCommand()
{
	const std::string command = " && ";
	return command;
}

std::string Compiler::GetNavToEngineFolderCommand(const CompilerParams& params)
{
	std::string command;
	command += params.tempPath.substr(0, 2) + " && "; // Change current drive
	command += "cd \"" + params.tempPath + "\"";
	return command;
}

std::string Compiler::GetCompileGameLibCommand(const CompilerParams& params, const std::vector<std::string>& sourceDestFolders)
{
	std::string command = "";
	// MP for multithreading (faster compilation)
	// EHsc for exceptions
	// MD Use dll to compile (MDd for debug mode)
	// DIMPORT define "IMPORT"
#if defined(DEBUG)
	command += "cl /std:c++17 /MP /EHsc /MDd /DDEBUG /DIMPORT /DNOMINMAX"; // Start compilation
#else
	command += "cl /std:c++17 /O2 /MP /EHsc /MD /DIMPORT /DNOMINMAX"; // Start compilation
#endif

	// Define "EDITOR" if compiled to play game in editor
	if (params.buildType == BuildType::EditorHotReloading)
	{
		command += " /DEDITOR";
	}

	// Add include directories
	command += " -I \"" + s_engineProjectLocation + "third_party\"";
	command += " -I \"" + s_engineProjectLocation + "engine/src\"";

	// Create DLL
	command += " /LD";

	// Add all source folders
	const size_t sourceDestFoldersSize = sourceDestFolders.size();
	for (size_t i = 0; i < sourceDestFoldersSize; i++)
	{
		command += " \"" + sourceDestFolders[i] + "*.cpp\"";
	}

	// Add the .lib file to use
	if (params.buildType != BuildType::EditorHotReloading)
	{
		command += " " + std::string(ENGINE_GAME_FOLDER) + ".lib";
	}
	else
	{
		command += " " + std::string(ENGINE_EDITOR_FOLDER) + ".lib";
	}

	command += " /link";
	// Set .lib output file name
	command += " /implib:" + params.libraryName + ".lib";
	//command += " /DEBUG"; ///????
	// Set dll output file name
	if (params.buildType != BuildType::EditorHotReloading)
	{
		command += " /out:" + params.getDynamicLibraryName();
	}
	else
	{
		command += " /out:" + params.getEditorDynamicLibraryName();
	}

	//command += " >nul"; // Mute output
	return command;
}

std::string Compiler::GetCompileIconCommand(const CompilerParams& params)
{
	std::string command;
	//Buid game resource
	command = "rc res.rc";
	//command += " >nul"; // Mute output
	return command;
}

std::string Compiler::GetCompileExecutableCommand(const CompilerParams& params)
{
	std::string command;
	//Buid game exe
	command = "cl /Fe\"" + ProjectManager::GetGameName() + ".exe\" res.res /std:c++17 /MP /EHsc /DNOMINMAX";
#if !defined(DEBUG)
	command += " /O2";
#endif
	command += " -I \"" + s_engineProjectLocation + "third_party\"";
	command += " -I \"" + s_engineProjectLocation + "engine/src\"";
	command += " main.cpp " + std::string(ENGINE_GAME_FOLDER) + ".lib";
	//command += " >nul"; // Mute output
	return command;
}

void Compiler::StartGame(Platform platform, const std::string& exportPath)
{
	XASSERT(!exportPath.empty(), "[Compiler::StartGame] exportPath is empty");
	if (exportPath.empty())
		return;

	std::string command = "";
	if (platform == Platform::P_Windows)
	{
		const std::string fileName = ProjectManager::GetGameName();
		command = "cd \"" + exportPath + "\"" + " && " + "\"" + fileName + ".exe\"";
	}


	if (!command.empty())
		system(command.c_str());
}
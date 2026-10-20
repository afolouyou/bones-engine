// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Xenity Engine

#include "file_system.h"

#include <string>

#if defined(__PS3__)
#include <lv2/sysfs.h>
#else
#include <filesystem>
#endif

#if defined(_WIN32) || defined(_WIN64) || defined(__LINUX__)
#include <fstream>
#endif

#include <engine/debug/debug.h>
#include <engine/constants.h>
#include <engine/application.h>

#include "directory.h"
#include "file.h"
#include "file_ps3.h"
#include "file_default.h"

#pragma region File

#pragma endregion

#pragma region Directory

bool FileSystem::Rename(const std::string& path, const std::string& newPath)
{
	bool success = true;
	try
	{
#if defined(_WIN32) || defined(_WIN64) || defined(__LINUX__)
		if (std::filesystem::exists(newPath))
		{
			success = false;
		}
		else
		{
			std::filesystem::rename(path, newPath);
		}
#endif
	}
	catch (const std::exception&)
	{
		success = false;
	}
	return success;
}

bool FileSystem::WriteFileSafely(const std::string& path, const std::string& data)
{
#if defined(_WIN32) || defined(_WIN64) || defined(__LINUX__)
	const std::string tempPath = path + ".tmp";
	try
	{
		{
			std::ofstream tempFile(tempPath, std::ios::binary | std::ios::trunc);
			if (!tempFile.is_open())
			{
				Debug::PrintError("[FileSystem::WriteFileSafely] Fail to create the temporary file: " + tempPath, true);
				return false;
			}
			tempFile.write(data.data(), static_cast<std::streamsize>(data.size()));
			tempFile.flush();
			if (!tempFile.good())
			{
				tempFile.close();
				std::filesystem::remove(tempPath);
				Debug::PrintError("[FileSystem::WriteFileSafely] Fail to write the temporary file: " + tempPath, true);
				return false;
			}
		}

		if (std::filesystem::file_size(tempPath) != data.size())
		{
			std::filesystem::remove(tempPath);
			Debug::PrintError("[FileSystem::WriteFileSafely] The temporary file is incomplete: " + tempPath, true);
			return false;
		}

		// Replace the original file (atomic on the same drive)
		std::filesystem::rename(tempPath, path);
	}
	catch (const std::exception& e)
	{
		std::error_code ec;
		std::filesystem::remove(tempPath, ec);
		Debug::PrintError("[FileSystem::WriteFileSafely] Fail to write the file: " + path + " (" + e.what() + ")", true);
		return false;
	}
	return true;
#else
	Delete(path);
	const std::shared_ptr<File> file = MakeFile(path);
	if (!file->Open(FileMode::WriteCreateFile))
	{
		return false;
	}
	file->Write(data);
	file->Close();
	return true;
#endif
}

CopyFileResult FileSystem::CopyFile(const std::string& path, const std::string& newPath, bool replace)
{
	CopyFileResult result = CopyFileResult::Success;
#if !defined(__PS3__)
	try
	{
		std::filesystem::copy_options option = std::filesystem::copy_options::none;
		if (!replace)
		{
			if (std::filesystem::exists(newPath))
			{
				result = CopyFileResult::FileAlreadyExists;
				return result;
			}
		}
		else
		{
			option |= std::filesystem::copy_options::overwrite_existing;
		}

		std::filesystem::copy_file(path, newPath, option);
	}
	catch (const std::exception&)
	{
		result = CopyFileResult::Failed;
	}
#endif
	return result;
}

//std::vector<std::shared_ptr<File>> files;

std::shared_ptr<File> FileSystem::MakeFile(const std::string& path)
{
	XASSERT(!path.empty(), "[FileSystem::MakeFile] path is empty");

	std::shared_ptr<File> file = nullptr;

	/*size_t fileCount = files.size();
	for (size_t i = 0; i < fileCount; i++)
	{
		if (files[i]->GetPath() == path)
		{
			file = files[i];
			break;
		}
	}*/

	/*if (!file)
	{*/
#if defined(__PS3__)
	file = std::make_shared<FilePS3>(path);
#else
	file = std::make_shared<FileDefault>(path);
#endif
	//files.push_back(file);
//}

	return file;
}

std::shared_ptr<Directory> FileSystem::MakeDirectory(const std::string& path)
{
	XASSERT(!path.empty(), "[FileSystem::MakeDirectory] path is empty");

	return std::make_shared<Directory>(path);
}

std::string FileSystem::ConvertWindowsPathToBasicPath(const std::string& path)
{
	const size_t pathSize = path.size();
	std::string newPath = path;
	for (size_t i = 0; i < pathSize; i++)
	{
		if (path[i] == '\\')
		{
			newPath[i] = '/';
		}
	}
	return newPath;
}

std::string FileSystem::ConvertBasicPathToWindowsPath(const std::string& path)
{
	const size_t pathSize = path.size();
	std::string newPath = path;
	for (size_t i = 0; i < pathSize; i++)
	{
		if (path[i] == '/')
		{
			newPath[i] = '\\';
		}
	}
	return newPath;
}


#pragma endregion

#pragma region Read/Input

#pragma endregion

bool FileSystem::CreateFolder(const std::string& path)
{
	XASSERT(!path.empty(), "[FileSystem::CreateFolder] path is empty");
	bool result = true;
#if defined(__PS3__)
	std::string tempPath = path;
	if (tempPath[0] != '/')
	{
		tempPath = Application::GetGameDataFolder() + path;
	}
	sysFsMkdir(tempPath.c_str(), 0777);
#else

	try
	{
		std::filesystem::create_directories(path);
	}
	catch (const std::exception&)
	{
		result = false;
	}
#endif
	return result;
}

void FileSystem::Delete(const std::string& path)
{
	XASSERT(!path.empty(), "[FileSystem::Delete] path is empty");

	std::string tempPath = path;


#if defined(__PS3__)
	if (tempPath[0] != '/')
	{
		tempPath = Application::GetGameDataFolder() + path;
	}
	sysFsRmdir(tempPath.c_str()); // Remove if dir
	sysFsUnlink(tempPath.c_str()); // Remove if file
#else
	try
	{
		std::filesystem::remove_all(tempPath.c_str());
	}
	catch (const std::exception&)
	{
	}
#endif
}

#pragma region Write/Output

#pragma endregion

int FileSystem::InitFileSystem()
{
	Debug::Print("-------- File System initiated --------", true);
	return 0;
}
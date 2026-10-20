// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Bones Engine

#include "directory.h"

#if !defined(__PS3__)
#include <filesystem>
#endif

#include <engine/assertions/assertions.h>
#include <engine/debug/debug.h>
#include "file_system.h"

Directory::Directory(std::string _path) : UniqueId(true)
{
	_path = FileSystem::ConvertWindowsPathToBasicPath(_path);

	m_path = _path;
}

Directory::~Directory()
{
	subdirectories.clear();
	files.clear();
}

void AddDirectoryFiles(std::vector<std::shared_ptr<File>> &vector, const Directory& directory)
{
	const size_t fileCount = directory.files.size();
	for (size_t i = 0; i < fileCount; i++)
	{
		vector.push_back(directory.files[i]);
	}

	const size_t directoryCount = directory.subdirectories.size();
	for (size_t i = 0; i < directoryCount; i++)
	{
		AddDirectoryFiles(vector, *directory.subdirectories[i]);
	}
}

void Directory::FillDirectory(Directory& directory, bool recursive)
{
	directory.files.clear();
	directory.subdirectories.clear();
	if (!directory.CheckIfExist())
	{
		return;
	}
#if defined(__PS3__)
#else
	// Use error codes: an unreadable or deleted folder must not throw and crash the project loading/refresh
	std::error_code iteratorError;
	std::filesystem::directory_iterator directoryIterator(directory.GetPath(), iteratorError);
	if (iteratorError)
	{
		Debug::PrintError("[Directory::FillDirectory] Cannot read the folder: " + directory.GetPath() + " (" + iteratorError.message() + ")", true);
		return;
	}
	const std::filesystem::directory_iterator directoryIteratorEnd;
	for (; directoryIterator != directoryIteratorEnd; directoryIterator.increment(iteratorError))
	{
		if (iteratorError)
		{
			Debug::PrintError("[Directory::FillDirectory] Error while reading the folder: " + directory.GetPath() + " (" + iteratorError.message() + ")", true);
			break;
		}
		const std::filesystem::directory_entry& file = *directoryIterator;
		std::error_code typeError;
		if (file.is_directory(typeError))
		{
			std::shared_ptr<Directory> newDirectory = nullptr;
			try
			{
				std::string path = file.path().string();

				newDirectory = std::make_shared<Directory>(path + "/");
				if (recursive)
					newDirectory->GetAllFiles(true);
				directory.subdirectories.push_back(newDirectory);
			}
			catch (const std::exception&)
			{
			}
		}
		else if (file.is_regular_file(typeError))
		{
			std::shared_ptr<File> newFile = nullptr;
			try
			{
				std::string path = file.path().string();
				newFile = FileSystem::MakeFile(path);
				directory.files.push_back(newFile);
			}
			catch (const std::exception&)
			{
			}
		}
	}
#endif
}

std::vector<std::shared_ptr<File>> Directory::GetAllFiles(bool recursive)
{
	FillDirectory(*this, recursive);
	std::vector<std::shared_ptr<File>> vector;
	AddDirectoryFiles(vector, *this);
	return vector;
}

bool Directory::CheckIfExist() const
{
	bool exists = false;
#if defined(__PS3__)
#else
	exists = std::filesystem::exists(m_path);
#endif
	return exists;
}
//==============================================================================
/**
@file       ESDUtilitiesLinux.cpp

@brief      Various filesystem and other utility functions (Linux)

@copyright  (c) 2018, Corsair Memory, Inc.
			This source code is licensed under the MIT-style license found in the LICENSE file.

**/
//==============================================================================

#include "ESDUtilities.h"

#include <limits.h>
#include <unistd.h>

static bool HasSuffix(const std::string& inString, const std::string& inSuffix)
{
	return (inString.length() >= inSuffix.length()) && (inSuffix.length() > 0) && (inString.compare(inString.size() - inSuffix.size(), inSuffix.size(), inSuffix) == 0);
}

void ESDUtilities::DoSleep(int inMilliseconds)
{
	usleep(1000 * inMilliseconds);
}


static char GetFileSystemPathDelimiter()
{
	return '/';
}


std::string ESDUtilities::AddPathComponent(const std::string &inPath, const std::string &inComponentToAdd)
{
	if (inPath.size() <= 0)
		return inComponentToAdd;

	char delimiter = GetFileSystemPathDelimiter();
	char lastChar = inPath[inPath.size() - 1];

	bool pathEndsWithDelimiter   = (delimiter == lastChar			) || ('/' == lastChar);
	bool compStartsWithDelimiter = (delimiter == inComponentToAdd[0]) || ('/' == inComponentToAdd[0]);

	std::string result;
	if (pathEndsWithDelimiter && compStartsWithDelimiter)
		result = inPath + inComponentToAdd.substr(1);
	else if (pathEndsWithDelimiter || compStartsWithDelimiter)
		result = inPath + inComponentToAdd;
	else
		result = inPath + GetFileSystemPathDelimiter() + inComponentToAdd;

	return result;
}

std::string ESDUtilities::GetFolderPath(const std::string& inPath)
{
	//
	// Use the platform specific delimiter
	//
	std::string delimiterString = std::string(1, GetFileSystemPathDelimiter());

	//
	// Remove the trailing delimiters
	//
	std::string pathWithoutTrailingDelimiters = inPath;
	while (pathWithoutTrailingDelimiters.length() > delimiterString.length() && HasSuffix(pathWithoutTrailingDelimiters, delimiterString))
	{
		pathWithoutTrailingDelimiters = pathWithoutTrailingDelimiters.substr(0, pathWithoutTrailingDelimiters.length() - delimiterString.length());
	}

	size_t pos = pathWithoutTrailingDelimiters.find_last_of(delimiterString);
	if (std::string::npos != pos)
	{
		std::string foundPath = inPath.substr(0, pos);

		//
		// Remove the trailing delimiters
		//
		std::string foundPathWithoutTrailingDelimiters = foundPath;
		while (foundPathWithoutTrailingDelimiters.length() > delimiterString.length() && HasSuffix(foundPathWithoutTrailingDelimiters, delimiterString))
		{
			foundPathWithoutTrailingDelimiters = foundPathWithoutTrailingDelimiters.substr(0, foundPathWithoutTrailingDelimiters.length() - delimiterString.length());
		}

		if (foundPathWithoutTrailingDelimiters.empty() && delimiterString == "/")
		{
			return "/";
		}

		return foundPathWithoutTrailingDelimiters;
	}

	return "";
}

std::string ESDUtilities::GetPluginPath()
{
	static std::string sPluginPath;

	if (sPluginPath.empty())
	{
		char exePath[PATH_MAX];
		ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
		if (len > 0)
		{
			exePath[len] = '\0';

			//
			// Mirror the macOS implementation: walk up from the executable
			// until a component with the .sdPlugin extension is found
			//
			std::string checkPath = GetFolderPath(exePath);
			while (!checkPath.empty() && checkPath != "/")
			{
				if (HasSuffix(checkPath, ".sdPlugin"))
				{
					sPluginPath = checkPath;
					break;
				}

				std::string parentPath = GetFolderPath(checkPath);
				if (parentPath == checkPath)
				{
					break;
				}
				checkPath = parentPath;
			}

			if (sPluginPath.empty())
			{
				//
				// Fall back to the executable's folder: with OpenDeck the
				// binary sits directly inside the .sdPlugin directory
				//
				sPluginPath = GetFolderPath(exePath);
			}
		}
	}

	return sPluginPath;
}

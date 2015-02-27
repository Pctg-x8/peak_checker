#pragma once

#include <string>
#include <sstream>
#include <forward_list>

#include <Windows.h>

class fileList
{
public:
	using FileList = std::forward_list < std::pair<std::wstring, std::wstring> > ;

	static FileList listDirectory(const std::wstring& dir);
};

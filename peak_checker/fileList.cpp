#include "fileList.h"

fileList::FileList fileList::listDirectory(const std::wstring& dir)
{
	std::wstring dir_listing = dir;
	if(dir.empty())
	{
		wchar_t buffer[MAX_PATH] = { };
		GetCurrentDirectory(MAX_PATH, buffer);
		dir_listing = buffer;
	}
	if(dir_listing.at(dir_listing.length() - 1) != L'\\') dir_listing.push_back(L'\\');

	std::wstringstream listing_filter;
	listing_filter << dir_listing << L"*.wav";

	fileList::FileList list;
	WIN32_FIND_DATA fd = { };
	auto hFind = FindFirstFile(listing_filter.str().c_str(), &fd);
	if(hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			std::wstringstream fullPath;
			fullPath << dir_listing << fd.cFileName;
			list.push_front(std::make_pair(std::wstring(fd.cFileName), fullPath.str()));
		} while(FindNextFile(hFind, &fd));
		FindClose(hFind);
	}

	return list;
}

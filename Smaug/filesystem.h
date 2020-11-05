#pragma once
namespace filesystem
{
	bool SaveFile(const char* path, char* data);
	bool SaveFileWithDialog(char* data, const char* fileType = "*.txt");
	char* LoadFile(const char* path, size_t& length);
};


#pragma once
#include <string_view>
#include <string>

class Client
{
private:
	std::wstring pipeName;
	void* hPipe;

public:
	Client(std::wstring_view pipeName);
	Client() : Client(L"\\\\.\\pipe\\mynamedpipe") {};
	bool connect();
	void halt();
	~Client() {};
};


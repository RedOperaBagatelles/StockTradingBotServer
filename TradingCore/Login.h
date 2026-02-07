#pragma once

#include "Core/Log.h"

#include <string>

class Login
{
public:
	// 접근토큰 발급 함수
	static std::string GetAccessToken();
	static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp);

private:
	Log& log = Log::GetInstance();
};
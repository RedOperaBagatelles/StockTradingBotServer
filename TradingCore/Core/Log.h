#pragma once

#include <string>

class Log
{
public:
	enum class Level { INFO, WARNING, ERROR, COUNT };

	static Log& GetInstance();
	
	void LogMessage(Level level, const char* message);
	void LogConsole(Level level, const char* message);

private:
	Log();
	Log(const Log&) = delete;
	Log& operator=(const Log&) = delete;

	constexpr static const char* GetLevelString(Level level);
	static std::string GetCurrentTimeString();

	std::string LogWrite(Level level, const char* message); // 로그 메시지 기록 메소드
};

inline constexpr const char* Log::GetLevelString(Level level)
{
	switch (level)
	{
	case Level::INFO:       return "INFO";
	case Level::WARNING:    return "WARNING";
	case Level::ERROR:      return "ERROR";
	default:                return "UNKNOWN";
	}
}
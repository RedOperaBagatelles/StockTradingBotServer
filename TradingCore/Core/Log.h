#pragma once

#include <string>

#define MAKE_LEVEL_TYPE \
	X(NORMAL) \
	X(INFO) \
	X(WARNING) \
	X(ERROR)

class Log
{
public:
#define X(type) type,
	enum class Level { MAKE_LEVEL_TYPE COUNT };
#undef X

	static Log& GetInstance();

	void LogMessage(Level level, const char* message);
	void LogConsole(Level level, const char* message);

private:
	Log();
	Log(const Log&) = delete;
	Log& operator=(const Log&) = delete;

	constexpr static const char* GetLevelString(Level level);
	static std::string GetCurrentTimeString();

	static const void SetConsoleColor(Log::Level level);		// Log Level에 따라서 색상 설정 메소드
	static const void ResetConsoleColor();						// 콘솔 색상 리셋 메소드
	static const char* GetAnsiColorPrefix(Log::Level level);	// Log Level에 따라서 ANSI 색상 코드 접두사 반환 (로그 파일에 삽입용)

	std::string LogWrite(Level level, const char* message);		// 로그 메시지 기록 메소드
};

// 로그 레벨을 문자열로 변환
inline constexpr const char* Log::GetLevelString(Level level)
{
#define X(type) case Level::type: return #type;
	switch (level)
	{
		MAKE_LEVEL_TYPE

		default:
			return "UNKNOWN";
	}
#undef X
}
#include "Log.h"
#include "Config.h"

#include "Utility/Directory.h"

#include <assert.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <mutex>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

static std::mutex g_logMutex;

const void Log::SetConsoleColor(Log::Level level)
{
#ifdef _WIN32
	static HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);   // 콘솔 핸들
	static WORD originalAttrs = 0;                              // 원래 속성 저장용
	static bool originalStored = false;                         // 원래 속성 저장 여부

    if (!originalStored)
    {
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (GetConsoleScreenBufferInfo(hConsole, &info))
        {
            originalAttrs = info.wAttributes;
            originalStored = true;
        }
    }

    WORD attrs = originalStored ? originalAttrs : (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

    switch (level)
    {
    case Log::Level::NORMAL:
        attrs = originalStored ? originalAttrs : (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        break;

    case Log::Level::INFO:
        attrs = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        break;

    case Log::Level::WARNING:
        attrs = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY; // yellow-ish
        break;

    case Log::Level::ERROR:
        attrs = FOREGROUND_RED | FOREGROUND_INTENSITY;
        break;

    default:
        break;
    }

    SetConsoleTextAttribute(hConsole, attrs);
#else
    switch (level)
    {
    case Log::Level::NORMAL:
        std::cout << "\033[0m"; // reset
        break;
    case Log::Level::INFO:
        std::cout << "\033[32m"; // green
        break;
    case Log::Level::WARNING:
        std::cout << "\033[33m"; // yellow
        break;
    case Log::Level::ERROR:
		// Error는 굵은 빨간색으로 표시
        std::cout << "\033[1;31m"; // bold red
        break;
    default:
        std::cout << "\033[0m";
        break;
    }
#endif
}

const void Log::ResetConsoleColor()
{
#ifdef _WIN32
    static HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;

    if (GetConsoleScreenBufferInfo(hConsole, &info))
        SetConsoleTextAttribute(hConsole, info.wAttributes);

#else
    std::cout << "\033[0m";

#endif
}

const char* Log::GetAnsiColorPrefix(Log::Level level)
{
    switch (level)
    {
    case Log::Level::NORMAL:  return "\033[0m";     // reset / default
    case Log::Level::INFO:    return "\033[32m";    // green
    case Log::Level::WARNING: return "\033[33m";    // yellow
    case Log::Level::ERROR:   return "\033[1;31m";  // bold red
    default:                  return "\033[0m";
    }
}

Log& Log::GetInstance()
{
    static Log instance;

    return instance;
}

// 현재 시간 문자열 반환 함수
std::string Log::GetCurrentTimeString()
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};

#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    char timeBuf[64];
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &tm);

    return std::string(timeBuf);
}

// 로그 메시지 포맷팅 ([시간] [레벨] 메시지)
std::string Log::LogWrite(Level level, const char* message)
{
    std::ostringstream oss;
    oss << "[" << GetCurrentTimeString() << "] [" << GetLevelString(level) << "] " << message;
    return oss.str();
}

void Log::LogConsole(Level level, const char* message)
{
    // 스레드 안전하게 콘솔에 로그 출력
    std::lock_guard<std::mutex> lock(g_logMutex);
    std::string line = LogWrite(level, message);

	// Level에 따라 콘솔 색상 설정 후 출력, 리셋
    SetConsoleColor(level);
    std::cout << line << std::endl;
    ResetConsoleColor();
}

void Log::LogMessage(Level level, const char* message)
{
    std::lock_guard<std::mutex> lock(g_logMutex);

    // 포맷된 로그 메시지 생성
    std::string line = LogWrite(level, message);

	// log 파일 경로 결정 : Config::rootPath의 절대 경로 + "/log.txt"
    std::string dir = Directory::GetAbsolutePath(std::string(Config::rootPath));

	// log 디렉터리 생성 시도
#ifdef _WIN32
    char sep = '\\';
#else
    char sep = '/';
#endif

    if (!dir.empty() && dir.back() != '/' && dir.back() != '\\')
        dir.push_back(sep);

    std::string logPath = dir + "log.txt";

    // 로그 메시지를 파일에 기록
    std::ofstream logFile;
    logFile.open(logPath, std::ios::app);

    if (!logFile.is_open())
    {
        // 폴백: 현재 작업 디렉터리에 log.txt를 생성
        const char* localPath = "log.txt";
        logFile.open(localPath, std::ios::app);
    }

    if (logFile.is_open())
    {
		// 탑재된 ANSI 색상 코드를 사용하여 로그 파일에 기록
        const char* colorPrefix = GetAnsiColorPrefix(level);
        const char* colorSuffix = "\033[0m"; // reset

        logFile << colorPrefix << line << colorSuffix << std::endl;
        logFile.close();
    }
}

Log::Log()
{
    // 로그 파일 경로 결정: Config::rootPath의 절대 경로 + "/log.txt"
    std::string dir = Directory::GetAbsolutePath(std::string(Config::rootPath));

    // 디렉터리 생성 시도
    bool isSucceedDir = Directory::EnsureDirectoryExists(dir);
    assert(isSucceedDir && "로그를 생성하기 위한 위치가 생성되지 않음");

    // log.txt 전체 경로 구성
#ifdef _WIN32
    char sep = '\\';
#else
    char sep = '/';
#endif

    if (!dir.empty() && dir.back() != '/' && dir.back() != '\\')
        dir.push_back(sep);

    std::string logPath = dir + "log.txt";

    // 로그 파일 존재 및 내용 확인
    bool hasLogFileAndNotEmpty = false;
    std::ifstream inputFileStream(logPath, std::ios::binary | std::ios::ate);

    if (inputFileStream.is_open())
    {
        // 파일 크기 확인
        std::streamsize size = inputFileStream.tellg();

        if (size > 0)
            hasLogFileAndNotEmpty = true;

        inputFileStream.close();
    }

    // 생성 또는 헤더 추가
    std::ofstream outFileStream(logPath, std::ios::app);

    if (outFileStream)
    {
        std::string timeStr = GetCurrentTimeString();

        if (hasLogFileAndNotEmpty)
			outFileStream << "\n---- 새로운 세션 시작 (기존 로그에 데이터 있음) [" << timeStr << "] ----\n\n";

        else
			outFileStream << "\n---- 새로운 세션 시작 (빈 로그 파일) [" << timeStr << "] ----\n\n";
    }

    else
    {
        // 루트에 생성 실패(권한 등). 현재 작업 디렉터리에 폴백하여 생성
        const char* localPath = "log.txt";
        std::ofstream outStream(localPath, std::ios::app);

        if (outStream)
        {
            std::string timeStr = GetCurrentTimeString();
			outStream << "\n---- 새로운 세션 시작 (폴백) [" << timeStr << "] ----\n\n";
        }
        // 실패해도 무시하고 계속 진행
    }
}
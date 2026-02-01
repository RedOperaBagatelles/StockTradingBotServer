#include "Log.h"
#include "Config.h"

#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <mutex>
#include <sstream>

static std::mutex g_logMutex;

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

    std::cout << line << std::endl;
}

void Log::LogMessage(Level level, const char* message)
{
    std::lock_guard<std::mutex> lock(g_logMutex);

	// 포맷된 로그 메시지 생성
    std::string line = LogWrite(level, message);

    // 로그 메시지를 log.txt 파일에 기록하는 기능 구현
    std::ofstream logFile;

    // 루트 디렉터리에 log.txt가 없으면 현재 작업 디렉터리에 생성
    logFile.open(Config::rootPath, std::ios::app);

    if (!logFile.is_open())
    {
        // 루트에 열리지 않으면 현재 작업 디렉터리에 생성 시도
        const char* localPath = "log.txt";
        logFile.open(localPath, std::ios::app);
    }

    if (logFile.is_open())
    {
        logFile << line << std::endl;
        logFile.close();
    }
}

Log::Log()
{
    // 루트 디렉터리에 log.txt가 있는지 확인하고 없으면 생성
    std::ifstream inputFileStream(Config::rootPath);

    // 이미 존재하면 종료
    if (inputFileStream.good())
        return;

    // 생성 시도
    std::ofstream outFileStream(Config::rootPath, std::ios::app);

    if (!outFileStream)
    {
        // 루트에 생성 실패(권한 등). 현재 작업 디렉터리에 폴백하여 생성
        const char* localPath = "log.txt";
        std::ofstream ofsLocal(localPath, std::ios::app);
        // 실패해도 무시하고 계속 진행
    }
}
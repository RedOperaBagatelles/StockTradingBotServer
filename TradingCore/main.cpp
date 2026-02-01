#include "Login.h"

#include "Core/Application.h"
#include "Core/Config.h"

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <libwebsockets.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct URLComponents
{
    std::string protocol;
    std::string host;
    int port;
    std::string path;
};

Log& g_log = Log::GetInstance();

// 전역 변수
std::string socketURL;
std::string accessToken;
std::atomic<bool> isKeepRunning(true);
std::atomic<bool> isConnected(false);
std::atomic<bool> isLoggedIn(false);

// 전송할 메시지 큐
std::string messageToSend;
std::atomic<bool> hasMessageToSend(false);

// WebSocket 콜백 함수
static int CallBackWebSocket(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t length)
{
    switch (reason)
    {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
        {
            g_log.LogConsole(Log::Level::INFO, "서버와 연결되었습니다.");
            g_log.LogMessage(Log::Level::INFO, "서버와 연결되었습니다.");

            isConnected = true;

            // 로그인 패킷 전송
            {
                json loginPacket =
                {
                    { "trnm", "LOGIN" },
                    { "token", accessToken }
                };

                messageToSend = loginPacket.dump();
                hasMessageToSend = true;
                std::cout << "실시간 시세 서버로 로그인 패킷을 전송합니다." << std::endl;
            }

            lws_callback_on_writable(wsi);

            break;
        }
		    

        case LWS_CALLBACK_CLIENT_RECEIVE:
        {
            std::string response((char*)in, length);

            try
            {
                json responseJson = json::parse(response);
                std::string trnm = responseJson.value("trnm", "");

                // PING 메시지 처리
                if (trnm == "PING")
                {
                    messageToSend = response;
                    hasMessageToSend = true;

                    lws_callback_on_writable(wsi);
                }

                else
                {
                    std::string logMessage = "실시간 시세 서버 응답 수신 : " + responseJson.dump();

                    g_log.LogConsole(Log::Level::INFO, logMessage.c_str());
                    g_log.LogMessage(Log::Level::INFO, logMessage.c_str());
                }

                // LOGIN 응답 처리
                if (trnm == "LOGIN")
                {
                    int returnCode = responseJson.value("return_code", -1);

                    if (returnCode != 0)
                    {
                        std::string returnMsg = responseJson.value("return_msg", "Unknown error");

                        g_log.LogConsole(Log::Level::ERROR, ("로그인 실패하였습니다: " + returnMsg).c_str());
                        g_log.LogMessage(Log::Level::ERROR, ("로그인 실패하였습니다: " + returnMsg).c_str());

                        isKeepRunning = false;
                    }

                    else
                    {
                        g_log.LogConsole(Log::Level::INFO, "로그인 성공하였습니다.");
                        g_log.LogMessage(Log::Level::INFO, "로그인 성공하였습니다.");

                        g_log.LogConsole(Log::Level::INFO, "조건검색 목록조회 패킷을 전송합니다.");
                        g_log.LogMessage(Log::Level::INFO, "조건검색 목록조회 패킷을 전송합니다.");

                        isLoggedIn = true;

                        // 조건검색 목록조회 패킷 전송
                        json cnsrlstPacket = { { "trnm", "CNSRLST" } };

                        messageToSend = cnsrlstPacket.dump();
                        hasMessageToSend = true;

                        lws_callback_on_writable(wsi);
                    }
                }

                // REAL 데이터 처리
                if (trnm == "REAL" && responseJson.contains("data"))
                {
                    json items = responseJson["data"];

                    for (const auto& item : items)
                    {
                        if (item.contains("values") && item["values"].contains("9001"))
                        {
                            std::string jmcode = item["values"]["9001"].get<std::string>();

                            g_log.LogConsole(Log::Level::INFO, ("실시간 시세 데이터 수신 - 종목코드: " + jmcode).c_str());
                            g_log.LogMessage(Log::Level::INFO, ("실시간 시세 데이터 수신 - 종목코드: " + jmcode).c_str());
                        }
                    }
                }
            }

            catch (json::parse_error& e)
            {
                g_log.LogConsole(Log::Level::ERROR, ("JSON 파싱 오류: " + std::string(e.what())).c_str());
                g_log.LogMessage(Log::Level::ERROR, ("JSON 파싱 오류: " + std::string(e.what())).c_str());
            }

            break;
        }

    case LWS_CALLBACK_CLIENT_WRITEABLE:
    {
        if (hasMessageToSend)
        {
            size_t messageLength = messageToSend.length();
            unsigned char buffer[LWS_PRE + messageLength];
            memcpy(&buffer[LWS_PRE], messageToSend.c_str(), messageLength);

            int written = lws_write(wsi, &buffer[LWS_PRE], messageLength, LWS_WRITE_TEXT);

            if (written < 0)
            {
				g_log.LogConsole(Log::Level::ERROR, "메시지 전송 실패");
				g_log.LogMessage(Log::Level::ERROR, "메시지 전송 실패");

                return -1;
            }

			g_log.LogMessage(Log::Level::ERROR, ("Message sent : " + messageToSend).c_str());
			g_log.LogConsole(Log::Level::ERROR, ("Message sent : " + messageToSend).c_str());

            hasMessageToSend = false;
        }

        break;
    }

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
    {
        std::string logMessage = "연결 오류 : " + (in == nullptr) ? (char*)in : "Unknown error";
        g_log.LogConsole(Log::Level::ERROR, logMessage.c_str());
        g_log.LogMessage(Log::Level::ERROR, logMessage.c_str());

        isConnected = false;
        isKeepRunning = false;

        break;
    }
        

    case LWS_CALLBACK_CLOSED:
        g_log.LogConsole(Log::Level::ERROR, "Connection closed by the server");
        g_log.LogMessage(Log::Level::ERROR, "Connection closed by the server");

        isConnected = false;
        isKeepRunning = false;

        break;

    default:
        break;
    }

    return 0;
}

// WebSocket 프로토콜 정의
static struct lws_protocols protocols[] =
{
    {
        "websocket-protocol",
        CallBackWebSocket,
        0,
        4096,
    },

    { NULL, NULL, 0, 0 } // 종료 표시
};

// URL 파싱 함수
URLComponents ParseURL(const std::string& url)
{
    URLComponents components;
    size_t pos = 0;

    // 프로토콜 추출
    size_t protocolEnd = url.find("://");
    if (protocolEnd != std::string::npos)
    {
        components.protocol = url.substr(0, protocolEnd);
        pos = protocolEnd + 3;
    }

    // 호스트와 포트 추출
    size_t pathStart = url.find('/', pos);
    std::string hostPort = url.substr(pos, pathStart - pos);

    size_t portStart = hostPort.find(':');

    if (portStart != std::string::npos)
    {
        components.host = hostPort.substr(0, portStart);
        components.port = std::stoi(hostPort.substr(portStart + 1));
    }

    else
    {
        components.host = hostPort;
        components.port = (components.protocol == "wss") ? 443 : 80;
    }

    // 경로 추출
    if (pathStart != std::string::npos)
        components.path = url.substr(pathStart);

    else
        components.path = "/";

    return components;
}

// 메시지 전송 함수
void sendMessage(struct lws* wsi, const json& message)
{
    messageToSend = message.dump();
    hasMessageToSend = true;

    lws_callback_on_writable(wsi);
}

int main() 
{
    Application& app = Application::GetInstance();
    app.Initialize();

    // g_socketURL을 런타임에 설정하여 정적 초기화 순서 문제 방지
    socketURL = std::string(Config::socketURL) + "/api/dostk/websocket";

    // 액세스 토큰 발급
	g_log.LogConsole(Log::Level::INFO, "액세스 토큰을 발급받는 중...");
	g_log.LogMessage(Log::Level::INFO, "액세스 토큰을 발급받는 중...");

    accessToken = Login::GetAccessToken();

    if (accessToken.empty())
    {
		g_log.LogConsole(Log::Level::ERROR, "토큰 발급 실패");
		g_log.LogMessage(Log::Level::ERROR, "토큰 발급 실패");

        return -1;
    }

	g_log.LogConsole(Log::Level::INFO, "토큰 발급 완료");
	g_log.LogMessage(Log::Level::INFO, "토큰 발급 완료");

    // URL 파싱
    URLComponents urlComp = ParseURL(socketURL);

    std::cout << "WebSocket 서버 연결 중..." << std::endl;
    std::cout << "URL: " << socketURL << std::endl;
    std::cout << "Host: " << urlComp.host << std::endl;
    std::cout << "Port: " << urlComp.port << std::endl;
    std::cout << "Path: " << urlComp.path << std::endl;

    // libwebsockets 컨텍스트 생성 정보
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));

    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    // 컨텍스트 생성
    struct lws_context* context = lws_create_context(&info);

    if (context == nullptr)
    {
		g_log.LogConsole(Log::Level::ERROR, "libwebsockets 컨텍스트 생성 실패");
		g_log.LogMessage(Log::Level::ERROR, "libwebsockets 컨텍스트 생성 실패");

        return -1;
    }

    // 클라이언트 연결 정보
    struct lws_client_connect_info clinetConnectInfo;
    memset(&clinetConnectInfo, 0, sizeof(clinetConnectInfo));

    clinetConnectInfo.context = context;
    clinetConnectInfo.address = urlComp.host.c_str();
    clinetConnectInfo.port = urlComp.port;
    clinetConnectInfo.path = urlComp.path.c_str();
    clinetConnectInfo.host = clinetConnectInfo.address;
    clinetConnectInfo.origin = clinetConnectInfo.address;
    clinetConnectInfo.protocol = protocols[0].name;

    // SSL 설정 (wss://)
    if (urlComp.protocol == "wss")
        clinetConnectInfo.ssl_connection = LCCSCF_USE_SSL;

    // 연결 시작
    struct lws* wsi = lws_client_connect_via_info(&clinetConnectInfo);

    if (wsi == nullptr)
    {
		g_log.LogConsole(Log::Level::ERROR, "WebSocket 연결 실패");
		g_log.LogMessage(Log::Level::ERROR, "WebSocket 연결 실패");

        lws_context_destroy(context);

        return -1;
    }

    // 실시간 항목 등록을 위한 타이머 스레드
    bool messageSent = false;

    std::thread timerThread([&wsi, &messageSent]()
    {
        std::this_thread::sleep_for(std::chrono::seconds(2));

        if (isLoggedIn && !messageSent)
        {
			g_log.LogConsole(Log::Level::INFO, "실시간 항목 등록 패킷을 전송합니다.");
			g_log.LogMessage(Log::Level::INFO, "실시간 항목 등록 패킷을 전송합니다.");

            json cnsrreqPacket =
            {
                { "trnm", "CNSRREQ" },
                { "seq", "0" },
                { "search_type", "1" },
                { "stex_tp", "K" }
            };

            sendMessage(wsi, cnsrreqPacket);
            messageSent = true;
        }
    });

    // 이벤트 루프
    while (isKeepRunning && lws_service(context, 50) >= 0)
    {
        // 계속 실행
    }

    // 정리
    if (timerThread.joinable())
        timerThread.join();

    lws_context_destroy(context);

	g_log.LogConsole(Log::Level::INFO, "프로그램 종료");
	g_log.LogMessage(Log::Level::INFO, "프로그램 종료");

    return 0;
}
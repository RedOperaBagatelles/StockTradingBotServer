#include "Account.h"
#include "Login.h"

#include "Core/Config.h"
#include "Core/Log.h"
#include "Utility/Convert.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <string>
#include <vector>
#include <set>
#include <sstream>
#include <algorithm>
#include <cctype>

using json = nlohmann::json;

std::set<std::string> Account::accounts;
std::string Account::currentAccountNumber;

std::set<std::string>& Account::GetAllAccountNumbers()
{
    Log& log = Log::GetInstance();
    
	// 이미 계좌번호를 가져온 경우 재사용
    if (!accounts.empty())
		return accounts;

	// 키움 증권의 Access Token 가져오기
    std::string token = Login::GetAccessToken();

    if (token.empty())
    {
		log.LogConsole(Log::Level::ERROR, "접근 토큰이 비어있습니다. 계좌 조회를 중단합니다.");
		log.LogMessage(Log::Level::ERROR, "접근 토큰이 비어있습니다. 계좌 조회를 중단합니다.");

        return accounts;
    }

	const std::string endpoint = "/api/dostk/acnt";                     // 계좌번호 조회 API 엔드포인트
	const std::string url = std::string(Config::hostURL) + endpoint;    // 전체 URL

	std::string hasGetNextData = "N";   // 다음에 가져올 데이터가 있는지 저장하는 변수
	std::string nextKey = "";           // 다음 데이터를 가져오기 위한 키

	// 최대 50페이지까지 반복
    const int maxPages = 50;

    for (int page = 0; page < maxPages; page++)
    {
        CURL* curl = curl_easy_init();

        if (curl == nullptr)
        {
            log.LogConsole(Log::Level::ERROR, "CURL 초기화 실패 (Account::GetAllAccountNumbers)");
            log.LogMessage(Log::Level::ERROR, "CURL 초기화 실패 (Account::GetAllAccountNumbers)");

            break;
        }

		// Request에 필요한 헤더 설정
        std::string readBuffer;
        std::string headerBuffer;

        struct curl_slist* headers = NULL;

        std::string hdrContentType = "Content-Type: application/json;charset=UTF-8";
        std::string hdrAuth = "authorization: Bearer " + token;
        std::string hdrCont = "cont-yn: " + hasGetNextData;
        std::string hdrNext = "next-key: " + nextKey;
        std::string hdrApiId = "api-id: ka00001";

        headers = curl_slist_append(headers, hdrContentType.c_str());
        headers = curl_slist_append(headers, hdrAuth.c_str());
        headers = curl_slist_append(headers, hdrCont.c_str());
        headers = curl_slist_append(headers, hdrNext.c_str());
        headers = curl_slist_append(headers, hdrApiId.c_str());

		// curl 옵션 설정
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{}");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Login::WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headerBuffer);

		// HTTP POST 요청 수행
        CURLcode response = curl_easy_perform(curl);

        if (response != CURLE_OK)
        {
            std::string errorMessage = std::string("HTTP 요청 실패 : ") + curl_easy_strerror(response);
            log.LogConsole(Log::Level::ERROR, errorMessage.c_str());
            log.LogMessage(Log::Level::ERROR, errorMessage.c_str());

            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);

            break;
        }

		// 응답 상태 코드 가져오기
        long responseCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

		// 비정상 응답이 들어올 경우 처리
        if (responseCode != 200)
        {
            std::string errorMessage = "계좌 번호 호출 비정상 응답 코드 수신 : " + std::to_string(responseCode);

            log.LogConsole(Log::Level::ERROR, errorMessage.c_str());
            log.LogMessage(Log::Level::ERROR, errorMessage.c_str());

            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);

            break;
		}

		// 가져온 헤더를 가지고 contYn, nextKey 파싱
        std::istringstream hstream(headerBuffer);
        std::string line;
		std::string hasThisNextData = "N";      // 이번 응답에서 다음 데이터가 있는지 여부
        std::string newNext = "";

		// 데이터를 한 줄씩 읽으면서 파싱
        while (std::getline(hstream, line))
        {
			JsonData data = Convert::GetJsonData(line);

            if (data.key.empty())
				continue;

			// 소문자로 변환하여 키 비교
            std::string lowerKey = data.key;
            std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);

            if (lowerKey == "cont-yn")
                hasThisNextData = data.value;

            else if (lowerKey == "next-key")
                newNext = data.value;
        }

		// JSON 파싱 및 계좌번호 추출
        try
        {
            if (!readBuffer.empty())
            {
                json data = json::parse(readBuffer);

				// 만약 응답에 acctNo 키가 있고, 그 값이 문자열일 경우
                if (data.contains("acctNo") && data["acctNo"].is_string())
                {
                    std::string account = data["acctNo"].get<std::string>();

                    if (!account.empty())
                        accounts.insert(account);
                }

				// 만약 응답에 acctNo 키가 있고, 그 값이 배열일 경우
                else if (data.contains("acctNo") && data["acctNo"].is_array())
                {
                    for (auto& accountElement : data["acctNo"])
                    {
                        if (accountElement.is_string())
                            accounts.insert(accountElement.get<std::string>());
                    }
                }

				// 그 외에 데이터 구조에 대해서 보수적으로 스캔
                else
                {
					// 모든 키-값 쌍을 순회
                    for (auto iter = data.begin(); iter != data.end(); ++iter)
                    {
					std::string key = iter.key();   // 키 가져오기
					std::string lk = key;           // 소문자로 변환

                        std::transform(lk.begin(), lk.end(), lk.begin(), ::tolower);

					// 계좌 관련 키인지 확인
                        if (lk.find("acct") != std::string::npos || lk.find("account") != std::string::npos)
                        {
                            if (iter.value().is_string())
                                accounts.insert(iter.value().get<std::string>());

                            else if (iter.value().is_array())
                            {
                                for (auto& accountElement : iter.value())
                                {
                                    if (accountElement.is_string())
                                        accounts.insert(accountElement.get<std::string>());
                                }
                            }
                        }
                    }
                }
            }
        }

        catch (json::parse_error& e)
        {
            std::string err = std::string("JSON 파싱 오류: ") + e.what();

            log.LogConsole(Log::Level::ERROR, err.c_str());
            log.LogMessage(Log::Level::ERROR, err.c_str());
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

		// hasGetNextData, nextKey 업데이트
        hasGetNextData = hasThisNextData;
        nextKey = newNext;

		// 더 이상 데이터가 없으면 종료
        if (hasGetNextData != "Y")
            break;
    }

    return accounts;
}

bool Account::HasAccount(const std::string& accountNumber)
{
    if (accounts.empty())
        GetAllAccountNumbers();

    return accounts.find(accountNumber) != accounts.end();
}

void Account::SetUseAccount(const bool isFake)
{
    Log& log = Log::GetInstance();
	const char* accountNumber = isFake ? Config::fakeAccountNum : Config::realAccountNum;

	// 설정한 계좌번호가 있으면 해당 계좌번호로 설정
    if (HasAccount(accountNumber))
    {
		currentAccountNumber = accountNumber;

		log.LogConsole(Log::Level::INFO, ("지정된 계좌번호 " + std::string(accountNumber) + "(으)로 설정합니다.").c_str());
		log.LogMessage(Log::Level::INFO, ("지정된 계좌번호 " + std::string(accountNumber) + "(으)로 설정합니다.").c_str());

        return;
    }

	log.LogConsole(Log::Level::WARNING, "지정된 계좌번호를 찾을 수 없습니다. 기본 계좌번호로 설정합니다.");
	log.LogMessage(Log::Level::WARNING, "지정된 계좌번호를 찾을 수 없습니다. 기본 계좌번호로 설정합니다.");

    // 계좌번호가 없으면 첫 번째 계좌번호를 사용하도록 설정
	std::set<std::string> allAccounts = GetAllAccountNumbers();

    if (!allAccounts.empty())
		currentAccountNumber = *allAccounts.begin();

	log.LogConsole(Log::Level::INFO, ("기본 계좌번호 " + currentAccountNumber + "(으)로 설정합니다.").c_str());
	log.LogMessage(Log::Level::INFO, ("기본 계좌번호 " + currentAccountNumber + "(으)로 설정합니다.").c_str());
}

std::string& Account::GetCurrentAccountNumber()
{
	return currentAccountNumber;
}

size_t Account::HeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata)
{
    size_t total = size * nitems;

    if (userdata != nullptr)
    {
        std::string* headerBuf = static_cast<std::string*>(userdata);
        headerBuf->append(buffer, total);
    }

    return total;
}
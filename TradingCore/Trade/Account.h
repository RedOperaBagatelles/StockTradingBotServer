#pragma once

#include <string>
#include <vector>
#include <set>

class Account
{
public:
    static std::set<std::string>& GetAllAccountNumbers();            // 모든 계좌번호를 반환하는 정적 메소드

    static bool HasAccount(const std::string& accountNumber);       // 특정 계좌번호가 존재하는지 확인하는 정적 메소드
    static void SetUseAccount(const bool isFake = false);           // 실제로 사용할 수 있는 계좌번호 설정

    static std::string& GetCurrentAccountNumber();                  // 현재 사용 중인 계좌번호 반환

private:
	static size_t HeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata); // 헤더 콜백 함수

	static std::set<std::string> accounts;      // 모든 계좌번호를 저장하는 변수
	static std::string currentAccountNumber;    // 현재 사용 중인 계좌번호를 저장하는 변수
};
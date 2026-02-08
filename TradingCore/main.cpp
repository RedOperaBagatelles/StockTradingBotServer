#include "Login.h"

#include "Core/Application.h"
#include "Trade/Account.h"
#include "Core/Config.h"

#include <unistd.h>

int main()
{
    Application& app = Application::GetInstance();
    app.Initialize();

    Account::SetUseAccount();           // 계좌 설정
    Account::RefreshCurrentHoldings();  // 잔고 조회
    Account::ShowHoldings();            // 출력

    // 프로그램 대기
    pause();

    return 0;
}
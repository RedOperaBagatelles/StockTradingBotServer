#include "String.h"

std::string String::GetSignDigit(const std::string& input)
{
    std::string out;
    out.reserve(input.size());

	// 부호가 나왔는지 확인하는 변수
    bool seenSign = false;

    for (unsigned char current : input)
    {
        // 부호가 처음으로 나왔을 경우
        if (!seenSign && (current == '-' || current == '+'))
        {
            if (current == '-')
                out.push_back(current);

            seenSign = true;
        }

		// 숫자인 경우
        else if (std::isdigit(current))
            out.push_back(current);
    }

    return out;
}
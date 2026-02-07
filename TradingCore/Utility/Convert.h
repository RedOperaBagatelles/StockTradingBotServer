#pragma once

#include <nlohmann/json.hpp>

#include <string_view>

using json = nlohmann::json;

struct JsonData
{
	std::string key;
	std::string value;
};

class Convert
{
public:
	static double JsonToDouble(const json& j, const std::string& key);
	static long JsonToLong(const json& j, const std::string& key);

	static JsonData GetJsonData(const std::string_view& data);				// 키:값 쌍을 JsonData 구조체로 반환
};


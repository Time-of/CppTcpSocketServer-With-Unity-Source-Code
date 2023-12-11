#pragma once

#include "Legacy_CVSP.h"
#include <any>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct CalcResult;

/**
* @author 이성수
* @brief 게임플레이에 필요한 계산을 수행할 클래스
* @details 솔루션 탐색 창에서 우클릭하고 클래스 생성 눌렀더니 폴더 하나 더 들어가서 만들어졌네...
* @since 2023-11-30
*/
class Legacy_GameplayCalcs
{
private:
	explicit Legacy_GameplayCalcs() noexcept;
	explicit Legacy_GameplayCalcs(const Legacy_GameplayCalcs&) = delete;
	Legacy_GameplayCalcs& operator=(const Legacy_GameplayCalcs&) = delete;


public:
	~Legacy_GameplayCalcs() = default;

	static Legacy_GameplayCalcs& GetInstance();

	CalcResult InvokeFunction(const std::string& functionName, const std::vector<std::shared_ptr<std::any>>& args);


private:
	CalcResult RPCTakeDamageServer(const std::vector<std::shared_ptr<std::any>>& args);


	std::map<std::string, std::function<CalcResult(const std::vector<std::shared_ptr<std::any>>&)>> functionMap;
};



struct CalcResult
{
	// 성공 여부?
	bool bSuccessed;

	// 결과 타입 정보
	std::vector<::byte> typeInfos;
	// 결과 벡터
	std::vector<std::shared_ptr<std::any>> result;
	// 전파할 함수 이름
	char broadcastFunctionName[20];
};

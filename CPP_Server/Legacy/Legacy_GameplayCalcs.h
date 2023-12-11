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
* @author �̼���
* @brief �����÷��̿� �ʿ��� ����� ������ Ŭ����
* @details �ַ�� Ž�� â���� ��Ŭ���ϰ� Ŭ���� ���� �������� ���� �ϳ� �� ���� ���������...
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
	// ���� ����?
	bool bSuccessed;

	// ��� Ÿ�� ����
	std::vector<::byte> typeInfos;
	// ��� ����
	std::vector<std::shared_ptr<std::any>> result;
	// ������ �Լ� �̸�
	char broadcastFunctionName[20];
};

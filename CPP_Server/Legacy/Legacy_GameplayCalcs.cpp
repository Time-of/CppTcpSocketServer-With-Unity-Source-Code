#include "Legacy_GameplayCalcs.h"
// include�� �� "CPP_Server/Legacy_GameplayCalcs.h" �� �ؾ� ��

#include <iostream>
#include <cassert>


Legacy_GameplayCalcs::Legacy_GameplayCalcs() noexcept : functionMap(
	{
		{"RPCTakeDamageServer", std::bind(&Legacy_GameplayCalcs::RPCTakeDamageServer, this, std::placeholders::_1)}
	})
{
	
}


Legacy_GameplayCalcs& Legacy_GameplayCalcs::GetInstance()
{
	static Legacy_GameplayCalcs instance;

	return instance;
}


CalcResult Legacy_GameplayCalcs::InvokeFunction(const std::string& functionName, const std::vector<std::shared_ptr<std::any>>& args)
{
	auto found = functionMap.find(functionName);
	if (found == functionMap.end())
	{
		CalcResult failed;
		failed.bSuccessed = false;
		
		std::cout << "InvokeFunction ����! �Լ���: " << functionName << "\n";
		return failed;
	}

	CalcResult result = found->second(args);

	return result;
}


CalcResult Legacy_GameplayCalcs::RPCTakeDamageServer(const std::vector<std::shared_ptr<std::any>>& args)
{
	// ������ȭ�� �Ǿ��ٰ� �����ϰ� ����

	//float damage = *(float*)args[0];
	//int defense = *(int*)args[1];

	float damage = *std::any_cast<float>(args[0].get());
	int defense = *std::any_cast<int>(args[1].get());

	float result = damage - defense;
	CalcResult returnValue;
	returnValue.result.push_back(std::make_shared<std::any>(result));
	strcpy_s(returnValue.broadcastFunctionName, "RPCTakeDamageAll");
	returnValue.typeInfos.push_back(Legacy_RPCValueType::Float);

	return returnValue;
}

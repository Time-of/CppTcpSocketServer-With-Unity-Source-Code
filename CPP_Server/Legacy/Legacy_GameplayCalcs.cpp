#include "Legacy_GameplayCalcs.h"
// include할 때 "CPP_Server/Legacy_GameplayCalcs.h" 로 해야 함

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
		
		std::cout << "InvokeFunction 실패! 함수명: " << functionName << "\n";
		return failed;
	}

	CalcResult result = found->second(args);

	return result;
}


CalcResult Legacy_GameplayCalcs::RPCTakeDamageServer(const std::vector<std::shared_ptr<std::any>>& args)
{
	// 역직렬화가 되었다고 가정하고 수행

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

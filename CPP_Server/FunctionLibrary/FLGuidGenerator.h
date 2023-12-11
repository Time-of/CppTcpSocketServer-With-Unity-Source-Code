#pragma once

#include "Types/CannotCreatable.h"
#include "Types/MyTypes.h"


/**
* GUID 생성을 담당하는 함수 라이브러리.
*/
class FLGuidGenerator : public CannotCreatable
{
public:
	static uint32 GenerateGUID();


private:
	inline static uint8 _CharToNum(char ch) { return ch - '0'; }
};
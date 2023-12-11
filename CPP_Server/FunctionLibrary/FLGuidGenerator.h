#pragma once

#include "Types/CannotCreatable.h"
#include "Types/MyTypes.h"


/**
* GUID ������ ����ϴ� �Լ� ���̺귯��.
*/
class FLGuidGenerator : public CannotCreatable
{
public:
	static uint32 GenerateGUID();


private:
	inline static uint8 _CharToNum(char ch) { return ch - '0'; }
};
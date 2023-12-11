#pragma once


/**
* 무작위 값 생성을 담당하는 함수 라이브러리.
*/
class FLRandom : public CannotCreatable
{
public:
	// minInclusive 이상, maxInclusive 이하 값 생성.
	static int32 GetFromRange(int32 minInclusive, int32 maxInclusive);
};
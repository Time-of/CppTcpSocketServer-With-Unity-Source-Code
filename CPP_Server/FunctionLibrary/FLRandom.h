#pragma once


/**
* ������ �� ������ ����ϴ� �Լ� ���̺귯��.
*/
class FLRandom : public CannotCreatable
{
public:
	// minInclusive �̻�, maxInclusive ���� �� ����.
	static int32 GetFromRange(int32 minInclusive, int32 maxInclusive);
};
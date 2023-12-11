#pragma once

class Serializer;


/**
* 직렬화 가능하도록, 직렬화 방법을 정의할 수 있는 인터페이스 클래스.
* 단, 받는 쪽에서 알아서 알맞은 타입에 받아야만 하기에, Unsafe하다는 문제가 있습니다.
*/
class ISerializable
{
public:
	virtual ~ISerializable() {}


public:
	virtual void Serialize(Serializer& ser)
	{
		bHasOverrided = false;
	}


	virtual void Deserialize(Serializer& ser)
	{
		bHasOverrided = false;
	}


public:
	bool bHasOverrided = true;
};
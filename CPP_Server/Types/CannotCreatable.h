#pragma once


/**
* ����Ͽ�, ������ �Ұ����ϰ� ������ִ� Ŭ����.
*/
class CannotCreatable
{
private:
	explicit CannotCreatable() noexcept = delete;
	explicit CannotCreatable(const CannotCreatable& other) = delete;
	explicit CannotCreatable(const CannotCreatable&& other) noexcept = delete;
	CannotCreatable& operator=(const CannotCreatable& other) = delete;
	CannotCreatable& operator=(const CannotCreatable&& other) noexcept = delete;
	~CannotCreatable() = delete;
};
#pragma once


/**
* 상속하여, 생성이 불가능하게 만들어주는 클래스.
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
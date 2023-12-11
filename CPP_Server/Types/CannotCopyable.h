#pragma once



/**
* 상속하여, 복사 불가능하도록 만들어주는 클래스.
*/
class CannotCopyable
{
public:
	explicit CannotCopyable() noexcept = default;
	virtual ~CannotCopyable() {}


private:
	explicit CannotCopyable(const CannotCopyable& other) = delete;
	CannotCopyable& operator=(const CannotCopyable& other) = delete;
};
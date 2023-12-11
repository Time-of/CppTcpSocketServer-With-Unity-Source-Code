#pragma once



/**
* ����Ͽ�, ���� �Ұ����ϵ��� ������ִ� Ŭ����.
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
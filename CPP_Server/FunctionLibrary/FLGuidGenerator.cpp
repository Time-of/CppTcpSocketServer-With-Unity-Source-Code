#include "pch.h"

#include "FLGuidGenerator.h"

#include "FunctionLibrary/FLRandom.h"


uint32 FLGuidGenerator::GenerateGUID()
{
	auto now = std::chrono::system_clock::now();
	std::time_t now_t = std::chrono::system_clock::to_time_t(now);
	std::tm now_tm;
	localtime_s(&now_tm, &now_t);
	int32 random = FLRandom::GetFromRange(0, 9999);

	// ���� �ð��� string���� �ޱ�
	std::stringstream ss;
	ss << (std::format("{0:02d}{1:02d}{2:02d}{3:02d}{4:02d}{5:02d}{6:04d}",
		now_tm.tm_year - 100,
		now_tm.tm_mon + 1,
		now_tm.tm_mday,
		now_tm.tm_hour,
		now_tm.tm_min,
		now_tm.tm_sec,
		random));


	// �� 6�ڸ� ���ϱ� (��¥)
	Byte byteOne = _CharToNum(ss.get()) + _CharToNum(ss.get()) 
		+ _CharToNum(ss.get()) + _CharToNum(ss.get()) 
		+ _CharToNum(ss.get()) + _CharToNum(ss.get());

	// �ð�
	Byte byteTwo = _CharToNum(ss.get()) * 10 + _CharToNum(ss.get());

	// �� + �� + ������ ���� 1�ڸ�
	Byte byteThree = _CharToNum(ss.get()) * 10 + _CharToNum(ss.get())
		+ _CharToNum(ss.get()) * 10 + _CharToNum(ss.get()) 
		+ _CharToNum(ss.get()) * 7;

	// ������ ���� 3�ڸ� (�ִ밪: 252)
	Byte byteFour =  _CharToNum(ss.get()) * 14
		+ _CharToNum(ss.get()) * 13 + _CharToNum(ss.get()) * 3;

	uint32 guid = byteOne << 24 | byteTwo << 16 | byteThree << 8 | byteFour;

	return guid;
}

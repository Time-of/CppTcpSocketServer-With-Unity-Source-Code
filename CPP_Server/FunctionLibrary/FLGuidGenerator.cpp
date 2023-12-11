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

	// 현재 시간을 string으로 받기
	std::stringstream ss;
	ss << (std::format("{0:02d}{1:02d}{2:02d}{3:02d}{4:02d}{5:02d}{6:04d}",
		now_tm.tm_year - 100,
		now_tm.tm_mon + 1,
		now_tm.tm_mday,
		now_tm.tm_hour,
		now_tm.tm_min,
		now_tm.tm_sec,
		random));


	// 앞 6자리 더하기 (날짜)
	Byte byteOne = _CharToNum(ss.get()) + _CharToNum(ss.get()) 
		+ _CharToNum(ss.get()) + _CharToNum(ss.get()) 
		+ _CharToNum(ss.get()) + _CharToNum(ss.get());

	// 시간
	Byte byteTwo = _CharToNum(ss.get()) * 10 + _CharToNum(ss.get());

	// 분 + 초 + 무작위 숫자 1자리
	Byte byteThree = _CharToNum(ss.get()) * 10 + _CharToNum(ss.get())
		+ _CharToNum(ss.get()) * 10 + _CharToNum(ss.get()) 
		+ _CharToNum(ss.get()) * 7;

	// 무작위 숫자 3자리 (최대값: 252)
	Byte byteFour =  _CharToNum(ss.get()) * 14
		+ _CharToNum(ss.get()) * 13 + _CharToNum(ss.get()) * 3;

	uint32 guid = byteOne << 24 | byteTwo << 16 | byteThree << 8 | byteFour;

	return guid;
}

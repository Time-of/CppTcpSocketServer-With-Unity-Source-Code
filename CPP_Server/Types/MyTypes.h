#pragma once

#include <cstdint>


using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;
using int8 = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;

// byte로 쓰는 경우는 직렬화/역직렬화로 사용되는 경우만 byte로 표기
// 그 외에는 uint8로 표기
using Byte = uint8;

#define uintCast8(n) static_cast<uint8>(n)
#define uintCast16(n) static_cast<uint16>(n)


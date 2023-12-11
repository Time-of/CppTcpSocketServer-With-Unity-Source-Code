#pragma once

#include <mutex>

#include "spdlog/spdlog.h"


/**
* @author 이성수(Time-of)
* @brief 로그를 편하게 찍기 위한 매크로들 모음
*/

/*
#define LOCKGUARD(myMutex, lock_guard_name) spdlog::trace("{} Line {}: mutex {} 요청", __FUNCTION__, __LINE__, #lock_guard_name); \
	std::lock_guard<std::mutex> lock_guard_name(myMutex); \
	spdlog::trace("{} Line {}: {} {} 획득!", __FUNCTION__, __LINE__, #myMutex, #lock_guard_name)
*/

#define LOCKGUARD(myMutex, lock_guard_name) std::lock_guard<std::mutex> lock_guard_name(myMutex);

#define LOG_INFO(messageText) spdlog::info("{}({}): {}", __FUNCTION__, __LINE__, messageText)
#define LOG_WARN(messageText) spdlog::warn("{}({}): {}", __FUNCTION__, __LINE__, messageText)
#define LOG_ERROR(messageText) spdlog::error("{}({}): {}", __FUNCTION__, __LINE__, messageText)
#define LOG_CRITICAL(messageText) spdlog::critical("{}({}), {}", __FUNCTION__, __LINE__, messageText)
#define LOG_DEBUG(messageText) spdlog::debug("{}({}): {}", __FUNCTION__, __LINE__, messageText)
#define LOG_TRACE(messageText) spdlog::trace("{}({}): {}", __FUNCTION__, __LINE__, messageText)

#define FMT fmt::format
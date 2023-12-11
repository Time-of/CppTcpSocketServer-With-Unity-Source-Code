#include "pch.h"

#pragma comment(lib, "ws2_32")

#include <format>
#include <iostream>

#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "CVSP/CVSPv2.h"
#include "Server/TcpServer.h"
#include "DBManager.h"

void SetLoggerMultisink()
{
	auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

	auto now = std::chrono::system_clock::now();
	std::time_t now_t = std::chrono::system_clock::to_time_t(now);
	std::tm now_tm;
	localtime_s(&now_tm, &now_t);
	std::string logName(fmt::format("server_logs/log_{0:02d}{1:02d}{2:02d}_{3:02d}_{4:02d}_{5:02d}.txt",
		now_tm.tm_year - 100,
		now_tm.tm_mon + 1,
		now_tm.tm_mday,
		now_tm.tm_hour,
		now_tm.tm_min,
		now_tm.tm_sec));

	auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logName, true);
	
	auto myLogger = std::make_shared<spdlog::logger>("", spdlog::sinks_init_list({ consoleSink, fileSink }));
	myLogger->set_pattern("[%C%m%d-%H:%M:%S.%e] [%^%l-%t%$] %v");
	spdlog::set_default_logger(myLogger);
}


int main(int argc, char* argv)
{
	system("mode con:cols=96 lines=48");

	SetLoggerMultisink();
	spdlog::set_level(spdlog::level::trace);
	
	DBManager::GetInstance();

	TcpServer::GetInstance().Listen();

	return 0;
}
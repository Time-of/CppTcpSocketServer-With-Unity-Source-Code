#include "pch.h"

#include "DBManager.h"



bool DBManager::CheckLogin(const std::string& nickname, const std::string& password)
{
	char* errorMessage = nullptr;
	std::string sql = "select password from AccountInfo where name = \"" + nickname + "\"";

	std::string callbackData;
	int result = sqlite3_exec(db, sql.c_str(), &DBManager::DBCallback, (void*)&callbackData, &errorMessage);
	
	if (result == SQLITE_OK)
	{
		LOG_INFO("DB 정보 조회 성공");
		LOG_DEBUG(FMT("입력한 ID [{}]와 비밀번호: [{}]", nickname, password));
		LOG_DEBUG(FMT("조회된 ID [{}]에 대한 비밀번호: [{}]", nickname, callbackData));
		LOG_DEBUG(FMT("성공 여부: {}", callbackData == password));
	}
	else
	{
		LOG_ERROR(FMT("SQL Error: {}", errorMessage));
		sqlite3_free(errorMessage);
	}

	return callbackData == password;
}


DBManager::DBManager() noexcept : db(nullptr)
{
	LOG_INFO(FMT("SQLITE VERSION: {}", sqlite3_libversion()));

	int result = sqlite3_open("account.db", &db);
	if (result == SQLITE_OK)
	{
		LOG_INFO("DB 열기 성공!");
	}
	else
	{
		LOG_ERROR("DB 열기 실패!");
		assert(false);
	}
}


DBManager::~DBManager()
{
	sqlite3_close(db);
}

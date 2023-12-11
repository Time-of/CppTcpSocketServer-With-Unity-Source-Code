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
		LOG_INFO("DB ���� ��ȸ ����");
		LOG_DEBUG(FMT("�Է��� ID [{}]�� ��й�ȣ: [{}]", nickname, password));
		LOG_DEBUG(FMT("��ȸ�� ID [{}]�� ���� ��й�ȣ: [{}]", nickname, callbackData));
		LOG_DEBUG(FMT("���� ����: {}", callbackData == password));
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
		LOG_INFO("DB ���� ����!");
	}
	else
	{
		LOG_ERROR("DB ���� ����!");
		assert(false);
	}
}


DBManager::~DBManager()
{
	sqlite3_close(db);
}

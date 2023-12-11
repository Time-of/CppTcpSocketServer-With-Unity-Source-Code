#pragma once

#include "sqlite/sqlite3.h"


class DBManager
{
public:
	~DBManager();

	static DBManager& GetInstance()
	{
		static DBManager instance;
		return instance;
	}

	static int DBCallback(void* data, int argc, char** argv, char** atozColName)
	{
		std::string* password = static_cast<std::string*>(data);
		*password = argv[0] ? argv[0] : "NULL";
		LOG_INFO(FMT("{} = {}", atozColName[0], *password));

		return 0;
	}


public:
	bool CheckLogin(const std::string& nickname, const std::string& password);


private:
	explicit DBManager() noexcept;
	sqlite3* db;
};
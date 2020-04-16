#include "SQLiteCore.h"
#include "sqlite3-bind.h"
#include <iostream>
#include <charconv>

struct SQLiteCore::make_shared_enabler : SQLiteCore
{
    template <class... Args>
    make_shared_enabler(Args &&... args) : SQLiteCore(std::forward<Args>(args)...) {}
};

bool SQLiteCore::InsertUser(const std::string& display_name, const std::string& login, const std::string& password)
{
    const static std::string query1{ "INSERT INTO Users(Display_name,Login,Password)VALUES(?,?,?)" };
    
    int error = sqlite3_bind_exec(m_db.get(), query1.c_str(), nullptr, nullptr,
            SQLITE_BIND_TEXT(display_name.c_str()), SQLITE_BIND_TEXT(login.c_str()),
            SQLITE_BIND_TEXT(password.c_str()), SQLITE_BIND_END);
    if (error != SQLITE_OK)
    {
        std::cerr << "SQLite::InsertUser(...) ERROR: " << sqlite3_errstr(error) << std::endl;
        return false;
    }
    return true;
}


static int GetUserByLoginCallback(void* data, int argc, char** argv, char** azColName)
{
    auto& result = *static_cast<std::optional<User> *>(data);
    if (argc == 0)
    {
        result = std::nullopt;
        return -1;

    }
    if (argc > 5)
    {
        std::cerr << "GetUserByLoginCallback(...) TO MANY COLUMNS: more then 5 record columns in table Users!\n";
        result = std::nullopt;
        return -1;
    }
    for (auto i = 0; i < argc; ++i)
    {
        if (strcmp(azColName[i], "Display_name") == 0)
            result->display_name = argv[i];
        else if (strcmp(azColName[i], "Login") == 0)
        {
            assert(strcmp(argv[i], result->login.c_str()) == 0);
            if (strcmp(argv[i], result->login.c_str()) != 0)
            {
                std::cerr << "GetUserByLoginCallback(...) FOUND USER WITH ANOTHER LOGIN: given: " << result->login << ", found: " << azColName[i] << std::endl;
                return -1;
            }
        }
        else if (strcmp(azColName[i], "Password") == 0)
            result->password = argv[i];
        else if (strcmp(azColName[i], "Image") == 0)
            result->image = argv[i] ? argv[i] : "";
        else
        {
            assert(!"Unexpected column name");
            std::cerr << "GetUserByLoginCallback(...) UNEXPECTED COLUMN NAME: " << azColName[i] << std::endl;
        }
    }
    return 0;
}

std::optional<User> SQLiteCore::GetUserByLogin(const std::string& login)
{
     auto result = std::make_optional<User>();
     result->login = login;
     const static std::string query{ "SELECT Display_name, Login, Password FROM Users WHERE Login=?" };
     const int error = sqlite3_bind_exec(m_db.get(), query.c_str(), &GetUserByLoginCallback, &result, 
                                   SQLITE_BIND_TEXT(login.c_str()), SQLITE_BIND_END);
    if (result->display_name.empty())
    {
        std::cerr << "SQLite::GetUserByLogin(...) USER(" << *result <<  " NOT FOUND!\n";
        return std::nullopt;
    }
     if (error != SQLITE_OK)
     {
         std::cerr << "SQLite::InsertUser(...) ERROR: " << sqlite3_errstr(error) << std::endl;
         return std::nullopt;
     }
     return result;
}

bool SQLiteCore::CheckUserPassword(const std::string& login, const std::string& password)
{
    const auto user = GetUserByLogin(login);
    return user->password == password;
}

std::string SQLiteCore::GetImageByLogin(const std::string& login)
{
    auto result = std::make_optional<User>();
    const static std::string query{ "SELECT Image FROM Users WHERE Login=?" };
    const int error = sqlite3_bind_exec(m_db.get(), query.c_str(), &GetUserByLoginCallback, &result,
        SQLITE_BIND_TEXT(login.c_str()), SQLITE_BIND_END);
    if (error != SQLITE_OK)
    {
        std::cerr << "SQLite::InsertUser(...) ERROR: " << sqlite3_errstr(error) << std::endl;

        return {};
    }
    return std::move(result->image);
}

SQLiteCore::SQLiteCore(std::string name/*, boost::asio::io_context& ioc*/)
    : m_db_name(std::move(name))
   /*, m_strand(ioc)*/
{
    sqlite3* ptr = nullptr;
    const int err = sqlite3_open(m_db_name.c_str(), &ptr);
    if (err == SQLITE_OK)
        m_db = sqlite3_unique_ptr(ptr);
    else
        std::cerr << "SQLite::SQLite(...) ERROR: " << sqlite3_errstr(err);
    CreateTables();

}

std::shared_ptr<SQLiteCore> SQLiteCore::GetInstance(std::string db_name,/* boost::asio::io_context& ioc, */ const bool create)
{
    static std::shared_ptr<SQLiteCore> instance = nullptr;
    if (!instance && create)
    {
        instance = std::make_shared<make_shared_enabler>(std::move(db_name)/*, ioc*/);
    }
    return instance;
}

bool SQLiteCore::SetUserImageByLogin(const std::string& login, const std::string& image)
{
    const static std::string query{ "UPDATE Users SET Image=? WHERE Login=?" };
    const int error = sqlite3_bind_exec(m_db.get(), query.c_str(), nullptr, nullptr,
        SQLITE_BIND_TEXT(image.c_str()), SQLITE_BIND_TEXT(login.c_str()), SQLITE_BIND_END);
    if (error != SQLITE_OK)
    {
        std::cerr << "SQLite::SetUserImageByLogin(...) ERROR: " << sqlite3_errstr(error) << std::endl;
        return false;
    }
    return true;
}

bool SQLiteCore::SaveUserMessage(const std::string& login, const std::string& message)
{
    const static std::string query{ "INSERT INTO Messages(Login, Content) VALUES (?,?)" };
    const int error = sqlite3_bind_exec(m_db.get(), query.c_str(), nullptr, nullptr,
        SQLITE_BIND_TEXT(login.c_str()), SQLITE_BIND_TEXT(message.c_str()), SQLITE_BIND_END);
    if (error != SQLITE_OK)
    {
        std::cerr << "SQLite::SaveUserMessage(...) ERROR: " << sqlite3_errstr(error) << std::endl;
        return false;
    }
    return true;
}

static int GetUserMessagesCallback(void* data, int argc, char** argv, char** azColName)
{
    auto& result = *static_cast<std::vector<std::string>*>(data);
    if (argc == 0)
    {
        return -1;
    }
    if (argc > 1)
    {
        std::cerr << "GetUserMessagesCallback(...) TO MANY COLUMNS: more then 1 record columns in table Messages!\n";
        return -1;
    }
    for (auto i = 0; i < argc; ++i)
    {
        if (strcmp(azColName[i], "Content") == 0)
            result.emplace_back(argv[i]);
        else
        {
            assert(!"Unexpected column name");
            std::cerr << "GetUserMessagesCallback(...) UNEXPECTED COLUMN NAME: " << azColName[i] << std::endl;
        }
    }
    return 0;
}


std::vector<std::string> SQLiteCore::GetUndeliveredMessages(const std::string& login)
{
    std::vector<std::string> result;
    const static std::string query{ "SELECT Content FROM Messages WHERE Login=?" };
    const int error = sqlite3_bind_exec(m_db.get(), query.c_str(), &GetUserMessagesCallback, &result,
        SQLITE_BIND_TEXT(login.c_str()), SQLITE_BIND_END);
    return result;
}

bool SQLiteCore::DeleteUserUndeliveredMessage(const std::string& login, const std::string& message)
{
    const static std::string query{ "DELETE FROM Messages WHERE Login=? AND Content=?" };
    const int error = sqlite3_bind_exec(m_db.get(), query.c_str(), &GetUserMessagesCallback, nullptr,
        SQLITE_BIND_TEXT(login.c_str()), SQLITE_BIND_TEXT(message.c_str()), SQLITE_BIND_END);
    return error == SQLITE_OK;
}

void SQLiteCore::DeleteInstance(boost::string_view db_name)
{
   // const auto it = opened_db_registry.find(db_name);
    //if (it == std::end(opened_db_registry))
      //  opened_db_registry.erase(it);
}

void SQLiteCore::CreateTables()
{
    const static std::string sql{ "BEGIN TRANSACTION;"
"CREATE TABLE IF NOT EXISTS \"Users\" ("
    "\"Display_name\" TEXT NOT NULL CHECK(length(Display_name) <= 30),"
    "\"Login\"	TEXT NOT NULL CHECK(length(Login) >= 4 AND length(Login) <= 30),"
    "\"Password\"	TEXT NOT NULL CHECK(length(Password) >= 6 AND length(Password) <= 32),"
    "\"Image\"	TEXT,"
    "PRIMARY KEY(\"Login\")"
");"
"CREATE TABLE IF NOT EXISTS \"Place\" ("
    "\"ID\"	INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,"
    "\"User_login\"	TEXT NOT NULL,"
    "\"Info\"	TEXT CHECK(length(Info) <= 255),"
    "\"Latitude\"	DOUBLE NOT NULL,"
    "\"Longitude\"	DOUBLE NOT NULL,"
    "FOREIGN KEY(\"User_login\") REFERENCES \"Users\"(\"Login\") ON UPDATE CASCADE"
");"
"CREATE TABLE IF NOT EXISTS \"Messages\" ("
    "\"Login\"	TEXT NOT NULL ,"
    "\"Content\"	TEXT NOT NULL,"
    "PRIMARY KEY(\"Login\", \"Content\"),"
    "FOREIGN KEY(\"Login\") REFERENCES \"Users\"(\"Login\") ON UPDATE CASCADE"
");"
"CREATE TABLE IF NOT EXISTS \"Chats\" ("
    "\"First_participant_login\"	TEXT NOT NULL,"
    "\"Second_participant_login\"	TEXT NOT NULL,"
    "\"Chat_id\"	INTEGER NOT NULL UNIQUE,"
    "PRIMARY KEY(\"Second_participant_login\",\"First_participant_login\"),"
    "FOREIGN KEY(\"Second_participant_login\") REFERENCES \"Users\"(\"Login\") ON UPDATE CASCADE,"
    "FOREIGN KEY(\"First_participant_login\") REFERENCES \"Users\"(\"Login\") ON UPDATE CASCADE"
");"
"COMMIT;"
    };
    /*"INSERT INTO \"Users\" VALUES('Alisa Levadna','alisakisa1','123456',NULL,NULL);"
        "INSERT INTO \"Users\" VALUES('Dima Glushenkov','dimasuper1','123456',NULL,NULL);"
        "INSERT INTO \"Users\" VALUES('Ivan Gritsay','vanya1488','123456',NULL,NULL);"
        "INSERT INTO \"Messages\" VALUES('alisakisa1','dimasuper1',0,1,'ÏÐÈÂÅÒ',3112321313);"
        "INSERT INTO \"Messages\" VALUES('dimasuper1','alisakisa1',0,2,'ÏÐÈÂÅÒ',3112321313);"
        "INSERT INTO \"Chats\" VALUES('alisakisa1','dimasuper1',0);"
        "INSERT INTO \"Chats\" VALUES('dimasuper1','alisakisa1',1);"*/
    char *zErrMsg = 0;

    int rc = sqlite3_exec(m_db.get(), sql.c_str(), nullptr, 0, &zErrMsg);

    if (rc != SQLITE_OK) {
        std::cout << zErrMsg << std::endl;
        //fprintf(stderr, "SQL error: %s\n", zErrMsg);
        //sqlite3_free(zErrMsg);
    }
    else {
       std::cout << "Table created successfully\n";
    }
}

#pragma once

#include <boost/utility/string_view.hpp>
#include <map>
#include <sqlite3.h>
#include "User.h"
#include <boost/asio/io_context_strand.hpp>
#include <optional>

struct sqlite3;

class SQLiteCore : public std::enable_shared_from_this<SQLiteCore>
{
    struct make_shared_enabler;
    //using InstancesRegistry = std::map<boost::string_view, std::shared_ptr<SQLite>, std::less<>>;
    struct closer
    {
        void operator()(sqlite3* p) const
        {
            sqlite3_close(p);
        }
    };
    using sqlite3_unique_ptr = std::unique_ptr<sqlite3, closer>;
    /*------------------------------------------------------------------------------------------*/
    std::string m_db_name;
    sqlite3_unique_ptr m_db;
    //boost::asio::io_context::strand m_strand;
public:
    /*---------------------------------Factory-method-functions---------------------------------*/
    //static InstancesRegistry opened_db_registry; //all current opened db files
    static std::shared_ptr<SQLiteCore> GetInstance(std::string db_name, /*boost::asio::io_context& ioc,*/ bool create);
    static void DeleteInstance(boost::string_view db_name);
    void CreateTables();
    /*------------------------------------------------------------------------------------------*/

    SQLiteCore(const SQLiteCore&) = delete;
    SQLiteCore& operator=(const SQLiteCore&) = delete;
    SQLiteCore(SQLiteCore&&) = default;
    SQLiteCore& operator=(SQLiteCore&&) = delete;
    ~SQLiteCore() = default;

    bool InsertUser(const std::string& display_name, const std::string& login, const std::string& password);
    std::optional<User> GetUserByLogin(const std::string& login);
    bool CheckUserPassword(const std::string& login, const std::string& password);
    std::string GetImageByLogin(const std::string& login);
    bool SetUserImageByLogin(const std::string& login, const std::string& image);

    bool SaveUserMessage(const std::string& login, const std::string& message);
    std::vector<std::string> GetUndeliveredMessages(const std::string& login);
    bool DeleteUserUndeliveredMessage(const std::string& login, const std::string& message);
private:
    explicit SQLiteCore(std::string name);
};
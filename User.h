#pragma once
#include <string>

struct User
{
    std::string display_name;
    std::string login;
    std::string password;
    std::string image;
    template<class charT>
    friend std::basic_ostream<charT>& operator<<(std::basic_ostream<charT>& out, const User& user)
    {
        out << "Display name: " << user.display_name << ", login: " << user.login;
        return out;
    }
};
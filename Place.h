#pragma once
#include <string>
#include <chrono>

struct PlaceInfo
{
    std::string latitude;
    std::string longitude;
    std::string creator_login;
    std::string name;
    std::string category;
    std::string subcategory;
    std::string from_time;
    std::string to_time;
    std::chrono::seconds expire_time;
    std::string expected_people_number;
    std::string expected_expenses;
    std::string description;
    std::string image_base64;
    bool scheduled_exipres = false;
};

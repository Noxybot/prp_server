#include "PlacesManager.h"
#include "shared_state.hpp"
#include <iostream>

#include <future>

PlacesManager::PlacesManager(boost::asio::io_context& ioc, boost::weak_ptr<shared_state> state)
    : m_state(std::move(state))
    , m_timer(make_strand(ioc))
    //, m_deletion_timer(make_strand(ioc))
{}

void PlacesManager::StartProcessing()
{
    ScheduleTimeout();
}

void PlacesManager::ScheduleDelition(uint64_t id, std::chrono::seconds secs)
{
    std::cout << "ScheduleDelition for marker: " << id << " timeout: " << secs.count();
    auto timer = new boost::asio::steady_timer(m_timer.get_executor());
    timer->expires_from_now(secs);
    timer->async_wait([this, timer, id](const boost::system::error_code& ec)
    {
        if (ec)
            return;
        std::cout << "Deleting marker " << id << " by timeout";
        DeletePlaceById(id);
        delete timer;
    });
}



uint64_t PlacesManager::AddPlace(PlaceInfo place)
{
    std::promise<uint64_t> res;
    auto future = res.get_future();
    post(m_timer.get_executor(), [this, w_this = weak_from_this(), place = std::move(place), res = std::move(res)]() mutable
    {
        if (!w_this.lock())
        {
            std::cerr << "PlacesManager::AddPlace: failed to obtain strong ptr\n";
            res.set_value(-1);
            return;
        }
        m_places[++m_last_place_id] = std::move(place);
        res.set_value(m_last_place_id);
    });
    return future.get();
}

bool PlacesManager::AddPlaceImage(uint64_t id, std::string image)
{
    std::promise<bool> res;
    auto future = res.get_future();
    post(m_timer.get_executor(), [this, w_this = weak_from_this(), id, image = std::move(image), res = std::move(res)]() mutable
        {
        if (!w_this.lock())
        {
            res.set_value(false);
            return;
        }
        if (m_places.contains(id))
        {
            m_places[id].image_base64 = std::move(image);
            std::cout << "image to marker:" << id << " added" << std::endl;
            res.set_value(true);
        }
        else
        {
            res.set_value(false);
            std::cerr << "image to marker: " << id <<  " was not added" << std::endl;
        }
        });
    return future.get();
}

std::string PlacesManager::GetPlaceImage(uint64_t id)
{
    std::promise<std::string> res;
    auto future = res.get_future();
    post(m_timer.get_executor(), [this, w_this = weak_from_this(), id, res = std::move(res)]() mutable
    {
        if (!w_this.lock())
        {
            res.set_value("");
            return;
        }
        if (m_places.contains(id))
        {
            std::cout << "image for marker:" << id << " obtained" << std::endl;
            res.set_value(m_places[id].image_base64);
        }
        else
        {
            res.set_value("");
            std::cerr << "image for marker: " << id << " was not obtained" << std::endl;
        }
    });
    return future.wait_for(std::chrono::seconds{ 5 }) == std::future_status::ready ? future.get() : "";
}

void PlacesManager::DeletePlaceById(uint64_t id)
{
    boost::property_tree::ptree root;
    root.put("method", "delete_marker");
    root.put("id", id);
    std::stringstream s_stream;
    write_json(s_stream, root);
    if (auto locked_state = m_state.lock())
        locked_state->send("", s_stream.str());
    post(m_timer.get_executor(), [this, w_this = weak_from_this(), id]
        {
                m_places.erase(id);
                std::cout << "markers left: " << m_places.size() << std::endl;
        });
}

void PlacesManager::Timeout()
{
    static uint64_t i = 0;
    if (!(i % 5))
        std::cout << "PlacesManager::Timeout\n";
    ++i;
    for (auto& place : m_places)
    {
        auto& place_info = place.second;
        if (!place_info.scheduled_exipres)
        {
            ScheduleDelition(place.first, place_info.expire_time);
            place_info.scheduled_exipres = true;
        }
        boost::property_tree::ptree root;
        root.put("method", "draw_marker");
        root.put("id", place.first);
        root.put("latitude", place_info.latitude);
        root.put("longitude", place_info.longitude);
        root.put("creator_login", place_info.creator_login);
        root.put("name", place_info.name);
        root.put("category", place_info.category);
        root.put("subcategory", place_info.subcategory);
        root.put("from_time", place_info.from_time);
        root.put("to_time", place_info.to_time);
        //root.put("creation_time", place_info.creation_time);
        root.put("expected_people_number", place_info.expected_people_number);
        root.put("expected_expenses", place_info.expected_expenses);
        root.put("description", place_info.description);
        std::stringstream s_stream;
        write_json(s_stream, root);
        if (auto locked_state = m_state.lock())
            locked_state->send("", s_stream.str());
    }
    ScheduleTimeout();
}

void PlacesManager::ScheduleTimeout()
{
    m_timer.expires_from_now(std::chrono::seconds{ 5 });
    m_timer.async_wait([this, sh_this = shared_from_this()](const boost::system::error_code& ec)
    {
        if (ec)
        {
            std::cerr << "PlacesManager::ScheduleTimeout ERROR: " << ec.message();
            return;
        }
        Timeout();
    });
}

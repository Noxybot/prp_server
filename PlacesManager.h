#pragma once

#include <unordered_map>
#include <boost/asio/steady_timer.hpp>
#include <boost/smart_ptr/enable_shared_from_this.hpp>
#include "Place.h"

class shared_state;

class PlacesManager : public boost::enable_shared_from_this<PlacesManager>
{
    uint64_t m_last_place_id = 0;
    std::unordered_map<uint64_t, PlaceInfo> m_places;
    boost::weak_ptr<shared_state> m_state;
    boost::asio::steady_timer m_timer;
    //boost::asio::steady_timer m_deletion_timer;

public:
    PlacesManager(boost::asio::io_context& ioc, boost::weak_ptr<shared_state> state);
    void StartProcessing();
    uint64_t AddPlace(PlaceInfo place);
    bool AddPlaceImage(uint64_t id, std::string image);
    std::string GetPlaceImage(uint64_t id);
    void DeletePlaceById(uint64_t id);

private:
    void Timeout();
    void ScheduleTimeout();
    void ScheduleDelition(uint64_t id, std::chrono::seconds secs);
};

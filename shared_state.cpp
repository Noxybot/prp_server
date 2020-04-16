#include "shared_state.hpp"
#include "websocket_session.hpp"
#include <iostream>

shared_state::shared_state(std::string doc_root, boost::asio::io_context& ioc_)
    : doc_root_(std::move(doc_root))
    , m_ioc(ioc_)
    , m_db_core(SQLiteCore::GetInstance("test.db", true))
{}

void shared_state::PostConstruct()
{
    m_places_manager = boost::make_shared<PlacesManager>(m_ioc, weak_from_this());
    m_places_manager->StartProcessing();
}

bool shared_state::userLoggedIn(const std::string& login)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sessions.contains(login);
}

void shared_state::Join(std::string login, boost::shared_ptr<websocket_session> session)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sessions.emplace(std::move(login), std::move(session));
}

void shared_state::leave(const std::string& login)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sessions.erase(login);
    }
    namespace pt = boost::property_tree;
    pt::ptree logout_user_json;
    logout_user_json.put("method", "logout_user");
    logout_user_json.put("login", login);
    std::stringstream s_stream;
    write_json(s_stream, logout_user_json);
    send("", s_stream.str()); //just broadcast login_user
}

// Broadcast a message to all websocket client sessions
void shared_state::send(const std::string login, std::string message)
{
    // Put the message in a shared pointer so we can re-use it for each client
    auto const ss = boost::make_shared<std::string const>(std::move(message));
    if (login.empty()) //broadcast the message if login no login
    {
        // Make a local list of all the weak pointers representing
        // the sessions, so we can do the actual sending without
        // holding the mutex:
        std::vector<boost::weak_ptr<websocket_session>> v;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            v.reserve(m_sessions.size());
            for (const auto& p : m_sessions)
                v.emplace_back(p.second->weak_from_this());
        }

        // For each session in our local list, try to acquire a strong
        // pointer. If successful, then send the message on that session.
        for (const auto& wp : v)
            if (auto sp = wp.lock())
                sp->send(ss);
    }
    else
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto to_session = m_sessions.find(login);
        if (to_session != std::end(m_sessions))
            to_session->second->send(ss);
        else
            std::cerr << "shared_state::send: send message error: no such user: " << login << std::endl;
    }
}

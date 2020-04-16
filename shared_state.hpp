//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/CppCon2018
//

#ifndef BOOST_BEAST_EXAMPLE_WEBSOCKET_CHAT_MULTI_SHARED_STATE_HPP
#define BOOST_BEAST_EXAMPLE_WEBSOCKET_CHAT_MULTI_SHARED_STATE_HPP

#include "stdafx.h"
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include "SQLiteCore.h"
#include "PlacesManager.h"

// Forward declaration
class websocket_session;

// Represents the shared server state
class shared_state : public boost::enable_shared_from_this<shared_state>
{
    std::string const doc_root_;

    // This mutex synchronizes all access to m_sessions
    std::mutex m_mutex;

    // Keep a list of all the connected clients
    std::unordered_map<std::string, boost::shared_ptr<websocket_session>> m_sessions;
    boost::asio::io_context& m_ioc;

public:
    std::shared_ptr<SQLiteCore> m_db_core;
    boost::shared_ptr<PlacesManager> m_places_manager;
    explicit shared_state(std::string doc_root, boost::asio::io_context& ioc);
    void PostConstruct();

    std::string const&
    doc_root() const noexcept
    {
        return doc_root_;
    }

    bool userLoggedIn(const std::string& login);
    void Join(std::string login, boost::shared_ptr<websocket_session> session);
    void leave(const std::string& login);
    void send(std::string login, std::string message);
};

#endif

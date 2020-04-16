//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/CppCon2018
//
#include "stdafx.h"
#include "websocket_session.hpp"
#include <iostream>
#include <chrono>

websocket_session::websocket_session(tcp::socket&& socket, boost::shared_ptr<shared_state> state)
    : ws_(std::move(socket))
    , m_state(std::move(state))
{
    std::cout << "ws: new connection from: " << ws_.next_layer().socket().remote_endpoint().address().to_string() << std::endl;
}

websocket_session::~websocket_session()
{
    std::lock_guard<std::mutex> lock{m_login_mutex };
    std::cout << "ws: logout user: " << m_login << std::endl;
}

void websocket_session::fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << std::endl;
    std::lock_guard<std::mutex> lock{ m_login_mutex };
    m_state->leave(m_login);
}

void websocket_session::on_accept(beast::error_code ec)
{
    // Handle the error, if any
    if(ec)
        return fail(ec, "accept");

    // Add this session to the list of active sessions (will do it after "login_user")
    //m_state->join(shared_from_this());

    // Read a message
    ws_.async_read(
        buffer_,
        beast::bind_front_handler(
            &websocket_session::on_read,
            shared_from_this()));
}

void websocket_session::on_read(beast::error_code ec, std::size_t)
{
    // Handle the error, if any
    if(ec)
        return fail(ec, "read");

    namespace pt = boost::property_tree;
    const auto msg = beast::buffers_to_string(buffer_.data());
    // Clear the buffer
    buffer_.consume(buffer_.size());
    std::stringstream ss;
    ss << msg;
    pt::ptree root;
    read_json(ss, root);
    const auto method = root.get<std::string>("method", "");
    std::cout << "ws: method: " << method << std::endl;
    if (method == "login_user")
    {
        std::unique_lock<std::mutex> lock{ m_login_mutex };
        m_login = root.get<std::string>("login", "");
        const auto login = m_login; //save in local variable to unlock mutex
        lock.unlock();
        if (login.empty())
        {
            std::cerr << "websocket_session::on_read: empty login\n";
            return;
        }
        m_state->Join(login, shared_from_this());

        boost::property_tree::ptree login_user_json;
        login_user_json.put("method", "login_user");
        login_user_json.put("login", login);
        std::stringstream s_stream;
        write_json(s_stream, root);
        m_state->send("", s_stream.str()); //just broadcast login_user

        auto& db_core = m_state->m_db_core;
        auto undelivered_messages = db_core->GetUndeliveredMessages(login);
        std::cout << "ws: user: " << login << " has " << undelivered_messages.size() << " undelievered messages\n";
        
        for (const auto& message: undelivered_messages)
        {
            send(boost::make_shared<std::string>(message));
            db_core->DeleteUserUndeliveredMessage(login, message);
        }
    }
    else if (method == "send_message")
    {
        const auto to_login = root.get<std::string>("to", "");
        const auto from_login = root.get<std::string>("from", "");
        auto now = std::chrono::system_clock::now();
        root.put("timestamp", std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());
        std::stringstream s_stream;
        write_json(s_stream, root);
        if (m_state->userLoggedIn(to_login))
            m_state->send(to_login, s_stream.str());
        else
           m_state->m_db_core->SaveUserMessage(to_login, s_stream.str());
        m_state->send(from_login, s_stream.str());
    }
    // Read another message
    ws_.async_read(
        buffer_,
        beast::bind_front_handler(
            &websocket_session::on_read,
            shared_from_this()));
}

void websocket_session::send(boost::shared_ptr<std::string const> const& ss)
{
    // Post our work to the strand, this ensures
    // that the members of `this` will not be
    // accessed concurrently.
    std::lock_guard<std::mutex> lock{ m_login_mutex };
    if (!m_login.empty())
    {
        net::post(
            ws_.get_executor(),
            beast::bind_front_handler(
                &websocket_session::on_send,
                shared_from_this(),
                ss));
    }
}

void websocket_session::on_send(boost::shared_ptr<std::string const> const& ss)
{
    // Always add to queue
    queue_.push_back(ss);

    // Are we already writing?
    if(queue_.size() > 1)
        return;

    // We are not currently writing, so send this immediately
    ws_.async_write(
        net::buffer(*queue_.front()),
        beast::bind_front_handler(
            &websocket_session::on_write,
            shared_from_this()));
}

void websocket_session::on_write(beast::error_code ec, std::size_t)
{
    // Handle the error, if any
    if(ec)
        return fail(ec, "write");

    // Remove the string from the queue
    queue_.erase(queue_.begin());

    // Send the next message if any
    if(! queue_.empty())
        ws_.async_write(
            net::buffer(*queue_.front()),
            beast::bind_front_handler(
                &websocket_session::on_write,
                shared_from_this()));
}

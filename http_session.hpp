#ifndef BOOST_BEAST_EXAMPLE_WEBSOCKET_CHAT_MULTI_HTTP_SESSION_HPP
#define BOOST_BEAST_EXAMPLE_WEBSOCKET_CHAT_MULTI_HTTP_SESSION_HPP

#include "stdafx.h"
#include "net.hpp"
#include "beast.hpp"
#include "shared_state.hpp"
#include "Place.h"
#include <memory>
#include "http_session.hpp"
#include "websocket_session.hpp"
#include <iostream>
#include "common/consts.h"

/** Represents an established HTTP connection
*/
class http_session : public boost::enable_shared_from_this<http_session>
{
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    boost::shared_ptr<shared_state> state_;

    // The parser is stored in an optional container so we can
    // construct it from scratch it at the beginning of each new message.
    boost::optional<http::request_parser<http::string_body>> parser_;

    struct send_lambda;

    void fail(beast::error_code ec, char const* what);
    void do_read();
    void on_read(beast::error_code ec, std::size_t);
    void on_write(beast::error_code ec, std::size_t, bool close);

public:
    http_session(
        tcp::socket&& socket,
        boost::shared_ptr<shared_state> state);

    void run();
    template<
        class Body, class Allocator,
        class Send>
        void
        handle_request(
            beast::string_view doc_root,
            http::request<Body, http::basic_fields<Allocator>>&& req,
            Send&& send)
    {
        // Returns a bad request response
        auto const bad_request =
            [&req](beast::string_view why)
        {
            http::response<http::string_body> res{ http::status::bad_request, req.version() };
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, "text/html");
            res.keep_alive(req.keep_alive());
            res.body() = std::string(why);
            res.prepare_payload();
            return res;
        };

        // Returns a not found response
        auto const not_found =
            [&req](beast::string_view target)
        {
            http::response<http::string_body> res{ http::status::not_found, req.version() };
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, "text/html");
            res.keep_alive(req.keep_alive());
            res.body() = "The resource '" + std::string(target) + "' was not found.";
            res.prepare_payload();
            return res;
        };
        // Returns a server error response
        auto const server_error =
            [&req](beast::string_view what) 
        {
            http::response<http::string_body> res{ http::status::internal_server_error, req.version() };
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, "text/html");
            res.keep_alive(req.keep_alive());
            res.body() = "An error occurred: '" + std::string(what) + "'";
            res.prepare_payload();
            return res;
        };
        // Returns indication that POST was processed
        auto const created_response =
            [&req](beast::string_view body)
        {
            http::response<http::string_body> res{ http::status::created, req.version() };
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, "application/json");
            res.keep_alive(req.keep_alive());
            res.body() = std::string(body);
            res.prepare_payload();
            return res;
        };
        auto const options_response = //it is response to browser options
            [&req]()
        {
            http::response<http::string_body> res{ http::status::ok, req.version() };
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::access_control_request_method, "POST, GET");
            res.set(http::field::access_control_allow_headers, "content-type");
            res.set(http::field::access_control_allow_origin, "*");
            res.keep_alive(req.keep_alive());
            res.prepare_payload();
            return res;
        };
        auto const ok_response =
            [&req](beast::string_view method, beast::string_view result)
        {
            http::response<http::string_body> res{ http::status::ok, req.version() };
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, "application/json");
            res.set(http::field::access_control_allow_origin, "*");
            res.keep_alive(req.keep_alive());

            boost::property_tree::ptree root;
            root.put("method", method);
            root.put("result", result);
            std::stringstream s_stream;
            write_json(s_stream, root);
            res.body() = s_stream.str();
            res.prepare_payload();
            return res;
        };
        auto const conflict_response =
            [&req](beast::string_view body)
        {
            http::response<http::string_body> res{ http::status::conflict, req.version() };
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, "application/json");
            res.keep_alive(req.keep_alive());
            res.body() = std::string(body);
            res.prepare_payload();
            return res;
        };
        auto const not_found_response =
            [&req](beast::string_view body)
        {
            http::response<http::string_body> res{ http::status::not_found, req.version() };
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, "text/plain");
            res.keep_alive(req.keep_alive());
            res.body() = std::string(body);
            res.prepare_payload();
            return res;
        };

        

        auto it = req.find(http::field::content_type);
        if (it != req.end() && boost::starts_with(it->value(), "application/json") && req.method() == http::verb::post || req.method() == http::verb::get)
        {
            namespace pt = boost::property_tree;
            pt::ptree root;
            std::stringstream ss;
            ss << req.body();
            if (ss.tellp() == 0)
                return;
            // Load the json in this ptree
            read_json(ss, root);
            const auto method = root.get<std::string>("method", "");
            boost::system::error_code err;
            const auto ep = stream_.socket().remote_endpoint(err).address().to_string();
            std::cout << "http: method: " << method << ", from:" << ep << std::endl;
            if (method.empty())
            {
                return send(bad_request("Empty method property"));
            }
            if (method == methods::REGISTER_USER)
            {
                //todo: register user, write to DB
                const auto login = root.get<std::string>("login", "");
                const auto password = root.get<std::string>("password", "");
                const auto display_name = root.get<std::string>("display_name", "");
                if (!state_->m_db_core->InsertUser(display_name, login, password))
                    return send(ok_response(method, "user exists"));
                std::cout << "http: registered user: " << display_name <<  " with login: " << login << std::endl;
                return send(ok_response(method, "registered"));
            }
            if (method == methods::LOGIN_USER)
            {
                const auto login = root.get<std::string>("login", "");
                const auto password = root.get<std::string>("password", "");
                const auto is_fb = root.get<std::string>("isFB", "");
                if (state_->userLoggedIn(login))
                {
                    std::cout << "http: user with login: " << login << " already logged in" << std::endl;
                    return send(ok_response(method, "already logged in"));
                }
                auto user = state_->m_db_core->GetUserByLogin(login);
                if (!user.has_value())
                {
                    std::cout << "http: user with login: " << login << " not found" << std::endl;
                    return send(ok_response(method, "not found"));
                } 
                if (user->password == password)
                {
                    std::cout << "http: successfully logged in user with login: " << login << std::endl;
                    return send(ok_response(method, "logged in"));
                }
                std::cout << "http: user: " << login << ": wrong password" << std::endl;
                return send(ok_response(method, "wrong credentials"));
            }
            if (method == methods::LOGIN_FB_USER)
            {
                const auto login = root.get<std::string>("login", "");
                const auto display_name = root.get<std::string>("display_name", "");
                state_->m_db_core->InsertUser(display_name, login, login); //tmp hack: register FB user with password same as login(FB id)
                if (state_->m_db_core->CheckUserPassword(login, login)) //always true
                {
                    std::cout << "http: successfully logged in FB user with login: " << login << std::endl;
                    return send(ok_response(method, "logged in"));
                }
                assert(false); //should never been here (for now we always successfully login FB user)
            }
            else if (method == methods::ADD_PLACE)
            {
                PlaceInfo place;
                place.latitude = root.get<std::string>("latitude", "");
                if (place.latitude.empty())
                {
                    std::cerr << "method add_place error: empty latitude\n";
                    return send(bad_request("Empty latitude"));
                }
                place.longitude = root.get<std::string>("longitude", "");
                if (place.longitude.empty())
                {
                    std::cerr << "method add_place error: empty longitude\n";
                    return send(bad_request("Empty longitude"));
                }
                place.name = root.get<std::string>("name", "");
                place.category = root.get<std::string>("category", "");
                place.subcategory = root.get<std::string>("subcategory", "");
                place.name = root.get<std::string>("name", "");
                place.category = root.get<std::string>("category", "");
                place.subcategory = root.get<std::string>("subcategory", "");
                place.from_time = root.get<std::string>("from_time", "");
                place.to_time = root.get<std::string>("to_time", "");
                const auto expire = root.get<std::string>("expire_time", "");
                std::cout << "EXPIRE IS: " << expire << std::endl;
                place.expire_time = std::chrono::seconds{ std::atoll(expire.c_str()) };
                std::cout << "final expire: " <<place.expire_time.count() << std::endl;
                place.creator_login = root.get<std::string>("creator_login", "");
                place.expected_people_number = root.get<std::string>("expected_people_number", "");
                place.expected_expenses = root.get<std::string>("expected_expenses", "");
                place.description = root.get<std::string>("description", "");
                const auto id = state_->m_places_manager->AddPlace(std::move(place));
                return send(ok_response(method, std::to_string(id)));
            }
            else if (method == methods::DELETE_MARKER)
            {
                const auto id = root.get<std::string>("id", "");
                state_->m_places_manager->DeletePlaceById(std::atoll(id.c_str()));
                return send(ok_response(method, "place deleted"));
            }
            else if (method == methods::GET_USER_IMAGE)
            {
                const auto login = root.get<std::string>("login", "");
                const auto image = state_->m_db_core->GetImageByLogin(login);
                if (image.empty())
                    return send(ok_response(method, "no image"));
                return send(ok_response(method, image));
            }
            else if (method == methods::UPLOAD_USER_IMAGE)
            {
                const auto login = root.get<std::string>("login", "");
                const auto image = root.get<std::string>("image", "");
                if (!state_->m_db_core->SetUserImageByLogin(login, image))
                    return send(ok_response(method, "image was not uploaded"));
                return send(ok_response(method, "image uploaded"));
            }
            else if (method == methods::GET_DISPLAY_NAME)
            {
                const auto login = root.get<std::string>("login", "");
                const auto user = state_->m_db_core->GetUserByLogin(login);
                if (!user.has_value())
                    return send(ok_response(method, "no such user"));
                return send(ok_response(method, user->display_name));
            }
            else if (method == methods::UPLOAD_MARKER_IMAGE)
            {
                const auto id = root.get<std::string>("id", "");
                auto image = root.get<std::string>("image", "");
                std::cout << "upload marker image size: " << image.size();
                if (state_->m_places_manager->AddPlaceImage(std::atoll(id.c_str()), std::move(image)))
                    return send(ok_response(method, "image added"));
                return send(ok_response(method, "image was not added"));
            }
            else if (method == methods::GET_MARKER_IMAGE)
            {
                const auto id = root.get<std::string>("id", "");
               // if (id < 0)
                    //return send(ok_response(method, "invalid marker id"));
                const auto image = state_->m_places_manager->GetPlaceImage(std::atoll(id.c_str()));
                if (image.empty())
                    return send(ok_response(method, "no image"));
                return send(ok_response(method, image));
            }
            else if (method == methods::GET_USER_STATUS)
            {
                const auto login = root.get<std::string>("login", "");
                if (state_->userLoggedIn(login))
                    return send(ok_response(method, "online"));
                return send(ok_response(method, "offline"));
            }
        }
    }
};

#endif

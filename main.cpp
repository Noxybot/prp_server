
#include "stdafx.h"

#include "listener.hpp"
#include "shared_state.hpp"

#include <boost/asio/signal_set.hpp>
#include <boost/smart_ptr.hpp>
#include <iostream>
#include <vector>
#include <io.h>
#include <fcntl.h>

#include "SQLiteCore.h"

int main(int argc, char* argv[])
{
	SetConsoleOutputCP(65001);
   //auto core = 

   /* auto user_alisa = core->GetUserByLogin("alisakisa1");
    auto non_exist_user = core->GetUserByLogin("alisakisa11");

    if (!core->InsertUser({"EduardV", "ediksyper", "qweASD123", {}}))
    {
        std::cout << "Insertion failed\n";
    }
    else
    {
        std::cout << "Insertion success!\n";
        auto user_ed = core->GetUserByLogin("ediksyper");
        std::cout << *user_ed;
    }
    if (!core->InsertUser({ "EduardV", "ediksyper1", "qweASD123", std::vector<uint8_t>{0x32, 0x32, 0x00} }))
    {
        std::cout << "Insertion failed\n";
    }
    else
    {
        std::cout << "Insertion success!\n";
        auto user_ed = core->GetUserByLogin("ediksyper1");
        std::cout << *user_ed;
    }*/
    // Check command line arguments.
	/*if (argc != 5)
	{
		std::cerr <<
			"Usage: websocket-chat-multi <address> <port> <doc_root> <threads>\n" <<
			"Example:\n" <<
			"    websocket-chat-server 0.0.0.0 8080 . 5\n";
		return EXIT_FAILURE;
	}*/
	const auto address = net::ip::make_address( "0.0.0.0"/*argv[1]*/);
	const auto port = static_cast<unsigned short>(std::atoi("1337"/*argv[2]*/));
	auto doc_root = "/"/*argv[3]*/;
	auto const threads = std::max<int>(1, std::atoi("8"/*argv[4]*/));

	// The io_context is required for all I/O
	net::io_context ioc;

	// Create and launch a listening port
	auto state = boost::make_shared<shared_state>(doc_root, ioc);
	state->PostConstruct();
	boost::make_shared<listener>(
		ioc,
		tcp::endpoint{ address, port },
		state)->run();
	state.reset(); //todo: std::move this shared_ptr
	// Capture SIGINT and SIGTERM to perform a clean shutdown
	net::signal_set signals(ioc, SIGINT, SIGTERM);
	signals.async_wait(
		[&ioc](boost::system::error_code const&, int)
	{
		// Stop the io_context. This will cause run()
		// to return immediately, eventually destroying the
		// io_context and any remaining handlers in it.
		ioc.stop();
	});

	// Run the I/O service on the requested number of threads
	std::vector<std::thread> v;
	v.reserve(threads - 1);
	for (auto i = threads - 1; i > 0; --i)
		v.emplace_back(
			[&ioc]
	{
		ioc.run();
	});
	ioc.run();

	// (If we get here, it means we got a SIGINT or SIGTERM)

	// Block until all the threads exit
	for (auto& t : v)
		t.join();

	return EXIT_SUCCESS;
}
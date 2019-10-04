// pixelgame.cpp : This file contains the 'main' function. Program execution begins and ends there.

#include <iostream>
#include <string>

#define ASIO_STANDALONE

#include "protocol.h"
#include "client_mgr.h"
#include "asio/asio.hpp"
#include "userinput/uinput.h"

asio::io_service io_service;
asio::ip::tcp::resolver resolver(io_service);

constexpr auto service_name = "pixelgame";

int main()
{
	bool running = true;
	
	while (running)
	{
		efiilj::UserInput<std::string> address_query("Enter server name/ip:", "> ");
		efiilj::UserInput<std::string> service_query("Enter port number/service name: ", "> ");

		if (address_query.Show())
		{
			if (address_query.Value() == "exit")
				running = false;
			else
			{
				if (service_query.Show())
				{
					std::cout << "Connecting to " << address_query.Value() << "..." << std::endl;
					auto endpoint_iterator = resolver.resolve(address_query.Value(), service_query.Value());

					asio::ip::tcp::socket socket(io_service);

					try
					{
						asio::connect(socket, endpoint_iterator);
					}
					catch (asio::system_error& e)
					{
						std::cout << "Could not connect - " << e.what();
						break;
					}

					std::cout << (socket.is_open() ? "Connection established!" : "Could not connect - unknown error") << std::endl;
					socket.close();
				}
			}
		}
	}

	return 0;
}



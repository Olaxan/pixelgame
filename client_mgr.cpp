#include "client_mgr.h"
#include <utility>
#include <iostream>

client_mgr::client_mgr(asio::ip::tcp::socket& endpoint)
	: endpoint_(std::move(endpoint)), sequence_(0), id_(0) { }

bool client_mgr::is_ready() const
{
	return endpoint_.is_open() && id_ > 0;
}

bool client_mgr::join(std::shared_ptr<client> player, asio::error_code& err)
{

	player_ = std::move(player);
	
	if (!endpoint_.is_open())
		return false;
	
	join_msg msg{ msg_head {0, 0, 0, msg_type::join}, player_->desc, player_->form, 0 };
	memcpy_s(msg.name, max_name_len, player_->name, max_name_len);
	const unsigned int len = sizeof(join_msg);
	msg.head.length = len;
	
	try
	{
		endpoint_.send(asio::buffer(&msg, len));
		sequence_ = 1;
	}
	catch (asio::system_error& e)
	{
		err = e.code();
		return false;
	}

	msg_head response{};
	try
	{
		endpoint_.receive(asio::buffer(&response, sizeof msg_head));
		if (response.type == msg_type::join)
			id_ = response.id;
		else return false;
	}
	catch (asio::system_error& e)
	{
		err = e.code();
		return false;
	}

	return true;
}

void client_mgr::start() const
{
	bool running = true;

	std::cout << "Game manager running: seqno " << sequence_ << ", id " << id_ << std::endl;
	
	while (running)
	{
		
	}
}

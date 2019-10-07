#pragma once

#define ASIO_STANDALONE
#include "asio/asio.hpp"

#include "protocol.h"
#include "client.h"
#include <string>
#include <vector>

class client_mgr
{
private:

	asio::ip::tcp::socket endpoint_;
	std::shared_ptr<client> player_;
	std::vector<client> others_;
	unsigned int sequence_;
	unsigned int id_;
	
public:
	explicit client_mgr(asio::ip::tcp::socket& endpoint);

	void player(std::shared_ptr<client> player) { player_ = std::move(player); }
	client player() const { return *player_; }
	
	bool is_ready() const;
	bool join(std::shared_ptr<client> player, asio::error_code& err);
	void start() const;
};

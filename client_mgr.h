#pragma once

#define ASIO_STANDALONE
#include "asio/asio.hpp"

#include "protocol.h"
#include "client.h"
#include <string>
#include <map>

constexpr unsigned int max_buf_len = 512;

class client_mgr
{
private:

	asio::ip::tcp::socket endpoint_;
	std::shared_ptr<client> self_;
	std::map<int, client> players_;
	unsigned int sequence_;
	unsigned int id_;
	bool running_;
	
	enum directions
	{
		forward,
		backward,
		left,
		right,
		none
	};

	static directions get_dir(const std::string& input);
	
public:
	explicit client_mgr(asio::ip::tcp::socket& endpoint);

	void player(std::shared_ptr<client> player) { self_ = std::move(player); }
	client player() const { return *self_; }
	
	bool is_ready();
	bool join(std::shared_ptr<client> player, asio::error_code& err);
	void start();
	void update();
	void input();
	void move(directions dir, int count);
};

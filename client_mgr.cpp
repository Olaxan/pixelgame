﻿#include "client_mgr.h"
#include "userinput/uinput.h"
#include "utils.h"

#include <utility>
#include <iostream>
#include <thread>

client_mgr::directions client_mgr::get_dir(const std::string& input)
{
	if (input == "up")
		return up;
	else if (input == "down")
		return down;
	else if (input == "left")
		return left;
	else if (input == "right")
		return right;

	return none;
}

client_mgr::client_mgr(asio::ip::tcp::socket& endpoint, std::string canvas_address, std::string canvas_service)
	: endpoint_(std::move(endpoint)), canvas_address_(std::move(canvas_address)), canvas_service_(std::move(canvas_service)),
	  sequence_(0), id_(0), is_running_(false), state_(idle) { }

bool client_mgr::is_ready() const
{
	return endpoint_.is_open() && id_ > 0;
}

bool client_mgr::join(client player, asio::error_code& err)
{
	
	if (!endpoint_.is_open())
		return false;

	// Create join message
	join_msg msg{ msg_head {sizeof join_msg, 0, 0, msg_type::join}, player.desc, player.form, 0 };
	memcpy_s(msg.name, max_name_len, player.name, max_name_len);
	
	try
	{
		// Send join message to server
		endpoint_.send(asio::buffer(&msg, msg.head.length));
		sequence_ = 1;
	}
	catch (asio::system_error& e)
	{
		err = e.code();
		return false;
	}

	// Create join message response
	msg_head response {};
	try
	{
		// Retrieve response from server
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

bool client_mgr::start()
{

	if (!is_ready())
		return false;
	
	is_running_ = true;
	
	// Start update listener thread
	update_listener_ = std::thread(&client_mgr::update, this);
	
	while (is_running_)
	{
		// Listen for user input
		input();
	}

	try
	{
		// Create leave message and send to server
		// No need to listen for reply
		
		std::cout << "Disconnecting..." << std::endl;
		leave_msg leave{ msg_head{sizeof leave_msg, sequence_, id_, msg_type::leave} };
		endpoint_.send(asio::buffer(&leave, leave.head.length));
		id_ = sequence_ = 0;
	}
	catch (asio::system_error& e) { }

	update_listener_.detach();
	return true;
}

void client_mgr::update()
{
	while (is_running_)
	{
		if (!is_ready())
			return;

		receive();
	}
}

void client_mgr::receive()
{

	state_ = waiting;
	
	asio::streambuf received_buffer;
	std::istream data_stream(&received_buffer);
	const unsigned int header_length = sizeof change_msg;

	try
	{
		// Hold until receiving a change message header
		// Then commit to buffer
		endpoint_.receive(received_buffer.prepare(header_length));
		received_buffer.commit(header_length);
		state_ = header_received;
	}
	catch (asio::system_error& e)
	{
		std::cerr << "Header transmission error: " << e.what() << std::endl;
		return;
	}
	catch (std::length_error& le)
	{
		std::cerr << "Header length invalid (" << header_length << "): " << le.what() << std::endl;
		return;
	}

	// Create change message header and read data from input stream
	change_msg msg{};
	data_stream.read(reinterpret_cast<char*>(&msg), header_length);

	const unsigned int message_id = msg.head.id;
	const unsigned int message_length = msg.head.length;
	const unsigned int remaining_length = message_length - header_length;
	auto player_iterator = players_.find(message_id);

	try
	{
		// Receive rest of message and commit to buffer
		endpoint_.receive(received_buffer.prepare(remaining_length));
		received_buffer.commit(remaining_length);
		state_ = message_received;
	}
	catch (asio::system_error& e)
	{
		std::cerr << "Message transmission error: " << e.what() << std::endl;
		return;
	}
	catch (std::length_error& le)
	{
		std::cerr << "Message length invalid (" << message_length << "): " << le.what() << std::endl;
		return;
	}

	switch (msg.type)
	{
	case new_player:
	{
		new_player_msg np{};
		data_stream.read(reinterpret_cast<char*>(&np.desc), remaining_length);

		if (msg.head.id != id_)
			std::cout << np.name << " (" << to_string(np.form) << ") joined the game!" << std::endl;

		players_[message_id] = client(np);

		break;
	}
	case player_leave:
	{
		player_leave_msg pl{};
		data_stream.read(reinterpret_cast<char*>(&pl.msg), remaining_length);
			
		if (player_iterator != players_.end())
		{
			client* cli = &player_iterator->second;
			std::cout << "Player " << cli->name << " left the game." << std::endl;
			players_.erase(player_iterator);
		}
	}
	case new_player_position:
	{
		new_player_position_msg pp{};
		data_stream.read(reinterpret_cast<char*>(&pp.pos), remaining_length);

		if (player_iterator != players_.end())
		{
			client* cli = &player_iterator->second;
			cli->position = pp.pos;
		}
		break;
	}
	default:
		std::cout << "Unknown server message!" << std::endl;
		break;
	}

	draw();
	state_ = idle;
}

void client_mgr::input()
{
	efiilj::UserInput<std::string> in_prompt("", "CMD: ");

	if (in_prompt.Show())
	{
		const std::string in_com = in_prompt.Value();
		auto com = split(in_com, " \t\n\v\f\r");

		if (com.empty())
			return;
		
		if (com[0] == "exit")
		{
			is_running_ = false;
		}
		else if (com[0] == "move")
		{
			int m_count = 1;
			
			if (com.size() == 3)
			{
				try
				{
					m_count = std::stoi(com[2]);
				}
				catch (std::invalid_argument& e)
				{
					std::cout << "Invalid move count [move 'direction' ('count')]" << std::endl;
					return;
				}
			}

			if (com.size() > 1)
			{
				const directions dir = get_dir(com[1]);
				if (dir != none)
					move(dir, m_count);
			}
			else 
				std::cout << "Invalid command syntax [move 'direction' ('count')]" << std::endl;
		}
	}
}

void client_mgr::move(const directions dir, const int count)
{
	if (!is_ready())
		return;

	const auto it = players_.find(id_);
	if (it == players_.end())
		return;
	
	for (int i = 0; i < count; i++)
	{
		
		// Hold until update thread is ready for new message
		while (state_ != waiting) { }
		
		coordinate cord = it->second.position;
		cord.x += (dir == right) - (dir == left);
		cord.y += (dir == down) - (dir == up);

		move_event msg_move
		{
			event_msg
			{
				msg_head{sizeof move_event, sequence_, id_, event },
				event_type::move
			},
			coordinate(cord),
			coordinate {0, 0}
		};

		endpoint_.send(asio::buffer(&msg_move, msg_move.event.head.length));
		sequence_++;
		state_ = hold;
	}
}

void client_mgr::draw() const
{
	try
	{

		// Create UDP port and resolve canvas address / port
		asio::io_service io_service;
		asio::ip::udp::resolver resolver(io_service);
		asio::ip::udp::endpoint receiver_endpoint = *resolver.resolve(canvas_address_, canvas_service_);
		asio::ip::udp::socket socket(io_service);
		socket.open(asio::ip::udp::v6());

		// Send "clear" packet with color black
		draw_packet clear{ -1, -1, 0 };
		socket.send_to(asio::buffer(&clear, sizeof draw_packet), receiver_endpoint);

		// Loop through players and send pixel data
		for (auto it = players_.begin(); it != players_.end(); ++it)
		{
			const client* cli = &it->second;
			
			draw_packet pixel
			{
				swap_endian<int>(cli->position.x + 100),
				swap_endian<int>(cli->position.y + 100),
				swap_endian<int>((cli->get_rgb()))
			};
			
			socket.send_to(asio::buffer(&pixel, sizeof draw_packet), receiver_endpoint);
		}

		socket.close();
	}
	catch (asio::system_error& e)
	{
		std::cout << "Draw error: " << e.what() << std::endl;
	}
}

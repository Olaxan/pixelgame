#include "client_mgr.h"
#include "userinput/uinput.h"
#include "utils.h"

#include <utility>
#include <iostream>
#include <thread>

client_mgr::directions client_mgr::get_dir(const std::string& input)
{
	if (input == "forward")
		return forward;
	else if (input == "backward")
		return backward;
	else if (input == "left")
		return left;
	else if (input == "right")
		return right;

	return none;
}

client_mgr::client_mgr(asio::ip::tcp::socket& endpoint, const std::string canvas_address, const std::string canvas_service)
	: endpoint_(std::move(endpoint)), canvas_address_(canvas_address), canvas_service_(canvas_service),
	  sequence_(0), id_(0), running_(false) { }

bool client_mgr::is_ready()
{
	return endpoint_.is_open() && id_ > 0;
}

bool client_mgr::join(std::shared_ptr<client> player, asio::error_code& err)
{

	self_ = std::move(player);
	
	if (!endpoint_.is_open())
		return false;
	
	join_msg msg{ msg_head {sizeof join_msg, 0, 0, msg_type::join}, self_->desc, self_->form, 0 };
	memcpy_s(msg.name, max_name_len, self_->name, max_name_len);
	
	try
	{
		endpoint_.send(asio::buffer(&msg, msg.head.length));
		sequence_ = 1;
	}
	catch (asio::system_error& e)
	{
		err = e.code();
		return false;
	}

	msg_head response {};
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

void client_mgr::start()
{
	running_ = true;

	std::cout << "Game manager running: sequence " << sequence_ << ", id " << id_ << std::endl;
	std::cout << "Manager is listening..." << std::endl;
	std::thread update_listener(&client_mgr::update, this);
	
	while (running_)
	{
		input();
	}

	try
	{
		std::cout << "Disconnecting..." << std::endl;
		leave_msg leave{ msg_head{sizeof leave_msg, sequence_, id_, msg_type::leave} };
		endpoint_.send(asio::buffer(&leave, leave.head.length));
		id_ = sequence_ = 0;
	}
	catch (asio::system_error& e) { }

	update_listener.detach();
}

void client_mgr::update()
{
	while (running_)
	{
		if (!is_ready())
			return;

		try
		{
			asio::error_code error;
			asio::streambuf received_buffer;

			const unsigned int header_length = sizeof change_msg;

			endpoint_.receive(received_buffer.prepare(header_length));
			received_buffer.commit(header_length);
			std::istream data_stream(&received_buffer);
			
			change_msg msg{};
			data_stream.read(reinterpret_cast<char*>(&msg), header_length);

			const unsigned int message_id = msg.head.id;
			const unsigned int message_length = msg.head.length;
			const unsigned int remaining_length = message_length - header_length;
			auto player_iterator = players_.find(message_id);
			
			switch (msg.type)
			{
				case new_player:
				{
					new_player_msg np {};
					endpoint_.receive(asio::buffer(&np.desc, remaining_length));
						
					if (msg.head.id != id_)
						std::cout << np.name << " (" << to_string(np.form) << ") joined the game!" << std::endl;
						
					players_[message_id] = client(np);
						
					break;
				}
				case player_leave:
				{
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
					endpoint_.receive(asio::buffer(&pp.pos, remaining_length));
						
					if (player_iterator != players_.end())
					{
						client* cli = &player_iterator->second;
						cli->position = pp.pos;
					}
					break;
				}
				default: ;
			}

			draw();
		}
		catch (asio::system_error& e)
		{
			std::cout << "Transmission error: " << e.what() << std::endl;
		}
	}
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
			running_ = false;
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
		coordinate cord = it->second.position;
		cord.x += (dir == right - (dir == left));
		cord.y += (dir == forward - (dir == backward));

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

		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
}

void client_mgr::draw() const
{
	try
	{
		asio::io_service io_service;
		asio::ip::udp::resolver resolver(io_service);
		asio::ip::udp::endpoint receiver_endpoint = *resolver.resolve(canvas_address_, canvas_service_);
		asio::ip::udp::socket socket(io_service);
		socket.open(asio::ip::udp::v6());

		draw_packet clear{ -1, -1, 0 };
	
		socket.send_to(asio::buffer(&clear, sizeof draw_packet), receiver_endpoint);

		for (auto it = players_.begin(); it != players_.end(); ++it)
		{
			const client* cli = &it->second;
			
			draw_packet pixel
			{
				swap_endian<int>(cli->position.x + 100),
				swap_endian<int>(cli->position.y + 100),
				swap_endian<int>((16711680)) /*cli->form + 1) * INT_MAX / 4*/
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

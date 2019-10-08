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

client_mgr::client_mgr(asio::ip::tcp::socket& endpoint)
	: endpoint_(std::move(endpoint)), sequence_(0), id_(0), running_(false) { }

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

		char bytes[max_buf_len];
		change_msg ch_msg_head{};

		try
		{
			endpoint_.receive(asio::buffer(bytes, max_buf_len));
		}
		catch (asio::system_error& e)
		{
			return;
		}

		memcpy_s(&ch_msg_head, sizeof change_msg, bytes, sizeof change_msg);

		const unsigned int id = ch_msg_head.head.id;
		const auto it = players_.find(id);

		switch (ch_msg_head.type)
		{
		case new_player_position:
		{
			new_player_position_msg pm_msg{ };
			memcpy_s(&pm_msg, sizeof new_player_position_msg, bytes, sizeof new_player_position_msg);
			if (it != players_.end())
			{
				client* cli = &it->second;
				cli->position = pm_msg.pos;
				std::cout << cli->name << " moved to " << cli->position.x << ", " << cli->position.y << std::endl;
			}

			break;
		}
		case new_player:
		{
			std::cout << "Player joined!" << std::endl;
			new_player_msg np_msg{ };
			memcpy_s(&np_msg, sizeof new_player_msg, bytes, sizeof new_player_msg);
			players_[id] = client(np_msg);

			break;
		}
		case player_leave:
		{
			std::cout << "Player left!" << std::endl;
			if (it != players_.end())
				players_.erase(it);

			break;
		}
		default:
			std::cout << "Unknown server response" << std::endl;
			break;
		}

		//std::cout << "Received data (id " << id << ") " << (id == id_ ? "[self]" : "[other]") << std::endl;
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

	coordinate cord = it->second.position;
	cord.x += (dir == right - (dir == left)) * count;
	cord.y += (dir == forward - (dir == backward)) * count;
	
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
}

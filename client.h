#pragma once
#include "protocol.h"
#include <string>

struct client
{
	client(): position(), form(), desc() { }

	client(std::string& name, const object_form form, const object_desc desc)
		: position(), form(form), desc(desc)
	{
		name.copy(this->name, name.size());
	}

	explicit client(new_player_msg& msg)
	: position(), form(msg.form), desc(msg.desc)
	{
		memcpy_s(msg.name, max_name_len, name, max_name_len);
	}

	coordinate position;
	object_form form;
	object_desc desc;
	char name[max_name_len] {};

	bool operator < (const client other) const
	{
		return memcmp(this, &other, sizeof(client)) > 0;
	};
};

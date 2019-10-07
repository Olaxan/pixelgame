#pragma once
#include "protocol.h"
#include <string>

struct client
{
	client(std::string& name, const object_form form, const object_desc desc)
		: position(), form(form), desc(desc)
	{
		name.copy(this->name, name.size());
	}
	
	coordinate position;
	object_form form;
	object_desc desc;
	char name[max_name_len] {};
};

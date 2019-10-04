#pragma once

class client_mgr
{
private:
	bool can_connect_;
	
public:
	client_mgr(const char* address, unsigned int port);
};

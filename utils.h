#pragma once
#include <string>
#include <vector>

inline std::vector<std::string> split(std::string input, const std::string& splitter)
{
	unsigned int index = input.find_first_of(splitter);
	std::vector<std::string> split;
	
	while (index != std::string::npos)
	{
		std::string sub = input.substr(0, index);
		if (!sub.empty())
			split.push_back(sub);
		
		input.erase(0, index + 1);
		index = input.find_first_of(splitter);
	}
	
	if (!input.empty())
		split.push_back(input);
	
	return split;
}

#include "Prompt.h"
#include <JSL.h>
#include <iostream>

std::string App::Prompt(const std::string &question, const std::string &defaultValue)
{
	std::cout << JSL::Display::Green() << question << ":";
	if (!defaultValue.empty()) std::cout << JSL::Display::Colour(100, 100, 100) << " [" << defaultValue << "] ";
	std::cout << JSL::Display::Cyan() << std::flush;

	std::string response;
	if (!std::getline(std::cin, response))
	{
		std::cout << JSL::Display::ResetAll() << "\n"; // keep later output off the prompt line
		if (defaultValue.empty()) throw std::runtime_error("input closed before an answer to '" + question + "' was given");
		return defaultValue;
	}
	std::cout << JSL::Display::ResetAll() << std::flush;
	// trimmed, including \r so piped Windows line endings don't leak into answers
	auto first = response.find_first_not_of(" \t\r");
	if (first == std::string::npos) return defaultValue;
	auto last = response.find_last_not_of(" \t\r");
	return response.substr(first, last - first + 1);
}

bool App::Confirm(const std::string &question, bool defaultValue)
{
	while (true)
	{
		auto response = Prompt(question, defaultValue ? "yes" : "no");
		if (JSL::String::iEquals(response, "y") || JSL::String::iEquals(response, "yes")) return true;
		if (JSL::String::iEquals(response, "n") || JSL::String::iEquals(response, "no")) return false;
		std::cout << "Please answer yes or no\n";
	}
}

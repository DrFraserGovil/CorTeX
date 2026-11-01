#pragma once
#include <string>

//! Questions for the author at the terminal
namespace App
{
	//! Asks a question, showing the default (if any), and reads one line. Empty input gives the default. If input has closed (EOF, or
	//! stdin redirected), the default is used if there is one; otherwise this throws, since there's no sensible answer
	std::string Prompt(const std::string &question, const std::string &defaultValue = "");

	//! Asks a yes/no question until it gets an answer
	bool Confirm(const std::string &question, bool defaultValue);
} // namespace App

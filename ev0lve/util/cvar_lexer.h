
#ifndef UTIL_CVAR_LEXER_H
#define UTIL_CVAR_LEXER_H

#include <map>
#include <vector>
#include <string>

namespace util
{
	std::vector<std::string> parse_cvars(std::string cmd);
}

#endif // UTIL_CVAR_LEXER_H

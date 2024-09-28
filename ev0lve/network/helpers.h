
#ifndef NETWORK_HELPERS_H
#define NETWORK_HELPERS_H

namespace network
{
	struct user_data
	{
		std::string username{};
		std::string expiry{};
		unsigned int avatar_size{};
	};
	
	inline std::string session{};
	
	void disconnect();
	user_data get_user_data();
	std::string get_decoded_avatar();
}

#endif // NETWORK_HELPERS_H


#include <network/app_hack.h>
#include <network/helpers.h>
#include <util/util.h>
#include <gzip/decompress.hpp>

#include <VirtualizerSDK.h>

void network::disconnect()
{
	VIRTUALIZER_SHARK_WHITE_START;
	NETWORK()->shutdown();
	VIRTUALIZER_SHARK_WHITE_END;
}

network::user_data network::get_user_data()
{
	return {
		NETWORK()->username,
		NETWORK()->expiry,
		NETWORK()->avatar_size
	};
}

std::string network::get_decoded_avatar()
{
	const auto s = evo::util::base64_decode(NETWORK()->avatar);
	return gzip::decompress(s.data(), s.size());
}

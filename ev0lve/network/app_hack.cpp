
#include <network/app_hack.h>
#include <network/helpers.h>
#include <data/hello.h>
#include <data/version.h>
#include <gui/gui.h>

#include <VirtualizerSDK.h>

using namespace evo;
using namespace network;

void app_hack::bind_callbacks(const std::shared_ptr<services::messenger> &msg)
{
	VIRTUALIZER_SHARK_WHITE_START;

	msg->allow_quit = false;
	msg->on_connect = [&]() {
		data::hello inf{};
		inf.version = data::version_hack;
		inf.session = session;
		inf.client = data::ct_hack;
		
		data::msg_t m(msg::msg_hello);
		m.set_data(inf);

		MESSENGER()->add(m, [&](const data::data_t& d, data::msg_t& resp) {
			if (!resp.id || resp.error.code)
			{
				quit();
				return;
			}

			const auto user = resp.get_data<data::user_data>();
			username = user.username;
			avatar = user.avatar;
			avatar_size = user.avatar_size;
			expiry = user.expiry;
			
			is_verified = true;
		});

		if (expecting_failure)
			gui::notify.add(std::make_shared<gui::notification>(XOR("Connected"), XOR("Network services are available")));

		expecting_failure = false;
		retries_left = 3;
	};
	
	msg->on_connect_error = [&](const std::string& e) {
		if (!expecting_failure)
			quit();
		else
		{
			reconnect();
			gui::notify.add(std::make_shared<gui::notification>(XOR("Connection failed"), XOR("Retrying in 30s")));
		}
	};
	
	msg->on_disconnect = [&]() {
		gui::notify.add(std::make_shared<gui::notification>(XOR("Connection lost"), XOR("Network services are unavailable")));
		expecting_failure = true;
		reconnect();
	};
	
	VIRTUALIZER_SHARK_WHITE_END;
}

void app_hack::reconnect()
{
	if (!retries_left)
	{
		QUIT_ERROR("Connection failed");
	}

	std::thread([&]() {
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(30s);
		std::this_thread::yield();

		--retries_left;
		MESSENGER()->start();
	}).detach();
}
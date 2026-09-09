#include "livekit_api.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

namespace livekit {

struct ConnectOptions {
    std::string url;
    std::string token;
    std::string room_name;
};

class Room {
public:
    Room() : room_(livekit_api::Room::init()) {}
    ~Room() = default;

    bool connect(const ConnectOptions& options)
    {
        livekit_api::ConnectOptions api_options {
            .url = options.url,
            .token = options.token,
            .room_name = options.room_name,
        };

        auto connected = room_->connect(api_options).get();
        return connected.connection_state == livekit_api::ConnectionState::kConnected;
    }
private:
    std::shared_ptr<livekit_api::Room> room_;
};

} // namespace livekit

int main() {
    livekit::Room room;
    assert(room.connect({
        .url = "wss://example.livekit.cloud",
        .token = "development-token",
        .room_name = "demo-room",
    }));

    // auto room = livekit_api::Room::init();

    // livekit_api::ConnectOptions options {
    //     .url = "wss://example.livekit.cloud",
    //     .token = "development-token",
    //     .room_name = "demo-room",
    // };

    // auto connected = room->connect(options).get();
    // assert(connected.connection_state == livekit_api::ConnectionState::kConnected);
    // assert(connected.room_name == "demo-room");
    // assert(connected.participant_count == 1);

    // auto sequence = room->send_data(std::vector<uint8_t> {1, 2, 3}).get();
    // assert(sequence == 1);

    // room->disconnect();
    // auto disconnected = room->snapshot();
    // assert(disconnected.connection_state == livekit_api::ConnectionState::kDisconnected);
    // assert(disconnected.participant_count == 0);

    return 0;
}

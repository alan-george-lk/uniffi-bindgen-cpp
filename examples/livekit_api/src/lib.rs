use std::sync::Mutex;

#[derive(Clone, Debug, uniffi::Enum)]
pub enum ConnectionState {
    Disconnected,
    Connecting,
    Connected,
}

#[derive(uniffi::Record)]
pub struct ConnectOptions {
    pub url: String,
    pub token: String,
    pub room_name: String,
}

#[derive(uniffi::Record)]
pub struct RoomSnapshot {
    pub room_name: String,
    pub connection_state: ConnectionState,
    pub participant_count: u32,
}

#[derive(uniffi::Object)]
pub struct Room {
    state: Mutex<RoomState>,
}

struct RoomState {
    room_name: String,
    connection_state: ConnectionState,
    participant_count: u32,
    next_data_sequence: u64,
}

#[uniffi::export]
impl Room {
    #[uniffi::constructor]
    pub fn new() -> Self {
        Self {
            state: Mutex::new(RoomState {
                room_name: String::new(),
                connection_state: ConnectionState::Disconnected,
                participant_count: 0,
                next_data_sequence: 0,
            }),
        }
    }

    /// Simulates connecting to a room and returns its initial snapshot.
    pub async fn connect(&self, options: ConnectOptions) -> RoomSnapshot {
        {
            let mut state = self.state.lock().expect("room state lock poisoned");
            state.connection_state = ConnectionState::Connecting;
        }

        // Keep this API asynchronous without introducing a runtime dependency.
        // A production SDK would await signaling here.
        std::future::ready(()).await;

        let mut state = self.state.lock().expect("room state lock poisoned");
        state.room_name = options.room_name;
        state.connection_state = ConnectionState::Connected;
        state.participant_count = 1;
        RoomSnapshot::from(&*state)
    }

    pub fn disconnect(&self) {
        let mut state = self.state.lock().expect("room state lock poisoned");
        state.connection_state = ConnectionState::Disconnected;
        state.participant_count = 0;
    }

    pub fn snapshot(&self) -> RoomSnapshot {
        let state = self.state.lock().expect("room state lock poisoned");
        RoomSnapshot::from(&*state)
    }

    /// Simulates sending a data packet and returns its sequence number.
    pub async fn send_data(&self, payload: Vec<u8>) -> u64 {
        std::future::ready(()).await;

        let mut state = self.state.lock().expect("room state lock poisoned");
        state.next_data_sequence += 1;
        println!("Sent {} bytes of room data", payload.len());
        state.next_data_sequence
    }
}

impl From<&RoomState> for RoomSnapshot {
    fn from(state: &RoomState) -> Self {
        Self {
            room_name: state.room_name.clone(),
            connection_state: state.connection_state.clone(),
            participant_count: state.participant_count,
        }
    }
}

uniffi::setup_scaffolding!("livekit_api");

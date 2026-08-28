use std::sync::Mutex;

#[derive(uniffi::Record)]
pub struct LogEntry {
    pub timestamp: u64,
    pub level: LogLevel,
    pub message: String,
}

#[derive(Debug, PartialOrd, PartialEq, uniffi::Enum)]
pub enum LogLevel {
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Critical = 5,
}

#[derive(uniffi::Object)]
pub struct Logger {
    state: Mutex<LoggerState>,
}

struct LoggerState {
    level: LogLevel,
}

#[uniffi::export]
impl Logger {
    #[uniffi::constructor]
    /// Create a new logger with the given level.
    pub fn new(level: LogLevel) -> Self {
        Self {
            state: Mutex::new(LoggerState { level }),
        }
    }

    /// Set the level of the logger at runtime.
    pub fn set_level(&self, level: LogLevel) {
        self.state.lock().unwrap().level = level;
    }

    /// Log an info message.
    pub fn info(&self, message: String) {
        self.log_internal(LogEntry {
            timestamp: std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).unwrap().as_secs(),
            level: LogLevel::Info,
            message,
        });
    }

    pub fn debug(&self, message: String) {
        self.log_internal(LogEntry {
            timestamp: std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).unwrap().as_secs(),
            level: LogLevel::Debug,
            message,
        });
    }

}

impl Logger {
    fn log_internal(&self, entry: LogEntry) {
        let state = self.state.lock().unwrap();
        if entry.level < state.level {
            return;
        }

        println!("[{} {:?}]: {}", entry.timestamp, entry.level, entry.message);
    }
}

uniffi::setup_scaffolding!("logger");

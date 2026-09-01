class AsyncCancelledError: public std::runtime_error {
public:
    AsyncCancelledError(): std::runtime_error("UniFFI async call cancelled") {}
};

class AsyncDispatcherError: public std::runtime_error {
public:
    AsyncDispatcherError(): std::runtime_error("UniFFI async dispatcher rejected a continuation") {}
};

using AsyncTask = std::function<void()>;
using AsyncDispatcher = std::function<bool(AsyncTask)>;
using AsyncDispatcherShutdown = std::function<void()>;

namespace detail {

class DefaultAsyncDispatcher {
public:
    DefaultAsyncDispatcher(): worker_([this] { run(); }) {}

    DefaultAsyncDispatcher(const DefaultAsyncDispatcher &) = delete;
    DefaultAsyncDispatcher &operator=(const DefaultAsyncDispatcher &) = delete;

    ~DefaultAsyncDispatcher() {
        shutdown();
    }

    bool dispatch(AsyncTask task) {
        std::lock_guard<std::mutex> guard(mutex_);
        if (!accepting_ || tasks_.size() >= MAX_QUEUED_TASKS) {
            return false;
        }
        tasks_.push_back(std::move(task));
        ready_.notify_one();
        return true;
    }

    void shutdown() noexcept {
        {
            std::lock_guard<std::mutex> guard(mutex_);
            accepting_ = false;
        }
        ready_.notify_all();
        if (worker_.joinable()) {
            worker_.join();
        }
    }

private:
    void run() noexcept {
        for (;;) {
            AsyncTask task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                ready_.wait(lock, [this] { return !tasks_.empty() || !accepting_; });
                if (tasks_.empty()) {
                    return;
                }
                task = std::move(tasks_.front());
                tasks_.pop_front();
            }
            try {
                task();
            } catch (...) {
                // Generated continuation tasks are noexcept. Protect the worker from
                // consumer-provided tasks that violate that contract.
            }
        }
    }

    static constexpr std::size_t MAX_QUEUED_TASKS = 1024;
    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<AsyncTask> tasks_;
    bool accepting_ = true;
    std::thread worker_;
};

inline DefaultAsyncDispatcher &default_async_dispatcher() {
    static DefaultAsyncDispatcher dispatcher;
    return dispatcher;
}

struct AsyncDispatcherState {
    AsyncDispatcherState():
        dispatch([](AsyncTask task) {
            return default_async_dispatcher().dispatch(std::move(task));
        }),
        shutdown([] { default_async_dispatcher().shutdown(); }) {}

    std::mutex mutex;
    bool accepting = true;
    bool started = false;
    AsyncDispatcher dispatch;
    AsyncDispatcherShutdown shutdown;
};

inline AsyncDispatcherState &async_dispatcher_state() {
    static AsyncDispatcherState state;
    return state;
}

inline bool dispatch_async(AsyncTask task) noexcept {
    auto &state = async_dispatcher_state();
    std::lock_guard<std::mutex> guard(state.mutex);
    if (!state.accepting) {
        return false;
    }
    state.started = true;
    try {
        return state.dispatch(std::move(task));
    } catch (...) {
        return false;
    }
}

} // namespace detail

// Installs the process-wide continuation dispatcher. The dispatcher returns true only
// when it has accepted the task. shutdown must stop accepting work, drain accepted
// tasks, and return only when they can no longer call generated code.
inline void set_async_dispatcher(
    AsyncDispatcher dispatcher,
    AsyncDispatcherShutdown shutdown
) {
    if (!dispatcher || !shutdown) {
        throw std::invalid_argument(
            "UniFFI async dispatcher and shutdown function must not be empty"
        );
    }
    auto &state = detail::async_dispatcher_state();
    std::lock_guard<std::mutex> guard(state.mutex);
    if (!state.accepting) {
        throw std::logic_error("UniFFI async dispatcher has already shut down");
    }
    if (state.started) {
        throw std::logic_error(
            "UniFFI async dispatcher must be installed before the first async call"
        );
    }
    state.dispatch = std::move(dispatcher);
    state.shutdown = std::move(shutdown);
}

// Permanently stops continuation dispatch for this process. Call this before unloading
// code used by a custom dispatcher or generated bindings.
inline void shutdown_async_dispatcher() noexcept {
    AsyncDispatcherShutdown shutdown;
    {
        auto &state = detail::async_dispatcher_state();
        std::lock_guard<std::mutex> guard(state.mutex);
        if (!state.accepting) {
            return;
        }
        state.accepting = false;
        shutdown = std::move(state.shutdown);
    }
    if (shutdown) {
        try {
            shutdown();
        } catch (...) {
        }
    }
}

// A C++17-compatible asynchronous operation returned by foreign implementations of
// UniFFI async callback interfaces.  Implementations arrange their own scheduling and
// invoke exactly one completion callback.  The returned function cancels the operation.
template <typename T>
class ForeignFuture {
public:
    using Cancel = std::function<void()>;
    using Success = std::function<void(T)>;
    using Failure = std::function<void(std::exception_ptr)>;
    using Start = std::function<Cancel(Success, Failure)>;

    explicit ForeignFuture(Start start): start_(std::move(start)) {}

    ForeignFuture(const ForeignFuture &) = delete;
    ForeignFuture &operator=(const ForeignFuture &) = delete;
    ForeignFuture(ForeignFuture &&) noexcept = default;
    ForeignFuture &operator=(ForeignFuture &&) noexcept = default;

    Cancel start(Success success, Failure failure) && {
        if (!start_) {
            throw std::logic_error("UniFFI foreign future has already been started");
        }
        auto start = std::move(start_);
        return start(std::move(success), std::move(failure));
    }

private:
    Start start_;
};

template <>
class ForeignFuture<void> {
public:
    using Cancel = std::function<void()>;
    using Success = std::function<void()>;
    using Failure = std::function<void(std::exception_ptr)>;
    using Start = std::function<Cancel(Success, Failure)>;

    explicit ForeignFuture(Start start): start_(std::move(start)) {}

    ForeignFuture(const ForeignFuture &) = delete;
    ForeignFuture &operator=(const ForeignFuture &) = delete;
    ForeignFuture(ForeignFuture &&) noexcept = default;
    ForeignFuture &operator=(ForeignFuture &&) noexcept = default;

    Cancel start(Success success, Failure failure) && {
        if (!start_) {
            throw std::logic_error("UniFFI foreign future has already been started");
        }
        auto start = std::move(start_);
        return start(std::move(success), std::move(failure));
    }

private:
    Start start_;
};

template <typename T>
class Future {
public:
    Future(std::future<T> future, std::function<void()> cancel):
        future_(std::move(future)), cancel_(std::move(cancel)) {}

    Future(const Future &) = delete;
    Future &operator=(const Future &) = delete;

    Future(Future &&other) noexcept:
        future_(std::move(other.future_)), cancel_(std::move(other.cancel_)) {
        other.cancel_ = nullptr;
    }

    Future &operator=(Future &&other) noexcept {
        if (this != &other) {
            cancel();
            future_ = std::move(other.future_);
            cancel_ = std::move(other.cancel_);
            other.cancel_ = nullptr;
        }
        return *this;
    }

    ~Future() {
        cancel();
    }

    bool valid() const noexcept {
        return future_.valid();
    }

    decltype(auto) get() {
        return future_.get();
    }

    void wait() const {
        future_.wait();
    }

    template <typename Rep, typename Period>
    std::future_status wait_for(const std::chrono::duration<Rep, Period> &timeout) const {
        return future_.wait_for(timeout);
    }

    void cancel() noexcept {
        if (cancel_) {
            auto cancel = std::move(cancel_);
            cancel_ = nullptr;
            cancel();
        }
    }

private:
    std::future<T> future_;
    std::function<void()> cancel_;
};

class AsyncCancelledError: public std::runtime_error {
public:
    AsyncCancelledError(): std::runtime_error("UniFFI async call cancelled") {}
};

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

#include <test_common.hpp>

#include <uniffi_bindgen_cpp_fixtures.hpp>

#include <chrono>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

using namespace std::chrono_literals;

namespace fixtures = uniffi_bindgen_cpp_fixtures;

class TestDispatcher {
public:
    TestDispatcher(): worker_([this] { run(); }) {}

    ~TestDispatcher() {
        shutdown();
    }

    bool dispatch(uniffi::AsyncTask task) {
        std::lock_guard<std::mutex> guard(mutex_);
        if (!accepting_ || reject_) {
            return false;
        }
        tasks_.push_back(std::move(task));
        dispatch_count_.fetch_add(1);
        ready_.notify_one();
        return true;
    }

    void reject(bool value) {
        std::lock_guard<std::mutex> guard(mutex_);
        reject_ = value;
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> guard(mutex_);
            accepting_ = false;
        }
        ready_.notify_all();
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    int dispatch_count() const {
        return dispatch_count_.load();
    }

private:
    void run() {
        for (;;) {
            uniffi::AsyncTask task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                ready_.wait(lock, [this] { return !accepting_ || !tasks_.empty(); });
                if (tasks_.empty()) {
                    return;
                }
                task = std::move(tasks_.front());
                tasks_.pop_front();
            }
            task();
        }
    }

    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<uniffi::AsyncTask> tasks_;
    bool accepting_ = true;
    bool reject_ = false;
    std::atomic<int> dispatch_count_ = 0;
    std::thread worker_;
};

class TestAsyncParser final : public fixtures::AsyncParserForeign {
public:
    TestAsyncParser(
        std::shared_ptr<std::atomic<int>> cancellation_count,
        std::shared_ptr<std::atomic<int>> destruction_count
    ):
        cancellation_count_(std::move(cancellation_count)),
        destruction_count_(std::move(destruction_count)) {}

    ~TestAsyncParser() override {
        destruction_count_->fetch_add(1);
    }

    uniffi::ForeignFuture<std::string> stringify(int32_t value) override {
        return uniffi::ForeignFuture<std::string>(
            [value](auto success, auto) {
                success(std::to_string(value));
                return [] {};
            }
        );
    }

    uniffi::ForeignFuture<int32_t> parse(const std::string &value) override {
        return uniffi::ForeignFuture<int32_t>(
            [value](auto success, auto failure) {
                try {
                    success(std::stoi(value));
                } catch (...) {
                    failure(std::make_exception_ptr(
                        fixtures::async_parser_error::InvalidInteger("invalid integer")
                    ));
                }
                return [] {};
            }
        );
    }

    uniffi::ForeignFuture<void> wait() override {
        auto cancellation_count = cancellation_count_;
        return uniffi::ForeignFuture<void>(
            [cancellation_count](auto, auto) {
                return [cancellation_count] {
                    cancellation_count->fetch_add(1);
                };
            }
        );
    }

private:
    std::shared_ptr<std::atomic<int>> cancellation_count_;
    std::shared_ptr<std::atomic<int>> destruction_count_;
};

void wait_for_pending_count(uint64_t expected) {
    for (int i = 0; i < 100 && uniffi_bindgen_cpp_fixtures::async_pending_count() != expected; ++i) {
        std::this_thread::sleep_for(1ms);
    }
    ASSERT_EQ(expected, uniffi_bindgen_cpp_fixtures::async_pending_count());
}

int main() {
    auto dispatcher = std::make_shared<TestDispatcher>();
    uniffi::set_async_dispatcher(
        [dispatcher](uniffi::AsyncTask task) {
            return dispatcher->dispatch(std::move(task));
        },
        [dispatcher] { dispatcher->shutdown(); }
    );

    ASSERT_EQ("async success", uniffi_bindgen_cpp_fixtures::async_fallible(false).get());
    EXPECT_EXCEPTION(
        uniffi_bindgen_cpp_fixtures::async_fallible(true).get(),
        uniffi_bindgen_cpp_fixtures::async_test_error::Failed
    );

    {
        auto future = uniffi_bindgen_cpp_fixtures::async_pending();
        wait_for_pending_count(1);
        future.cancel();
        EXPECT_EXCEPTION(future.get(), uniffi::AsyncCancelledError);
        wait_for_pending_count(0);
    }

    {
        auto future = uniffi_bindgen_cpp_fixtures::async_pending();
        wait_for_pending_count(1);
    }
    wait_for_pending_count(0);

    auto cancellation_count = std::make_shared<std::atomic<int>>(0);
    auto destruction_count = std::make_shared<std::atomic<int>>(0);
    auto parser = std::make_shared<TestAsyncParser>(cancellation_count, destruction_count);
    ASSERT_EQ("42", fixtures::stringify_using_parser(parser, 42).get());
    ASSERT_EQ(42, fixtures::parse_using_parser(parser, "42").get());
    EXPECT_EXCEPTION(
        fixtures::parse_using_parser(parser, "not an integer").get(),
        fixtures::async_parser_error::InvalidInteger
    );

    {
        auto future = fixtures::wait_using_parser(parser);
        parser.reset();
        ASSERT_EQ(0, destruction_count->load());
        future.cancel();
        EXPECT_EXCEPTION(future.get(), uniffi::AsyncCancelledError);
    }
    for (int i = 0;
         i < 100 && (cancellation_count->load() == 0 || destruction_count->load() == 0);
         ++i) {
        std::this_thread::sleep_for(1ms);
    }
    ASSERT_EQ(1, cancellation_count->load());
    ASSERT_EQ(1, destruction_count->load());
    ASSERT_TRUE(dispatcher->dispatch_count() > 0);

    dispatcher->reject(true);
    EXPECT_EXCEPTION(
        fixtures::async_fallible(false).get(),
        uniffi::AsyncDispatcherError
    );
    dispatcher->reject(false);
    ASSERT_EQ("async success", fixtures::async_fallible(false).get());

    uniffi::shutdown_async_dispatcher();
    EXPECT_EXCEPTION(
        fixtures::async_fallible(false).get(),
        uniffi::AsyncDispatcherError
    );

    return 0;
}

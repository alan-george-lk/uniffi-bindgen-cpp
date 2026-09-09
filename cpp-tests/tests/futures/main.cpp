#include <test_common.hpp>

#include <futures.hpp>

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

int main() {
    auto inline_executor = [](uniffi::AsyncTask task) {
        task();
        return true;
    };

    ASSERT_EQ("Hello, C++", futures::greet("C++"));
    ASSERT_TRUE(futures::always_ready().get());
    futures::void_return().get();
    ASSERT_EQ("Hello, Future!", futures::say().get());
    ASSERT_EQ("Hello, Future!", futures::say_after(1, "Future").get());
    ASSERT_TRUE(futures::sleep(1).get());
    futures::sleep_no_return(1).get();

    ASSERT_EQ(42, futures::fallible_me(false).get());
    EXPECT_EXCEPTION(futures::fallible_me(true).get(), futures::my_error::Foo);

    auto megaphone = futures::new_megaphone();
    ASSERT_EQ("HELLO, WORLD!", megaphone->say_after(1, "World").get());
    ASSERT_EQ("", megaphone->silence().get());
    ASSERT_EQ(42, megaphone->fallible_me(false).get());
    EXPECT_EXCEPTION(megaphone->fallible_me(true).get(), futures::my_error::Foo);

    ASSERT_TRUE(futures::fallible_struct(false).get() != nullptr);
    EXPECT_EXCEPTION(futures::fallible_struct(true).get(), futures::my_error::Foo);
    ASSERT_TRUE(futures::async_new_megaphone().get() != nullptr);
    ASSERT_TRUE(futures::async_maybe_new_megaphone(true).get() != nullptr);
    ASSERT_TRUE(futures::async_maybe_new_megaphone(false).get() == nullptr);
    ASSERT_EQ(
        "HELLO, FRIEND!",
        futures::say_after_with_megaphone(megaphone, 1, "Friend").get()
    );

    ASSERT_TRUE(futures::Megaphone::init().get() != nullptr);
    ASSERT_TRUE(futures::Megaphone::secondary().get() != nullptr);
    EXPECT_EXCEPTION(futures::FallibleMegaphone::init().get(), futures::my_error::Foo);

    auto udl_megaphone = futures::UdlMegaphone::init().get();
    ASSERT_EQ("HELLO, UDL!", udl_megaphone->say_after(1, "Udl").get());
    ASSERT_TRUE(futures::UdlMegaphone::secondary().get() != nullptr);

    auto record = futures::new_my_record("value", 42).get();
    ASSERT_EQ("value", record.a);
    ASSERT_EQ(42, record.b);

    auto holding = futures::use_shared_resource({50, 100});
    std::this_thread::sleep_for(5ms);
    EXPECT_EXCEPTION(
        futures::use_shared_resource({0, 5}).get(),
        futures::async_error::Timeout
    );
    holding.get();

    std::promise<std::string> continued_value;
    auto continued_value_future = continued_value.get_future();
    auto value_continuation = std::move(futures::say()).then(
        inline_executor,
        [&continued_value](uniffi::FutureResult<std::string> result) {
            try {
                continued_value.set_value(std::move(result).get());
            } catch (...) {
                continued_value.set_exception(std::current_exception());
            }
        }
    );
    ASSERT_EQ("Hello, Future!", continued_value_future.get());

    std::promise<std::string> rejected_value;
    auto rejected_value_future = rejected_value.get_future();
    auto rejected_continuation = std::move(futures::say()).then(
        [](uniffi::AsyncTask) { return false; },
        [&rejected_value](uniffi::FutureResult<std::string> result) {
            try {
                rejected_value.set_value(std::move(result).get());
            } catch (...) {
                rejected_value.set_exception(std::current_exception());
            }
        }
    );
    EXPECT_EXCEPTION(rejected_value_future.get(), uniffi::AsyncDispatcherError);

    std::promise<uint8_t> continued_error;
    auto continued_error_future = continued_error.get_future();
    auto error_continuation = std::move(futures::fallible_me(true)).then(
        inline_executor,
        [&continued_error](uniffi::FutureResult<uint8_t> result) {
            try {
                continued_error.set_value(std::move(result).get());
            } catch (...) {
                continued_error.set_exception(std::current_exception());
            }
        }
    );
    EXPECT_EXCEPTION(continued_error_future.get(), futures::my_error::Foo);

    std::promise<void> continued_cancel;
    auto continued_cancel_future = continued_cancel.get_future();
    auto cancel_continuation = std::move(futures::sleep_no_return(100)).then(
        inline_executor,
        [&continued_cancel](uniffi::FutureResult<void> result) {
            try {
                result.get();
                continued_cancel.set_value();
            } catch (...) {
                continued_cancel.set_exception(std::current_exception());
            }
        }
    );
    cancel_continuation.cancel();
    EXPECT_EXCEPTION(continued_cancel_future.get(), uniffi::AsyncCancelledError);

    return 0;
}

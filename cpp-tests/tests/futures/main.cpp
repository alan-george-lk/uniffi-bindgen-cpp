#include <test_common.hpp>

#include <futures.hpp>

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

int main() {
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

    auto holding = futures::use_shared_resource({ .release_after_ms = 50, .timeout_ms = 100 });
    std::this_thread::sleep_for(5ms);
    EXPECT_EXCEPTION(
        futures::use_shared_resource({ .release_after_ms = 0, .timeout_ms = 5 }).get(),
        futures::async_error::Timeout
    );
    holding.get();

    return 0;
}

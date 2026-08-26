#include <test_common.hpp>

#include <uniffi_bindgen_cpp_fixtures.hpp>

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

void wait_for_pending_count(uint64_t expected) {
    for (int i = 0; i < 100 && uniffi_bindgen_cpp_fixtures::async_pending_count() != expected; ++i) {
        std::this_thread::sleep_for(1ms);
    }
    ASSERT_EQ(expected, uniffi_bindgen_cpp_fixtures::async_pending_count());
}

int main() {
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

    return 0;
}

#include <test_common.hpp>

#include <futures.hpp>

#include <atomic>
#include <memory>
#include <string>

namespace {

struct ExecutorState {
    explicit ExecutorState(std::shared_ptr<std::atomic<bool>> destroyed):
        destroyed(std::move(destroyed)) {}

    ~ExecutorState() {
        destroyed->store(true);
    }

    std::shared_ptr<std::atomic<bool>> destroyed;
    std::atomic<int> dispatch_count = 0;
};

void test_inline_reentrant_dispatch() {
    auto state = std::make_shared<ExecutorState>(std::make_shared<std::atomic<bool>>(false));
    uniffi::set_async_dispatcher(
        [state](uniffi::AsyncTask task) {
            state->dispatch_count.fetch_add(1);
            task();
            return true;
        },
        [] {}
    );

    bool nested_ran = false;
    ASSERT_TRUE(uniffi::detail::dispatch_async([&nested_ran] {
        ASSERT_TRUE(uniffi::detail::dispatch_async([&nested_ran] { nested_ran = true; }));
    }));
    ASSERT_TRUE(nested_ran);
    ASSERT_EQ(2, state->dispatch_count.load());
    uniffi::shutdown_async_dispatcher();
}

void test_rejected_dispatch() {
    auto shutdown_count = std::make_shared<std::atomic<int>>(0);
    uniffi::set_async_dispatcher(
        [](uniffi::AsyncTask) { return false; },
        [shutdown_count] { shutdown_count->fetch_add(1); }
    );

    ASSERT_FALSE(uniffi::detail::dispatch_async([] {}));
    uniffi::shutdown_async_dispatcher();
    uniffi::shutdown_async_dispatcher();
    ASSERT_EQ(1, shutdown_count->load());
}

void test_dispatcher_lifetime() {
    auto destroyed = std::make_shared<std::atomic<bool>>(false);
    auto state = std::make_shared<ExecutorState>(destroyed);
    std::weak_ptr<ExecutorState> weak_state = state;
    uniffi::set_async_dispatcher(
        [state](uniffi::AsyncTask task) {
            state->dispatch_count.fetch_add(1);
            task();
            return true;
        },
        [state] {}
    );
    state.reset();

    ASSERT_TRUE(futures::always_ready().get());
    ASSERT_FALSE(weak_state.expired());
    uniffi::shutdown_async_dispatcher();
    ASSERT_TRUE(weak_state.expired());
    ASSERT_TRUE(destroyed->load());
}

} // namespace

int main(int argc, char **argv) {
    ASSERT_EQ(2, argc);
    const std::string scenario(argv[1]);
    if (scenario == "inline") {
        test_inline_reentrant_dispatch();
    } else if (scenario == "rejection") {
        test_rejected_dispatch();
    } else if (scenario == "lifetime") {
        test_dispatcher_lifetime();
    } else {
        ASSERT_TRUE(false && "unknown async dispatcher scenario");
    }
    return 0;
}

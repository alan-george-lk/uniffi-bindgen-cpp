#include "logger.hpp"

int main() {

    auto logger = logger::Logger::init(logger::LogLevel::kInfo);
    logger->info("Hello, world!");
    logger->debug("Debug message"); // shouldn't print
    logger->set_level(logger::LogLevel::kDebug);
    logger->debug("Debug message"); // should print

    return 0;
}

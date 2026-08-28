c++ -std=c++20 -I examples/logger/generated \
  examples/logger/cpp/main.cpp examples/logger/generated/logger.cpp\
  -L examples/logger/target/release -luniffi_cpp_logger_example \
  -Wl,-rpath,"$PWD/examples/logger/target/release" \
  -o examples/logger/generated/logger-demo
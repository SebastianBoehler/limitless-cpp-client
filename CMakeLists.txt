cmake_minimum_required(VERSION 3.22)

project(limitless_client
    VERSION 0.1.0
    DESCRIPTION "C++20 SDK for the Limitless Exchange API"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(LIMITLESS_CLIENT_BUILD_EXAMPLES "Build Limitless examples" ON)
option(LIMITLESS_CLIENT_BUILD_TESTS "Build offline Limitless tests" ON)

find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
set(JSON_Install OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
)
FetchContent_MakeAvailable(nlohmann_json)

add_library(limitless_client
    src/sdk_error.cpp
    src/network.cpp
    src/decimal_math.cpp
    src/json_util.cpp
    src/hmac_auth.cpp
    src/keccak.cpp
    src/order_signer.cpp
    src/http_client.cpp
    src/orderbook.cpp
    src/socket_io.cpp
    src/websocket_client.cpp
    src/clob_client.cpp
    third_party/tiny_sha3/sha3.c
)
add_library(limitless::client ALIAS limitless_client)

target_include_directories(limitless_client
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tiny_sha3
)
target_link_libraries(limitless_client
    PUBLIC nlohmann_json::nlohmann_json
    PRIVATE CURL::libcurl OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB
)
target_compile_options(limitless_client PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -Wpedantic>
)
set_target_properties(limitless_client PROPERTIES
    OUTPUT_NAME limitless_client
    EXPORT_NAME client
    POSITION_INDEPENDENT_CODE ON
)

if(LIMITLESS_CLIENT_BUILD_EXAMPLES)
    add_executable(public_markets examples/public_markets.cpp)
    target_link_libraries(public_markets PRIVATE limitless::client)
    target_compile_options(public_markets PRIVATE -Wall -Wextra -Wpedantic)

    add_executable(sign_example examples/sign_example.cpp)
    target_link_libraries(sign_example PRIVATE limitless::client)
    target_compile_options(sign_example PRIVATE -Wall -Wextra -Wpedantic)

    add_executable(ws_example examples/ws_example.cpp)
    target_link_libraries(ws_example PRIVATE limitless::client)
    target_compile_options(ws_example PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(LIMITLESS_CLIENT_BUILD_TESTS)
    enable_testing()
    add_executable(limitless_offline_tests tests/offline_tests.cpp)
    target_link_libraries(limitless_offline_tests PRIVATE limitless::client)
    target_compile_options(limitless_offline_tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME offline COMMAND limitless_offline_tests)
endif()

include(GNUInstallDirs)
install(TARGETS limitless_client
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/limitless DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

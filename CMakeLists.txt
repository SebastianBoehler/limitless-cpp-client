cmake_minimum_required(VERSION 3.22)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

project(limitless_cpp_client VERSION 0.1.0 LANGUAGES CXX C)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    endif()
endif()

set(LIMITLESS_CLIENT_TOP_LEVEL_DEFAULT OFF)
if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
    set(LIMITLESS_CLIENT_TOP_LEVEL_DEFAULT ON)
endif()

option(LIMITLESS_CLIENT_BUILD_EXAMPLES "Build the public markets example" ${LIMITLESS_CLIENT_TOP_LEVEL_DEFAULT})
option(LIMITLESS_CLIENT_BUILD_TESTS "Build unit tests" ${LIMITLESS_CLIENT_TOP_LEVEL_DEFAULT})
unset(LIMITLESS_CLIENT_TOP_LEVEL_DEFAULT)

include(FetchContent)

FetchContent_Declare(
    json
    URL https://codeload.github.com/nlohmann/json/tar.gz/55f93686c01528224f448c19128836e7df245f72
    URL_HASH SHA256=67f4cdd9ca930c9c1e130af4a437c7fc98fab77a2846fc2d2a14b4943831f8ef
)

FetchContent_Declare(
    ixwebsocket
    URL https://codeload.github.com/machinezone/IXWebSocket/tar.gz/64fae7676bd8fe31f7cb4bcde7a6841892dad65e
    URL_HASH SHA256=af641277dd79c8f0970a31b84beec223d8c683eb8a671b0cce20bd8a8f750878
)

FetchContent_Declare(
    secp256k1
    URL https://codeload.github.com/bitcoin-core/secp256k1/tar.gz/1a53f4961f337b4d166c25fce72ef0dc88806618
    URL_HASH SHA256=b51329bdd3b6c955380f4c4ca236d07dd8e109d4e869a4758be4602d8ed5fe31
)

FetchContent_Declare(
    ethash
    URL https://codeload.github.com/chfast/ethash/tar.gz/83bd5ad51acf7d7fdba215347cc805d51a194d2b
    URL_HASH SHA256=9c2b342411d6c68983a6b1cae752cf4b65d105b582b9e3f277f5112c9b76c90f
)

find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

set(USE_TLS ON)
set(USE_OPEN_SSL ON)
set(IXWEBSOCKET_INSTALL OFF)
set(SECP256K1_ENABLE_MODULE_RECOVERY ON)
set(SECP256K1_BUILD_TESTS OFF)
set(SECP256K1_BUILD_EXHAUSTIVE_TESTS OFF)
set(SECP256K1_BUILD_BENCHMARK OFF)
set(ETHASH_BUILD_ETHASH OFF)
set(OPENSSL_LIBRARIES OpenSSL::SSL OpenSSL::Crypto)

FetchContent_MakeAvailable(json ixwebsocket secp256k1 ethash)

set(LIMITLESS_CLIENT_SOURCES
    src/auth.cpp
    src/clob_client.cpp
    src/decimal_math.cpp
    src/environment.cpp
    src/http_client.cpp
    src/order_signer.cpp
    src/orderbook.cpp
    src/position_client.cpp
    src/rest_session.cpp
    src/sdk_error.cpp
    src/types.cpp
    src/user_stream.cpp
    src/websocket_client.cpp
)

add_library(limitless_client ${LIMITLESS_CLIENT_SOURCES})
add_library(limitless::client ALIAS limitless_client)
set_target_properties(limitless_client PROPERTIES
    OUTPUT_NAME limitless_client
    EXPORT_NAME client
    POSITION_INDEPENDENT_CODE ON
)
target_compile_features(limitless_client PUBLIC cxx_std_20)
target_compile_options(limitless_client PRIVATE -Wall -Wextra -Wpedantic)

target_include_directories(limitless_client
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(limitless_client
    PUBLIC
        nlohmann_json::nlohmann_json
    PRIVATE
        CURL::libcurl
        OpenSSL::SSL
        OpenSSL::Crypto
        Threads::Threads
        ixwebsocket
        secp256k1
        keccak
)

if(LIMITLESS_CLIENT_BUILD_EXAMPLES)
    add_executable(limitless_markets_orderbook examples/markets_orderbook.cpp)
    target_link_libraries(limitless_markets_orderbook PRIVATE limitless::client)
    target_compile_options(limitless_markets_orderbook PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(LIMITLESS_CLIENT_BUILD_TESTS)
    enable_testing()
    function(limitless_add_test name)
        add_executable(${name} tests/${name}.cpp)
        target_link_libraries(${name} PRIVATE limitless::client)
        target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
        add_test(NAME ${name} COMMAND ${name})
    endfunction()
    limitless_add_test(test_decimal)
    limitless_add_test(test_auth)
    limitless_add_test(test_signer)
    limitless_add_test(test_orderbook)
    limitless_add_test(test_socketio)
    limitless_add_test(test_prepare_order)
endif()

install(TARGETS limitless_client
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)
install(DIRECTORY include/limitless DESTINATION include)
install(FILES LICENSE DESTINATION share/licenses/limitless_client)

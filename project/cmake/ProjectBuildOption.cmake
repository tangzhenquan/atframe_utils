﻿if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.10")
    include_guard(GLOBAL)
endif()

# 功能检测选项
include(WriteCompilerDetectionHeader)

# generate check header
write_compiler_detection_header(
    FILE "${PROJECT_ALL_INCLUDE_DIR}/config/compiler_features.h"
    PREFIX UTIL_CONFIG
    COMPILERS GNU Clang AppleClang MSVC
    FEATURES cxx_auto_type cxx_constexpr cxx_decltype cxx_decltype_auto cxx_defaulted_functions cxx_deleted_functions cxx_final cxx_override cxx_range_for cxx_noexcept cxx_nullptr cxx_rvalue_references cxx_static_assert cxx_thread_local cxx_variadic_templates cxx_lambdas
)
#file(MAKE_DIRECTORY "${PROJECT_ALL_INCLUDE_DIR}/config")
#file(RENAME "${CMAKE_BINARY_DIR}/compiler_features.h" "${PROJECT_ALL_INCLUDE_DIR}/config/compiler_features.h")

# 默认配置选项
#####################################################################
option(LIBUNWIND_ENABLED "Enable using libunwind." OFF)
option(LOG_WRAPPER_ENABLE_LUA_SUPPORT "Enable lua support." ON)
option(LOG_WRAPPER_CHECK_LUA "Check lua support." ON)
set(LOG_WRAPPER_MAX_SIZE_PER_LINE "2097152" CACHE STRING "Max size in one log line.")
set(LOG_WRAPPER_CATEGORIZE_SIZE "16" CACHE STRING "Default log categorize number.")

find_package(Threads)
if (CMAKE_USE_PTHREADS_INIT)
    set(THREAD_TLS_USE_PTHREAD 1)
    if(NOT ANDROID)
        list(APPEND COMPILER_OPTION_EXTERN_CXX_LIBS pthread)
    endif ()
    if (THREADS_PREFER_PTHREAD_FLAG)
        add_definitions(${THREADS_PREFER_PTHREAD_FLAG})
    endif ()
endif ()

if (ANDROID)
    # Android发现偶现_Unwind_Backtrace调用崩溃,默认金庸掉这个功能。
    # 可以用adb logcat | ./ndk-stack -sym $PROJECT_PATH/obj/local/armeabi 代替
    option(LOG_WRAPPER_ENABLE_STACKTRACE "Try to enable stacktrace for log." OFF)
else()
    option(LOG_WRAPPER_ENABLE_STACKTRACE "Try to enable stacktrace for log." ON)
endif()
option(LOCK_DISABLE_MT "Disable multi-thread support lua support." OFF)
set(LOG_STACKTRACE_MAX_STACKS "100" CACHE STRING "Max stacks when stacktracing.")

include("${PROJECT_ALL_SOURCE_DIR}/log/log_configure.cmake")

# 内存混淆int
set(ENABLE_MIXEDINT_MAGIC_MASK 0 CACHE STRING "Integer mixed magic mask")
if (NOT ENABLE_MIXEDINT_MAGIC_MASK)
    set(ENABLE_MIXEDINT_MAGIC_MASK 0)
endif()

# libuv
option(ENABLE_NETWORK "Enable network support." ON)
if (ENABLE_NETWORK)
    find_package(Libuv)
    if(Libuv_FOUND)
        message(STATUS "Libuv support enabled")
        set(NETWORK_EVPOLL_ENABLE_LIBUV 1)
        include_directories(${Libuv_INCLUDE_DIRS})
    else()
        message(STATUS "Libuv support disabled")
    endif()

    # curl
    find_package(CURL)
    if(CURL_FOUND AND (TARGET CURL::libcurl OR CURL_LIBRARIES))
        message(STATUS "Curl support enabled")
        set(NETWORK_ENABLE_CURL 1)
        include_directories(${CURL_INCLUDE_DIRS})
        if (TARGET CURL::libcurl)
            list(APPEND ATFRAME_UTILS_NETWORK_LINK_NAME CURL::libcurl)
        else()
            list(APPEND ATFRAME_UTILS_NETWORK_LINK_NAME ${CURL_LIBRARIES})
        endif()
    else()
        message(STATUS "Curl support disabled")
    endif()

    if(Libuv_FOUND)
        if (TARGET unofficial::libuv::libuv)
            list(APPEND ATFRAME_UTILS_NETWORK_LINK_NAME unofficial::libuv::libuv)
        else ()
            list(APPEND ATFRAME_UTILS_NETWORK_LINK_NAME ${Libuv_LIBRARIES})
        endif()
    endif()
endif()

# openssl
option(CRYPTO_DISABLED "Disable crypto module if not specify crypto lib." OFF)
if (CRYPTO_USE_OPENSSL OR CRYPTO_USE_LIBRESSL OR CRYPTO_USE_BORINGSSL)
    find_package(OpenSSL)
    if (OPENSSL_FOUND)
    message(STATUS "Crypto enabled.(openssl found)")
        include_directories(BEFORE SYSTEM ${OPENSSL_INCLUDE_DIR})
    else()
        message(FATAL_ERROR "CRYPTO_USE_OPENSSL,CRYPTO_USE_LIBRESSL,CRYPTO_USE_BORINGSSL is set but openssl not found")
    endif()
elseif (CRYPTO_USE_MBEDTLS)
    find_package(MbedTLS)
    if (MBEDTLS_FOUND)
        message(STATUS "Crypto enabled.(mbedtls found)")
        include_directories(BEFORE SYSTEM ${MbedTLS_INCLUDE_DIRS})
    else()
        message(FATAL_ERROR "CRYPTO_USE_MBEDTLS is set but mbedtls not found")
    endif()
elseif (NOT CRYPTO_DISABLED)
    # try to find openssl or mbedtls
    find_package(OpenSSL)
    if (OPENSSL_FOUND)
        message(STATUS "Crypto enabled.(openssl found)")
        set(CRYPTO_USE_OPENSSL 1)
        include_directories(BEFORE SYSTEM ${OPENSSL_INCLUDE_DIR})
    else ()
        find_package(MbedTLS)
        if (MBEDTLS_FOUND) 
            message(STATUS "Crypto enabled.(mbedtls found)")
            set(CRYPTO_USE_MBEDTLS 1)
            include_directories(BEFORE SYSTEM ${MbedTLS_INCLUDE_DIRS})
        endif()
    endif()
endif()

if (OPENSSL_FOUND)
    if ((TARGET OpenSSL::SSL) OR (TARGET OpenSSL::Crypto))
        if (TARGET OpenSSL::SSL)
            list(APPEND ATFRAME_UTILS_BIN_CRYPT_LINK_NAME OpenSSL::SSL)
        endif ()
        if (TARGET OpenSSL::Crypto)
            list(APPEND ATFRAME_UTILS_BIN_CRYPT_LINK_NAME OpenSSL::Crypto)
        endif ()
    else ()
        list(APPEND ATFRAME_UTILS_BIN_CRYPT_LINK_NAME ${OPENSSL_CRYPTO_LIBRARY})
    endif()
elseif (MBEDTLS_FOUND)
    list(APPEND ATFRAME_UTILS_BIN_CRYPT_LINK_NAME ${MbedTLS_CRYPTO_LIBRARIES})
endif()

if (NOT CRYPTO_DISABLED)
    find_package(Libsodium)
    if(LIBSODIUM_FOUND)
        set(CRYPTO_USE_LIBSODIUM 1)
        include_directories(${Libsodium_INCLUDE_DIRS})
        list(APPEND ATFRAME_UTILS_BIN_CRYPT_LINK_NAME ${Libsodium_LIBRARIES})
    endif()
endif()

# 测试配置选项
set(GTEST_ROOT "" CACHE STRING "GTest root directory")
set(BOOST_ROOT "" CACHE STRING "Boost root directory")
option(PROJECT_TEST_ENABLE_BOOST_UNIT_TEST "Enable boost unit test." OFF)

option(PROJECT_ENABLE_UNITTEST "Enable unit test" OFF)
option(PROJECT_ENABLE_SAMPLE "Enable sample" OFF)

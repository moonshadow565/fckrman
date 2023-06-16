cmake_minimum_required(VERSION 3.8)

project(fckrman LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


include(FetchContent)

find_package (Threads REQUIRED)
find_package (CURL)

if (NOT CURL_FOUND)
  set(BUILD_CURL_EXE OFF CACHE INTERNAL "Dont need")
  set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "Dont need")
  set(BUILD_TESTING OFF CACHE INTERNAL "Dont need")
  set(CURL_DISABLE_DICT ON CACHE INTERNAL "Dont need")
  set(CURL_DISABLE_GOPHER ON CACHE INTERNAL "Dont need")
  set(CURL_DISABLE_IMAP ON CACHE INTERNAL "Dont need")
  set(CURL_DISABLE_LDAP ON CACHE INTERNAL "Dont need")
  set(CURL_DISABLE_LDAPS ON CACHE INTERNAL "Dont need")
  set(CURL_DISABLE_MQTT ON CACHE INTERNAL "Dont need")
  set(CURL_DISABLE_POP3 ON CACHE INTERNAL "Dont need")
  set(CURL_DISABLE_RTSP ON CACHE INTERNAL "Dont need")
  set(CURL_DISABLE_SMTP ON CACHE INTERNAL "Dont need")
  set(CURL_DISABLE_TELNET ON CACHE INTERNAL "Dont need")
  set(CURL_DISABLE_TFTP ON CACHE INTERNAL "Dont need")
  if (WIN32)
    set(CURL_USE_SCHANNEL ON CACHE INTERNAL "Native windows ssl")
  endif()
  FetchContent_Declare(
    CURL
    GIT_REPOSITORY https://github.com/curl/curl.git
    GIT_TAG        curl-7_88_1
  )
  FetchContent_MakeAvailable(CURL)
endif()

# argparse is header only library, just include it
FetchContent_Declare(
    argparse
    GIT_REPOSITORY https://github.com/p-ranav/argparse.git
    GIT_TAG        v2.5
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
)
FetchContent_GetProperties(argparse)
if(NOT argparse_POPULATED)
    FetchContent_Populate(argparse)
    add_library(argparse INTERFACE)
    target_include_directories(argparse INTERFACE ${argparse_SOURCE_DIR}/include/argparse)
endif()

# zstd has to be built manually since it doesn't have cmake support
FetchContent_Declare(
    zstd
    GIT_REPOSITORY https://github.com/facebook/zstd.git
    GIT_TAG        v1.5.2
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
)
FetchContent_GetProperties(zstd)
if(NOT zstd_POPULATED)
    FetchContent_Populate(zstd)
    file(GLOB zstd_SRCS
        ${zstd_SOURCE_DIR}/lib/common/*.c
        ${zstd_SOURCE_DIR}/lib/compress/*.c
        ${zstd_SOURCE_DIR}/lib/decompress/*.c
    )
    add_library(zstd STATIC ${zstd_SRCS})
    target_include_directories(zstd PUBLIC ${zstd_SOURCE_DIR}/lib)
    target_include_directories(zstd PRIVATE ${zstd_SOURCE_DIR}/lib/common)
    target_compile_definitions(zstd PRIVATE -DZSTD_DISABLE_ASM)
    target_compile_definitions(zstd PUBLIC -DXXH_STATIC_LINKING_ONLY)
    target_compile_definitions(zstd INTERFACE -DZSTD_STATIC_LINKING_ONLY)
    if(MSVC)
        target_compile_options(zstd PRIVATE /wd4267)
    endif()
endif()

add_executable(fckrman
    include/fltbf.hpp
    include/json.hpp
    include/sha2.hpp
    src/cli.hpp
    src/cli.cpp
    src/download_opts.hpp
    src/download.hpp
    src/download.cpp
    src/file.hpp
    src/file.cpp
    src/error.hpp
    src/error.cpp
    src/manifest.hpp
    src/manifest.cpp
    src/main.cpp
)
target_link_libraries(fckrman PRIVATE argparse zstd Threads::Threads)
target_include_directories(fckrman PRIVATE include/)
target_link_libraries(fckrman PRIVATE CURL::libcurl)

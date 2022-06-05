cmake_minimum_required(VERSION 3.8)

project(fckrman LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package (Threads REQUIRED)

find_package (CURL)
if (NOT CURL_FOUND)
  include(FetchContent)
  set(BUILD_CURL_EXE OFF CACHE INTERNAL "Dont need")
  set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "Dont need")
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
    GIT_TAG        curl-7_83_1
  )
  FetchContent_MakeAvailable(CURL)
endif()

add_subdirectory(zstd)
add_executable(fckrman
    include/argparse.hpp
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
target_link_libraries(fckrman PRIVATE zstd Threads::Threads)
target_include_directories(fckrman PRIVATE include/)
target_link_libraries(fckrman PRIVATE CURL::libcurl)

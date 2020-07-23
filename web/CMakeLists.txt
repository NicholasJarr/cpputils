find_package(CURL REQUIRED)

add_library(web src/curl.cpp src/ftp.cpp)

target_include_directories(web PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(web PUBLIC CURL::libcurl)
target_compile_features(web PUBLIC cxx_std_17)
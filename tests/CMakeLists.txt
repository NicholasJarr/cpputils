
find_package(Catch2 REQUIRED)

add_executable(tests src/main.cpp)

list(APPEND TEST_LIBS Catch2::Catch2)

if (BUILD_WEB)
    list(APPEND TEST_LIBS cpputils::web)
endif()
if (BUILD_IOTHUB)
    list(APPEND TEST_LIBS cpputils::iothub)
endif()

target_link_libraries(tests PRIVATE ${TEST_LIBS})
target_compile_features(tests PUBLIC cxx_std_17)
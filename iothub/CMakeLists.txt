find_package(azure_iot_sdks REQUIRED)
find_package(nlohmann_json REQUIRED)

add_library(iothub INTERFACE)

set(AZUREIOT_INC_FOLDER "/usr/include/azureiot" "/usr/include/azureiot/inc")

target_include_directories(iothub INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include ${AZUREIOT_INC_FOLDER})
target_link_libraries(iothub INTERFACE
                             nlohmann_json
                             nlohmann_json::nlohmann_json
                             serializer
                             iothub_client
                             prov_auth_client
                             hsm_security_client)
target_compile_features(iothub INTERFACE cxx_std_17)
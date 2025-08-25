file(GLOB_RECURSE SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.S"
    "/opt/esp-idf/examples/zigbee/common/zcl_utility/src/*.c"
)

set(ZCL_UTIL_PATH "/opt/esp-idf/examples/zigbee/common/zcl_utility")

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "." "${ZCL_UTIL_PATH}/include"
)

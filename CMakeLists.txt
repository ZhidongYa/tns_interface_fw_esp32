# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS 
			 ${PROJECT_PATH}/components/sensor
			 ${PROJECT_PATH}/components/websocket
			 ${PROJECT_PATH}/components/protocol_examples_common
			)
set(EXCLUDE_COMPONENTS "coap" "asio")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(olimex_webserver)

target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "components/server/ipr.ico" BINARY)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "components/server/root.html" BINARY)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "components/server/error.html" BINARY)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "components/server/echarts.min.js" BINARY)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "components/server/style.css" BINARY)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "components/server/mychart.js" BINARY)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "components/server/websocket.js" BINARY)


set(COMPONENT_SRCS  "webserver.c"
                    "websocket.c"
                    "websocket_server.c")

set(COMPONENT_ADD_INCLUDEDIRS "include")

set(COMPONENT_REQUIRES lwip mbedtls sensor)

register_component()

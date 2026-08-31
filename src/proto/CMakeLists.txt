set(SRC_GRP_PROTO
    ClientConnection.cpp
    ClientConnection.h
    IClientLink.h
    IWorldGateway.h
    Listener.cpp
    Listener.h
    Opcodes.h
    WorldPacket.h
    PacketCodec.cpp
    PacketCodec.h
)
source_group("proto" FILES ${SRC_GRP_PROTO})

add_library(proto STATIC ${SRC_GRP_PROTO})

target_include_directories(proto
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(proto
    PUBLIC
        shared
    PRIVATE
        mangos_openssl_strict
)

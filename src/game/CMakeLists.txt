file(GLOB SRC_GRP_BATTLEGROUND BattleGround/*.cpp BattleGround/*.h)
source_group("BattleGround" FILES ${SRC_GRP_BATTLEGROUND})

file(GLOB SRC_GRP_CHAT_COMMANDS ChatCommands/*.cpp ChatCommands/*.h)
source_group("Chat Commands" FILES ${SRC_GRP_CHAT_COMMANDS})

file(GLOB SRC_GRP_COMBAT Combat/*.cpp Combat/*.h)
source_group("Combat" FILES ${SRC_GRP_COMBAT})

file(GLOB SRC_GRP_SYNTHETIC Synthetic/*.cpp Synthetic/*.h)
source_group("Synthetic" FILES ${SRC_GRP_SYNTHETIC})

file(GLOB SRC_GRP_METRICS Metrics/*.cpp Metrics/*.h)
source_group("Metrics" FILES ${SRC_GRP_METRICS})

file(GLOB SRC_GRP_MAP_THREAD Maps/*.cpp Maps/*.h)
source_group("Map Threading" FILES ${SRC_GRP_MAP_THREAD})

file(GLOB SRC_GRP_MOTION_GEN MotionGenerators/*.cpp MotionGenerators/*.h)
source_group("Motion generators" FILES ${SRC_GRP_MOTION_GEN})

file(GLOB SRC_GRP_MOVEMENT movement/*.cpp movement/*.h)
source_group("Movement" FILES ${SRC_GRP_MOVEMENT})

file(GLOB SRC_GRP_OBJECT Object/*.cpp Object/*.h)
source_group("Object" FILES ${SRC_GRP_OBJECT})

file(GLOB SRC_GRP_OUTDOOR_PVP OutdoorPvP/*.cpp OutdoorPvP/*.h)
source_group("Outdoor PvP" FILES ${SRC_GRP_OUTDOOR_PVP})

file(GLOB SRC_GRP_PROTOCOL Protocol/*.cpp Protocol/*.h)
source_group("Protocol" FILES ${SRC_GRP_PROTOCOL})

file(GLOB SRC_GRP_REFERENCES References/*.cpp References/*.h)
source_group("References" FILES ${SRC_GRP_REFERENCES})

file(GLOB SRC_GRP_SERVER Server/*.cpp Server/*.h)
source_group("Server" FILES ${SRC_GRP_SERVER})

file(GLOB SRC_GRP_TIME Time/*.cpp Time/*.h)
source_group("Time" FILES ${SRC_GRP_TIME})

file(GLOB SRC_GRP_TOOL Tools/*.cpp Tools/*.h)
source_group("Tool" FILES ${SRC_GRP_TOOL})

file(GLOB SRC_GRP_WORLD_HANDLERS WorldHandlers/*.cpp WorldHandlers/*.h)
source_group("World\\Handlers" FILES ${SRC_GRP_WORLD_HANDLERS})

add_library(game STATIC
    ${SRC_GRP_BATTLEGROUND}
    ${SRC_GRP_CHAT_COMMANDS}
    ${SRC_GRP_COMBAT}
    ${SRC_GRP_METRICS}
    ${SRC_GRP_SYNTHETIC}
    ${SRC_GRP_MAP_THREAD}
    ${SRC_GRP_MOTION_GEN}
    ${SRC_GRP_MOVEMENT}
    ${SRC_GRP_OBJECT}
    ${SRC_GRP_OUTDOOR_PVP}
    ${SRC_GRP_PROTOCOL}
    ${SRC_GRP_REFERENCES}
    ${SRC_GRP_SERVER}
    ${SRC_GRP_TIME}
    ${SRC_GRP_TOOL}
    ${SRC_GRP_WORLD_HANDLERS}
)

target_include_directories(game
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        BattleGround
        ChatCommands
        Combat
        Metrics
        Synthetic
        Maps
        MotionGenerators
        Object
        OutdoorPvP
        Protocol
        References
        Server
        Time
        Tools
        WorldHandlers
)

target_compile_definitions(game
    PUBLIC
        $<$<BOOL:${SCRIPT_LIB_SD3}>:ENABLE_SD3>
)

target_link_libraries(game
    PUBLIC

        terrain
        geometry
        shared
        RecastNavigation::Detour
        ZLIB::ZLIB
        $<$<BOOL:${SCRIPT_LIB_SD3}>:mangosscript>
    PRIVATE
        mangos_openssl_strict
)

if(PCH)
    add_cxx_pch(game pchdef.h pchdef.cpp)
endif()

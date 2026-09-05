macro(game_group VAR FOLDER)
    file(GLOB ${VAR} ${FOLDER}/*.cpp ${FOLDER}/*.h)
    string(REPLACE "/" "\\" _display "${FOLDER}")
    source_group("${_display}" FILES ${${VAR}})
endmacro()

game_group(SRC_OBJECT           Object)
game_group(SRC_UPDATEFIELDS     UpdateFields)
game_group(SRC_VISIBILITY       Visibility)

game_group(SRC_UNIT             Unit)
game_group(SRC_AURAS            Unit/Auras)
game_group(SRC_PLAYER           Player)
game_group(SRC_PLAYER_INVENTORY Player/Inventory)
game_group(SRC_PLAYER_HONOR     Player/Honor)
game_group(SRC_PLAYER_HONOR     Player/Honor)
game_group(SRC_PLAYER_JOURNAL   Player/Journal)
game_group(SRC_PLAYER_TALENTS   Player/Talents)
game_group(SRC_CREATURE         Creature)
game_group(SRC_CREATURE_AI      Creature/AI)
game_group(SRC_GAMEOBJECT       GameObject)
game_group(SRC_ITEM             Item)
game_group(SRC_CORPSE           Corpse)

game_group(SRC_COMBAT           Combat)
game_group(SRC_CAST             Cast)
game_group(SRC_CAST_EFFECTS     Cast/Effects)
game_group(SRC_LOOT             Loot)

game_group(SRC_MAP              Map)
game_group(SRC_MAP_GRID         Map/Grid)
game_group(SRC_MAP_COLLISION    Map/Collision)
game_group(SRC_MAP_TRANSPORT    Map/Transport)
game_group(SRC_MOVEMENT         Movement)
game_group(SRC_MOVE_GENERATORS  Movement/Generators)
game_group(SRC_MOVE_NAVIGATION  Movement/Navigation)
game_group(SRC_MOVE_SPLINE      Movement/Spline)

game_group(SRC_PACKET           Packet)
game_group(SRC_SESSION          Session)
game_group(SRC_HANDLERS         Session/Handlers)
game_group(SRC_CHAT             Chat)
game_group(SRC_CHAT_COMMANDS    Chat/Commands)

game_group(SRC_WORLD            World)
game_group(SRC_WORLD_GROUP      World/Group)
game_group(SRC_WORLD_GUILD      World/Guild)
game_group(SRC_WORLD_CHANNEL    World/Channel)
game_group(SRC_WORLD_MAIL       World/Mail)
game_group(SRC_WORLD_AUCTION    World/Auction)
game_group(SRC_WORLD_LFG        World/LFG)
game_group(SRC_WORLD_SOCIAL     World/Social)
game_group(SRC_BATTLEGROUND     BattleGround)
game_group(SRC_OUTDOOR_PVP      OutdoorPvP)

game_group(SRC_DATASTORE        DataStore)

game_group(SRC_SCRIPT           Script)
game_group(SRC_CLOCK            Clock)
game_group(SRC_METRICS          Metrics)
game_group(SRC_SYNTHETIC        Synthetic)
game_group(SRC_TOOLS            Tools)

add_library(game STATIC
    ${SRC_OBJECT}
    ${SRC_UPDATEFIELDS}
    ${SRC_VISIBILITY}
    ${SRC_UNIT}
    ${SRC_AURAS}
    ${SRC_PLAYER}
    ${SRC_PLAYER_INVENTORY}
    ${SRC_PLAYER_HONOR}
    ${SRC_PLAYER_HONOR}
    ${SRC_PLAYER_JOURNAL}
    ${SRC_PLAYER_TALENTS}
    ${SRC_CREATURE}
    ${SRC_CREATURE_AI}
    ${SRC_GAMEOBJECT}
    ${SRC_ITEM}
    ${SRC_CORPSE}
    ${SRC_COMBAT}
    ${SRC_CAST}
    ${SRC_CAST_EFFECTS}
    ${SRC_LOOT}
    ${SRC_MAP}
    ${SRC_MAP_GRID}
    ${SRC_MAP_COLLISION}
    ${SRC_MAP_TRANSPORT}
    ${SRC_MOVEMENT}
    ${SRC_MOVE_GENERATORS}
    ${SRC_MOVE_NAVIGATION}
    ${SRC_MOVE_SPLINE}
    ${SRC_PACKET}
    ${SRC_SESSION}
    ${SRC_HANDLERS}
    ${SRC_CHAT}
    ${SRC_CHAT_COMMANDS}
    ${SRC_WORLD}
    ${SRC_WORLD_GROUP}
    ${SRC_WORLD_GUILD}
    ${SRC_WORLD_CHANNEL}
    ${SRC_WORLD_MAIL}
    ${SRC_WORLD_AUCTION}
    ${SRC_WORLD_LFG}
    ${SRC_WORLD_SOCIAL}
    ${SRC_BATTLEGROUND}
    ${SRC_OUTDOOR_PVP}
    ${SRC_DATASTORE}
    ${SRC_SCRIPT}
    ${SRC_CLOCK}
    ${SRC_METRICS}
    ${SRC_SYNTHETIC}
    ${SRC_TOOLS}
)

target_include_directories(game
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        Object
        UpdateFields
        Visibility
        Unit
        Unit/Auras
        Player
        Player/Honor
        Player/Honor
        Player/Inventory
        Player/Journal
        Creature
        Creature/AI
        GameObject
        Item
        Corpse
        Combat
        Cast
        Cast/Effects
        Loot
        Map
        Map/Grid
        Map/Collision
        Map/Transport
        Movement
        Movement/Generators
        Movement/Navigation
        Movement/Spline
        Packet
        Session
        Session/Handlers
        Chat
        World
        World/Group
        World/Guild
        World/Channel
        World/Mail
        World/Auction
        World/LFG
        World/Social
        BattleGround
        OutdoorPvP
        DataStore
        Script
        Clock
        Metrics
        Synthetic
        Tools
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

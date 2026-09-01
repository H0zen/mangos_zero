set(mangosscript_PCH "${CMAKE_CURRENT_SOURCE_DIR}/pch.h")
if(PCH)
    LIST(APPEND mangosscript_PCH "${CMAKE_CURRENT_SOURCE_DIR}/pch.cpp")
endif()

set(mangosscript_LIB_SRCS ${mangosscript_PCH})

file(GLOB sources_include include/*.cpp include/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_include})
    source_group("Include Files" FILES ${sources_include})

file(GLOB sources_system system/*.cpp system/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_system})
    source_group("System Files" FILES ${sources_system})

file(GLOB sources_base base/*.cpp base/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_base})
    source_group("Base Files" FILES ${sources_base})

file(GLOB sources_battlegrounds scripts/battlegrounds/*.cpp scripts/battlegrounds/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_battlegrounds})
    source_group("Battleground Scripts" FILES ${sources_battlegrounds})

file(GLOB sources_world scripts/world/*.cpp scripts/world/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_world})
    source_group("World Scripts" FILES ${sources_world})

file(GLOB sources_eastern_kingdoms scripts/eastern_kingdoms/*.cpp scripts/eastern_kingdoms/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_eastern_kingdoms})
    source_group("Eastern Kingdoms Scripts" FILES ${sources_eastern_kingdoms})

file(GLOB sources_kalimdor scripts/kalimdor/*.cpp scripts/kalimdor/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_kalimdor})
    source_group("Kalimdor Scripts" FILES ${sources_kalimdor})

file(GLOB sources_instance_ek_brd scripts/eastern_kingdoms/blackrock_mountain/blackrock_depths/*.cpp scripts/eastern_kingdoms/blackrock_mountain/blackrock_depths/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_ek_brd})
    source_group("Eastern Kingdoms Scripts\\Instances\\Blackrock Depths" FILES ${sources_instance_ek_brd})

file(GLOB sources_instance_ek_brs scripts/eastern_kingdoms/blackrock_mountain/blackrock_spire/*.cpp scripts/eastern_kingdoms/blackrock_mountain/blackrock_spire/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_ek_brs})
    source_group("Eastern Kingdoms Scripts\\Instances\\Blackrock Spire" FILES ${sources_instance_ek_brs})

file(GLOB sources_instance_ek_tdm scripts/eastern_kingdoms/deadmines/*.cpp scripts/eastern_kingdoms/deadmines/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_ek_tdm})
    source_group("Eastern Kingdoms Scripts\\Instances\\Deadmines" FILES ${sources_instance_ek_tdm})

file(GLOB sources_instance_ek_gno scripts/eastern_kingdoms/gnomeregan/*.cpp scripts/eastern_kingdoms/gnomeregan/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_ek_gno})
    source_group("Eastern Kingdoms Scripts\\Instances\\Gnomeregan" FILES ${sources_instance_ek_gno})

file(GLOB sources_instance_ek_sm scripts/eastern_kingdoms/scarlet_monastery/*.cpp scripts/eastern_kingdoms/scarlet_monastery/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_ek_sm})
    source_group("Eastern Kingdoms Scripts\\Instances\\Scarlet Monastery" FILES ${sources_instance_ek_sm})

file(GLOB sources_instance_ek_scholo scripts/eastern_kingdoms/scholomance/*.cpp scripts/eastern_kingdoms/scholomance/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_ek_scholo})
    source_group("Eastern Kingdoms Scripts\\Instances\\Scholomance" FILES ${sources_instance_ek_scholo})

file(GLOB sources_instance_ek_uld scripts/eastern_kingdoms/uldaman/*.cpp scripts/eastern_kingdoms/uldaman/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_ek_uld})
    source_group("Eastern Kingdoms Scripts\\Instances\\Uldaman" FILES ${sources_instance_ek_uld})

file(GLOB sources_instance_ek_sfk scripts/eastern_kingdoms/shadowfang_keep/*.cpp scripts/eastern_kingdoms/shadowfang_keep/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_ek_sfk})
    source_group("Eastern Kingdoms Scripts\\Instances\\Shadowfang Keep" FILES ${sources_instance_ek_sfk})

file(GLOB sources_instance_ek_st scripts/eastern_kingdoms/sunken_temple/*.cpp scripts/eastern_kingdoms/sunken_temple/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_ek_st})
    source_group("Eastern Kingdoms Scripts\\Instances\\Sunken Temple" FILES ${sources_instance_ek_st})

file(GLOB sources_instance_ek_strat scripts/eastern_kingdoms/stratholme/*.cpp scripts/eastern_kingdoms/stratholme/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_ek_strat})
    source_group("Eastern Kingdoms Scripts\\Instances\\Stratholme" FILES ${sources_instance_ek_strat})

file(GLOB sources_raid_ek_mc scripts/eastern_kingdoms/blackrock_mountain/molten_core/*.cpp scripts/eastern_kingdoms/blackrock_mountain/molten_core/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_raid_ek_mc})
    source_group("Eastern Kingdoms Scripts\\Raids\\Molten Core" FILES ${sources_raid_ek_mc})

file(GLOB sources_raid_ek_bwl scripts/eastern_kingdoms/blackrock_mountain/blackwing_lair/*.cpp scripts/eastern_kingdoms/blackrock_mountain/blackwing_lair/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_raid_ek_bwl})
    source_group("Eastern Kingdoms Scripts\\Raids\\Blackwing Lair" FILES ${sources_raid_ek_bwl})

file(GLOB sources_raid_ek_naxx scripts/eastern_kingdoms/naxxramas/*.cpp scripts/eastern_kingdoms/naxxramas/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_raid_ek_naxx})
    source_group("Eastern Kingdoms Scripts\\Raids\\Naxxramas" FILES ${sources_raid_ek_naxx})

file(GLOB sources_raid_ek_zg scripts/eastern_kingdoms/zulgurub/*.cpp scripts/eastern_kingdoms/zulgurub/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_raid_ek_zg})
    source_group("Eastern Kingdoms Scripts\\Raids\\Zul Gurub" FILES ${sources_raid_ek_zg})

file(GLOB sources_instance_k_bdf scripts/kalimdor/blackfathom_deeps/*.cpp scripts/kalimdor/blackfathom_deeps/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_k_bdf})
    source_group("Kalimdor Scripts\\Instances\\Blackfathom Deeps" FILES ${sources_instance_k_bdf})

file(GLOB sources_instance_k_dm scripts/kalimdor/dire_maul/*.cpp scripts/kalimdor/dire_maul/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_k_dm})
    source_group("Kalimdor Scripts\\Instances\\Dire Maul" FILES ${sources_instance_k_dm})

file(GLOB sources_instance_k_mara scripts/kalimdor/maraudon/*.cpp scripts/kalimdor/maraudon/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_k_mara})
    source_group("Kalimdor Scripts\\Instances\\Maraudon Scripts" FILES ${sources_instance_k_mara})

file(GLOB sources_instance_k_rfd scripts/kalimdor/razorfen_downs/*.cpp scripts/kalimdor/razorfen_downs/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_k_rfd})
    source_group("Kalimdor Scripts\\Instances\\Razorfen Downs" FILES ${sources_instance_k_rfd})

file(GLOB sources_instance_k_rfk scripts/kalimdor/razorfen_kraul/*.cpp scripts/kalimdor/razorfen_kraul/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_k_rfk})
    source_group("Kalimdor Scripts\\Instances\\Razorfen Kraul" FILES ${sources_instance_k_rfk})

file(GLOB sources_instance_k_wc scripts/kalimdor/wailing_caverns/*.cpp scripts/kalimdor/wailing_caverns/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_k_wc})
    source_group("Kalimdor Scripts\\Instances\\Wailing Caverns" FILES ${sources_instance_k_wc})

file(GLOB sources_instance_k_zf scripts/kalimdor/zulfarrak/*.cpp scripts/kalimdor/zulfarrak/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_k_zf})
    source_group("Kalimdor Scripts\\Instances\\Zul Farrak" FILES ${sources_instance_k_zf})

file(GLOB sources_raid_k_ony scripts/kalimdor/onyxias_lair/*.cpp scripts/kalimdor/onyxias_lair/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_raid_k_ony})
    source_group("Kalimdor Scripts\\Raids\\Onyxias Lair" FILES ${sources_raid_k_ony})

file(GLOB sources_raid_k_raq scripts/kalimdor/ruins_of_ahnqiraj/*.cpp scripts/kalimdor/ruins_of_ahnqiraj/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_raid_k_raq})
    source_group("Kalimdor Scripts\\Raids\\Ruins of AQ" FILES ${sources_raid_k_raq})

file(GLOB sources_raid_k_taq scripts/kalimdor/temple_of_ahnqiraj/*.cpp scripts/kalimdor/temple_of_ahnqiraj/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_raid_k_taq})
    source_group("Kalimdor Scripts\\Raids\\Temple of AQ" FILES ${sources_raid_k_taq})

add_library(mangosscript STATIC ${mangosscript_LIB_SRCS})

target_include_directories(mangosscript
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/base
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(mangosscript
    PUBLIC
        game
)

if(PCH)
    ADD_CXX_PCH(mangosscript ${mangosscript_PCH})
endif()

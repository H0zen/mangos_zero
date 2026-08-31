#pragma once

/*
 * The version interface every translation unit uses.
 *
 * This header is hand-written and NEVER regenerated, which is the whole point:
 * the generated VersionData.h carries the git hash and therefore changes on
 * every commit. Exactly one translation unit (Version.cpp) includes it and
 * re-exports its contents through the functions below, so a new commit
 * recompiles that one object file and relinks -- and nothing else.
 *
 * Add a value here only as a function. A macro would put the literal back into
 * every caller and undo the isolation.
 */

#include <cstdint>

/* Genuinely fixed: not build-, git- or configure-dependent, so they cost
   nothing to expose directly. */
#define DEFAULT_PLAYER_LIMIT        100
#define DEFAULT_WORLDSERVER_PORT    8085
#define DEFAULT_REALMSERVER_PORT    3724

namespace MangosVersion
{
    /* product */
    char const* PackageName();          ///< "Mangos Zero"
    char const* Version();              ///< "22.6.4"
    char const* ProductRevision();      ///< "Mangos Zero 22.6.4"
    char const* ProductVersion();       ///< version + git, one line

    /* git */
    char const* Hash();
    char const* Date();
    char const* Branch();

    /* ready-made banner lines */
    char const* FullRevision();         ///< "Mangos revision: ..."
    char const* RunningSystem();        ///< "Running on: ..."
    char const* BuildHost();
    char const* BuildCMake();

    /* database contract */
    char const* WorldDbVersion();
    char const* WorldDbStructure();
    char const* WorldDbContent();
    char const* WorldDbDescription();

    char const* CharDbVersion();
    char const* CharDbStructure();
    char const* CharDbContent();
    char const* CharDbDescription();

    char const* RealmdDbVersion();
    char const* RealmdDbStructure();
    char const* RealmdDbContent();
    char const* RealmdDbDescription();

    /* configuration files */
    std::uint32_t MangosdConfigVersion();
    std::uint32_t RealmdConfigVersion();

    char const* SysConfDir();
    char const* MangosdConfigName();
    char const* RealmdConfigName();
    char const* MangosdConfigLocation();
    char const* RealmdConfigLocation();
}

/*
 * The single translation unit that sees the generated version data.
 *
 * VersionData.h changes on every commit; this file is the only C++ source
 * allowed to include it, so a commit rebuilds exactly this object and relinks.
 * Every other consumer goes through the accessors declared in Version.h and is
 * untouched by a new revision.
 */

#include "Version.h"
#include "VersionData.h"

namespace MangosVersion
{
    char const* PackageName()       { return MANGOS_PACKAGENAME; }
    char const* Version()           { return MANGOS_VERSION_STR; }
    char const* ProductRevision()   { return PROJECT_REVISION_NR; }
    char const* ProductVersion()    { return VER_PRODUCTVERSION_STR; }

    char const* Hash()              { return REVISION_HASH; }
    char const* Date()              { return REVISION_DATE; }
    char const* Branch()            { return REVISION_BRANCH; }

    char const* FullRevision()      { return MANGOS_FULL_REVISION; }
    char const* RunningSystem()     { return MANGOS_RUNNING_SYSTEM; }
    char const* BuildHost()         { return MANGOS_BUILD_HOST; }
    char const* BuildCMake()        { return MANGOS_BUILD_CMAKE; }

    char const* WorldDbVersion()     { return WORLD_DB_VERSION_NR; }
    char const* WorldDbStructure()   { return WORLD_DB_STRUCTURE_NR; }
    char const* WorldDbContent()     { return WORLD_DB_CONTENT_NR; }
    char const* WorldDbDescription() { return WORLD_DB_UPDATE_DESCRIPT; }

    char const* CharDbVersion()      { return CHAR_DB_VERSION_NR; }
    char const* CharDbStructure()    { return CHAR_DB_STRUCTURE_NR; }
    char const* CharDbContent()      { return CHAR_DB_CONTENT_NR; }
    char const* CharDbDescription()  { return CHAR_DB_UPDATE_DESCRIPT; }

    char const* RealmdDbVersion()     { return REALMD_DB_VERSION_NR; }
    char const* RealmdDbStructure()   { return REALMD_DB_STRUCTURE_NR; }
    char const* RealmdDbContent()     { return REALMD_DB_CONTENT_NR; }
    char const* RealmdDbDescription() { return REALMD_DB_UPDATE_DESCRIPT; }

    std::uint32_t MangosdConfigVersion() { return MANGOSD_CONFIG_VERSION; }
    std::uint32_t RealmdConfigVersion()  { return REALMD_CONFIG_VERSION; }

    char const* SysConfDir()             { return SYSCONFDIR; }
    char const* MangosdConfigName()      { return MANGOSD_CONFIG_NAME; }
    char const* RealmdConfigName()       { return REALMD_CONFIG_NAME; }
    char const* MangosdConfigLocation()  { return MANGOSD_CONFIG_LOCATION; }
    char const* RealmdConfigLocation()   { return REALMD_CONFIG_LOCATION; }
}

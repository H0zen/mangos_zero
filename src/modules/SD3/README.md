# ScriptDev3

Script engine for mangos-zero, developed from the old ScriptDev2.

Upstream ScriptDev3 is a single library shared by every MaNGOS core, with
preprocessor directives selecting the code for each expansion. This copy has
been reduced to the classic (1.12) content only: the Outland, Northrend and
Maelstrom trees are gone, as are the TBC/WotLK/Cataclysm zones, instances and
raids, and the per-expansion `#if defined (TBC) / (WOTLK) / (CATA) / (MISTS)`
branches have been resolved down to their `CLASSIC` side.

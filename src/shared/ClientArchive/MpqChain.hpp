#pragma once

// The archives a 1.12.1 client has open at once, in the order it opens them.
//
// One file name can live in several archives; the client answers with the copy
// from the archive it opened last, which is why a patch replaces a file without
// anything being rewritten. Lookups here walk the chain backwards for exactly
// that reason, so Spell.dbc comes back as patch-2 left it, not as dbc.MPQ ships
// it.
//
// A patch can also take a file away, and it does that by carrying an entry of its
// own -- zero bytes, flagged deleted -- over the name. Four of the DBCs are gone
// from 1.12.1 this way, SpellEffectNames and SpellAuraNames among them. So the
// walk stops at the first archive that mentions a name, whichever answer that
// archive gives: reading past a deletion would hand back a file the client cannot
// open.
//
// Nothing is ever extracted to answer a read: the bytes are decompressed
// straight into the caller's buffer.

#include "ClientArchive/MpqArchive.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace client
{
    class MpqChain
    {
    public:
        /// Adds one archive. Later archives win over earlier ones, so patches go last.
        bool AddArchive(const std::string& path);

        /// Adds the archive chain found in a client Data directory, in the client's own
        /// order, skipping whatever is not there. Returns how many archives opened.
        ///
        /// A locale names the sub-directory whose archives go on top -- they carry the
        /// translated DBCs, so they have to win over the patches. An empty locale takes
        /// every locale directory the install has, which is what a caller wants when it
        /// is after files no translation touches.
        std::size_t AddClientData(const std::string& dataDirectory,
                                  const std::string& locale = std::string());

        bool Read(const std::string& name, std::vector<std::uint8_t>* out) const;
        bool Contains(const std::string& name) const;

        /// Every name any archive in the chain declares in its "(listfile)", without
        /// duplicates. An archive with no listfile contributes nothing -- this says
        /// what the archives admit to holding, not what they hold.
        std::vector<std::string> ListedNames() const;

        std::size_t ArchiveCount() const { return m_archives.size(); }
        const std::string& LastError() const { return m_error; }

    private:
        /// The archive a name resolves to, or nullptr when no archive holds it or the
        /// highest-priority one that mentions it says it is deleted.
        const MpqArchive* Holder(const std::string& name) const;

        std::vector<MpqArchive> m_archives;
        mutable std::string m_error;
    };
}

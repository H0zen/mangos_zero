#pragma once

// Real client data for the parsers above, read through this tree's own MPQ reader.
//
// The chain resolves a name the way the client does -- last archive opened wins --
// so a file a patch replaced comes out patched. Which files exist is whatever the
// archives declare in their "(listfile)", the same source StormLib enumerated.

#include "IMpqArchive.hpp"

#include "ClientArchive/MpqChain.hpp"

#include <string>
#include <vector>

namespace world::terrain
{
    class MpqChainArchive : public IMpqArchive
    {
    public:
        bool AddArchive(const std::string& mpqPath);

        /// Opens a client Data directory, with \a locale's archives on top. Missing
        /// archives are skipped, not an error. Returns how many opened.
        int OpenClientData(const std::string& dataDir, const std::string& locale);

        bool Read(const std::string& path, std::vector<uint8_t>& out) override;
        bool Contains(const std::string& path) const override;

        /// Every listed name matching a '*' and '?' pattern, case-insensitively.
        std::vector<std::string> FindFiles(const std::string& pattern) const;

        size_t ArchiveCount() const { return m_chain.ArchiveCount(); }

    private:
        client::MpqChain m_chain;
    };
}

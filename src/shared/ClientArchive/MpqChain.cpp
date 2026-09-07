#include "ClientArchive/MpqChain.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace client
{
    namespace
    {
        /// 1.12 ships one archive per kind of asset -- no common.MPQ, no expansion.MPQ,
        /// and dbc, terrain, model and wmo as separate files. Lowest priority first.
        const char* const kBaseArchives[] = {
            "base.MPQ", "misc.MPQ", "fonts.MPQ", "interface.MPQ",
            "model.MPQ", "sound.MPQ", "speech.MPQ", "texture.MPQ",
            "terrain.MPQ", "wmo.MPQ", "dbc.MPQ"
        };

        const char* const kPatchArchives[] = {
            "patch.MPQ", "patch-2.MPQ", "patch-3.MPQ"
        };

        /// Inside a locale directory, and above the patches: a translated DBC has to win
        /// over the one the general patch carries. The numbered patch is last, which
        /// alphabetical order would get wrong -- "patch-enUS-2" sorts before
        /// "patch-enUS", and reading Map.dbc from the older of the two gives a table
        /// that parses perfectly and is missing rows.
        const char* const kLocaleArchives[] = {
            "base-{locale}.MPQ", "locale-{locale}.MPQ",
            "patch-{locale}.MPQ", "patch-{locale}-2.MPQ"
        };

        std::string WithLocale(const std::string& pattern, const std::string& locale)
        {
            const std::string token = "{locale}";
            std::string name = pattern;
            for (std::size_t at = name.find(token); at != std::string::npos;
                 at = name.find(token, at + locale.size()))
            {
                name.replace(at, token.size(), locale);
            }
            return name;
        }

        /// A four-letter directory such as enUS.
        bool NamesALocale(const std::string& name)
        {
            if (name.size() != 4)
            {
                return false;
            }

            for (char c : name)
            {
                if (!std::isalpha(static_cast<unsigned char>(c)))
                {
                    return false;
                }
            }

            return true;
        }

        std::string Lowered(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        }
    }

    bool MpqChain::AddArchive(const std::string& path)
    {
        MpqArchive archive;
        if (!archive.Open(path))
        {
            m_error = archive.LastError();
            return false;
        }

        m_archives.push_back(std::move(archive));
        return true;
    }

    std::size_t MpqChain::AddClientData(const std::string& dataDirectory, const std::string& locale)
    {
        namespace fs = std::filesystem;

        std::size_t opened = 0;
        std::error_code ec;
        const fs::path root(dataDirectory);

        for (const char* name : kBaseArchives)
        {
            const fs::path candidate = root / name;
            if (fs::exists(candidate, ec) && AddArchive(candidate.string()))
            {
                ++opened;
            }
        }

        for (const char* name : kPatchArchives)
        {
            const fs::path candidate = root / name;
            if (fs::exists(candidate, ec) && AddArchive(candidate.string()))
            {
                ++opened;
            }
        }

        std::vector<std::string> locales;
        if (!locale.empty())
        {
            locales.push_back(locale);
        }
        else if (fs::is_directory(root, ec))
        {
            for (const fs::directory_entry& entry : fs::directory_iterator(root, ec))
            {
                const std::string name = entry.path().filename().string();
                if (entry.is_directory(ec) && NamesALocale(name))
                {
                    locales.push_back(name);
                }
            }
            std::sort(locales.begin(), locales.end());
        }

        // A locale-merged install has no locale directory at all and every one of these
        // is simply absent; the DBCs are then in the root dbc.MPQ.
        for (const std::string& name : locales)
        {
            for (const char* pattern : kLocaleArchives)
            {
                const fs::path candidate = root / name / WithLocale(pattern, name);
                if (fs::exists(candidate, ec) && AddArchive(candidate.string()))
                {
                    ++opened;
                }
            }
        }

        if (opened == 0)
        {
            m_error = "no MPQ archives found in " + dataDirectory;
        }

        return opened;
    }

    const MpqArchive* MpqChain::Holder(const std::string& name) const
    {
        // A patch that drops a file does not remove it from the archive it shipped in --
        // it carries an entry of its own, zero bytes long, flagged deleted. The first
        // archive that mentions the name settles the question: if that entry is the
        // marker, the file is gone, and reading on down the chain would resurrect a copy
        // the client itself refuses to open. Storm answers such a name with its own
        // result code, which the open path treats exactly like "no such file".
        for (auto archive = m_archives.rbegin(); archive != m_archives.rend(); ++archive)
        {
            MpqArchive::Entry entry;
            if (!archive->Describe(name, &entry))
            {
                continue;
            }

            return entry.Exists() && !entry.Deleted() ? &*archive : nullptr;
        }

        return nullptr;
    }

    bool MpqChain::Read(const std::string& name, std::vector<std::uint8_t>* out) const
    {
        if (const MpqArchive* archive = Holder(name))
        {
            return archive->Read(name, out, &m_error);
        }

        m_error = "not found in any archive: " + name;
        return false;
    }

    bool MpqChain::Contains(const std::string& name) const
    {
        return Holder(name) != nullptr;
    }

    std::vector<std::string> MpqChain::ListedNames() const
    {
        std::vector<std::string> names;
        std::vector<std::uint8_t> listfile;

        for (const MpqArchive& archive : m_archives)
        {
            if (!archive.Contains("(listfile)") || !archive.Read("(listfile)", &listfile, &m_error))
            {
                continue;
            }

            std::string line;
            for (std::uint8_t byte : listfile)
            {
                if (byte == '\r' || byte == '\n')
                {
                    if (!line.empty())
                    {
                        names.push_back(line);
                        line.clear();
                    }
                    continue;
                }
                line.push_back(static_cast<char>(byte));
            }

            if (!line.empty())
            {
                names.push_back(line);
            }
        }

        std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b)
        {
            return Lowered(a) < Lowered(b);
        });
        names.erase(std::unique(names.begin(), names.end(), [](const std::string& a, const std::string& b)
        {
            return Lowered(a) == Lowered(b);
        }), names.end());

        return names;
    }
}

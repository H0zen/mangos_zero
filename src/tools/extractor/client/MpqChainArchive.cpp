#include "MpqChainArchive.hpp"

#include <algorithm>
#include <cctype>

namespace world::terrain
{
    namespace
    {
        char Folded(char c)
        {
            if (c == '/')
            {
                return '\\';
            }
            return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        /// '*' spans any run of characters, '?' one. Backtracking to the last star is
        /// what lets a pattern such as "DBFilesClient\*.dbc" survive a name whose tail
        /// nearly matched and then did not.
        bool Matches(const std::string& pattern, const std::string& name)
        {
            std::size_t p = 0;
            std::size_t n = 0;
            std::size_t star = std::string::npos;
            std::size_t retry = 0;

            while (n < name.size())
            {
                if (p < pattern.size() && pattern[p] == '*')
                {
                    star = p++;
                    retry = n;
                }
                else if (p < pattern.size() &&
                         (pattern[p] == '?' || Folded(pattern[p]) == Folded(name[n])))
                {
                    ++p;
                    ++n;
                }
                else if (star != std::string::npos)
                {
                    p = star + 1;
                    n = ++retry;
                }
                else
                {
                    return false;
                }
            }

            while (p < pattern.size() && pattern[p] == '*')
            {
                ++p;
            }

            return p == pattern.size();
        }
    }

    bool MpqChainArchive::AddArchive(const std::string& mpqPath)
    {
        return m_chain.AddArchive(mpqPath);
    }

    int MpqChainArchive::OpenClientData(const std::string& dataDir, const std::string& locale)
    {
        return static_cast<int>(m_chain.AddClientData(dataDir, locale));
    }

    bool MpqChainArchive::Read(const std::string& path, std::vector<uint8_t>& out)
    {
        return m_chain.Read(path, &out);
    }

    bool MpqChainArchive::Contains(const std::string& path) const
    {
        return m_chain.Contains(path);
    }

    std::vector<std::string> MpqChainArchive::FindFiles(const std::string& pattern) const
    {
        std::vector<std::string> found;
        if (pattern.empty())
        {
            return found;
        }

        for (const std::string& name : m_chain.ListedNames())
        {
            if (Matches(pattern, name))
            {
                found.push_back(name);
            }
        }

        return found;
    }
}

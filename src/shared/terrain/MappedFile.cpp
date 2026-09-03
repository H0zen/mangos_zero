#include "terrain/MappedFile.hpp"

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <fcntl.h>
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

namespace world::terrain
{
#ifdef _WIN32

    std::shared_ptr<const MappedFile> MappedFile::Open(const std::string& path)
    {
        HANDLE file = ::CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            return nullptr;
        }

        LARGE_INTEGER size{};
        if (!::GetFileSizeEx(file, &size) || size.QuadPart <= 0)
        {
            ::CloseHandle(file);
            return nullptr;
        }

        HANDLE mapping = ::CreateFileMappingA(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!mapping)
        {
            ::CloseHandle(file);
            return nullptr;
        }

        void* view = ::MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
        if (!view)
        {
            ::CloseHandle(mapping);
            ::CloseHandle(file);
            return nullptr;
        }

        std::shared_ptr<MappedFile> mapped(new MappedFile());
        mapped->m_data = static_cast<const uint8_t*>(view);
        mapped->m_size = static_cast<size_t>(size.QuadPart);
        mapped->m_handle = file;
        mapped->m_mapping = mapping;
        return mapped;
    }

    MappedFile::~MappedFile()
    {
        if (m_data)
        {
            ::UnmapViewOfFile(m_data);
        }
        if (m_mapping)
        {
            ::CloseHandle(m_mapping);
        }
        if (m_handle)
        {
            ::CloseHandle(m_handle);
        }
    }

#else

    std::shared_ptr<const MappedFile> MappedFile::Open(const std::string& path)
    {
        const int fd = ::open(path.c_str(), O_RDONLY);
        if (fd < 0)
        {
            return nullptr;
        }

        struct stat st{};
        if (::fstat(fd, &st) != 0 || st.st_size <= 0)
        {
            ::close(fd);
            return nullptr;
        }

        const size_t size = static_cast<size_t>(st.st_size);
        void* view = ::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (view == MAP_FAILED)
        {
            ::close(fd);
            return nullptr;
        }

        // The descriptor has done its job: the mapping keeps the file alive on
        // its own, and holding it open would spend one per resident tile.
        ::close(fd);

        std::shared_ptr<MappedFile> mapped(new MappedFile());
        mapped->m_data = static_cast<const uint8_t*>(view);
        mapped->m_size = size;
        return mapped;
    }

    MappedFile::~MappedFile()
    {
        if (m_data)
        {
            ::munmap(const_cast<uint8_t*>(m_data), m_size);
        }
    }

#endif
}

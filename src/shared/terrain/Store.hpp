#pragma once

// An array of PODs that either owns its memory or looks at a file mapping.
//
// The extractor builds tiles and needs to fill them; the server reads tiles and
// wants the bytes to stay in the page cache rather than on its heap. One type
// serves both: Adopt() takes a built vector, View() points at mapped bytes that
// something else keeps alive. Reads are identical either way, so the call sites
// that only index and size cannot tell which they hold.
//
// data() is computed on each call rather than cached, so moving a Store that
// owns cannot leave a pointer behind to the vector's old buffer.

#include <cassert>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>

namespace world::terrain
{
    template <class T>
    class Store
    {
        public:

            static_assert(std::is_trivially_copyable<T>::value,
                          "Store maps raw bytes, so its element must be trivially copyable");

            Store() = default;

            /// Take ownership of built data.
            void Adopt(std::vector<T> data) { m_owned = std::move(data); m_view = nullptr; m_viewSize = 0; }

            /// Point at memory owned by something else -- a mapping the holder
            /// keeps alive. Nothing is copied.
            void View(const T* data, size_t count) { m_owned.clear(); m_owned.shrink_to_fit(); m_view = data; m_viewSize = count; }

            bool Owns() const { return m_view == nullptr; }

            const T* data() const { return m_view ? m_view : m_owned.data(); }
            size_t size() const { return m_view ? m_viewSize : m_owned.size(); }
            bool empty() const { return size() == 0; }

            /// The only indexing. Deliberately const whatever the handle is:
            /// an overload pair split by constness would send a read through the
            /// writing path whenever the Store was reached through a non-const
            /// reference, and on a view that reads an empty vector.
            const T& operator[](size_t i) const { return data()[i]; }

            /// Element reference for a builder filling the array in place.
            /// Mapped bytes are read-only and shared by everything holding the
            /// tile, so this is not reachable from a view.
            T& Mutable(size_t i)
            {
                assert(Owns() && "writing through a mapped Store");
                return m_owned[i];
            }

            const T* begin() const { return data(); }
            const T* end() const { return data() + size(); }

            // --- building ---------------------------------------------------
            //
            // All of these act on the owned vector. Calling one on a view is a
            // mistake, not a copy-on-write: mapped tile bytes are read-only and
            // shared, and silently duplicating them would hide it.

            void assign(size_t count, const T& value) { DropView(); m_owned.assign(count, value); }
            void resize(size_t count) { DropView(); m_owned.resize(count); }
            void reserve(size_t count) { DropView(); m_owned.reserve(count); }
            void push_back(const T& value) { DropView(); m_owned.push_back(value); }
            void clear() { m_owned.clear(); m_view = nullptr; m_viewSize = 0; }

            void swap(std::vector<T>& other) { DropView(); m_owned.swap(other); }

            /// The owned buffer, for a builder that wants to fill it directly.
            std::vector<T>& Owned() { DropView(); return m_owned; }

        private:

            void DropView() { m_view = nullptr; m_viewSize = 0; }

            std::vector<T> m_owned;
            const T* m_view = nullptr;
            size_t m_viewSize = 0;
    };
}

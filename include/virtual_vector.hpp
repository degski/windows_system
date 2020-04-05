
// MIT License
//
// Copyright (c) 2020 degski
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <type_traits>

#include <sax/stl.hpp>

#include "winsys.hpp"

namespace sax {

template<typename SizeType, typename = std::enable_if_t<std::is_unsigned<SizeType>::value>>
struct growth_policy {
    [[nodiscard]] static SizeType grow ( SizeType const & cap_in_bytes_ ) noexcept {
        return cap_in_bytes_ + win::page_size_in_bytes;
    }
    [[nodiscard]] static SizeType shrink ( SizeType const & cap_in_bytes_ ) noexcept {
        return cap_in_bytes_ - win::page_size_in_bytes;
    }
};

template<typename ValueType, typename SizeType, SizeType Capacity, typename growth_policy = growth_policy<SizeType>>
struct virtual_vector {

    public:
    using value_type = ValueType;

    using pointer       = value_type *;
    using const_pointer = value_type const *;

    using reference       = value_type &;
    using const_reference = value_type const &;
    using rv_reference    = value_type &&;

    using size_type       = SizeType;
    using difference_type = std::make_signed<size_type>;

    using iterator               = pointer;
    using const_iterator         = const_pointer;
    using reverse_iterator       = pointer;
    using const_reverse_iterator = const_pointer;

    virtual_vector ( ) noexcept {
        if ( HEDLEY_UNLIKELY ( not win::set_privilege ( SE_LOCK_MEMORY_NAME, true ) ) )
            std::cout << "Could not set lock page privilege to enabled." << nl;
        m_end = m_begin =
            reinterpret_cast<pointer> ( win::virtual_alloc ( nullptr, capacity_in_bytes ( ), MEM_RESERVE, PAGE_READWRITE ) );
    };

    ~virtual_vector ( ) noexcept {
        if constexpr ( not std::is_scalar<value_type>::value ) {
            for ( auto & v : *this )
                v.~value_type ( );
        }
        if ( HEDLEY_LIKELY ( m_begin ) ) {
            win::virtual_free ( m_begin, capacity_in_bytes ( ), MEM_RELEASE );
            m_end = m_begin      = nullptr;
            m_committed_in_bytes = 0;
        }
        if ( HEDLEY_UNLIKELY ( not win::set_privilege ( SE_LOCK_MEMORY_NAME, false ) ) )
            std::cout << "Could not set lock page privilege to disabled." << nl;
    }

    // Size.

    private:
    [[nodiscard]] constexpr size_type capacity_in_bytes ( ) noexcept { return Capacity * sizeof ( value_type ); }
    [[nodiscard]] size_type committed_in_bytes ( ) const noexcept { return m_committed_in_bytes; }
    [[nodiscard]] size_type size_in_bytes ( ) const noexcept {
        return reinterpret_cast<char *> ( m_end ) - reinterpret_cast<char *> ( m_begin );
    }

    public:
    [[nodiscard]] constexpr size_type capacity ( ) noexcept { return Capacity; }
    [[nodiscard]] size_type committed ( ) const noexcept { return m_committed_in_bytes / sizeof ( value_type ); }
    [[nodiscard]] size_type size ( ) const noexcept {
        return reinterpret_cast<value_type *> ( m_end ) - reinterpret_cast<value_type *> ( m_begin );
    }
    [[nodiscard]] constexpr size_type max_size ( ) noexcept { return capacity ( ); }

    // Add.

    template<typename... Args>
    reference emplace_back ( Args &&... value_ ) noexcept {
        if ( HEDLEY_UNLIKELY ( size_in_bytes ( ) == m_committed_in_bytes ) ) {
            size_type new_commited = m_committed_in_bytes ? growth_policy::grow ( m_committed_in_bytes ) : win::page_size_in_bytes;
            win::virtual_alloc ( m_end, new_commited - m_committed_in_bytes, MEM_COMMIT, PAGE_READWRITE );
            m_committed_in_bytes = new_commited;
        }
        return *new ( m_end++ ) value_type{ std::forward<Args> ( value_ )... };
    }
    template<typename... Args>
    reference push_back ( Args &&... value_ ) noexcept {
        return emplace_back ( value_type{ std::forward<Args> ( value_ )... } );
    }

    // TODO lowering growth factor when vector becomes really large as compared to free memory.

    // Data.

    [[nodiscard]] const_pointer data ( ) const noexcept { return reinterpret_cast<pointer> ( m_begin ); }
    [[nodiscard]] pointer data ( ) noexcept { return const_cast<pointer> ( std::as_const ( *this ).data ( ) ); }

    // Iterators.

    [[nodiscard]] const_iterator begin ( ) const noexcept { return reinterpret_cast<pointer> ( m_begin ); }
    [[nodiscard]] const_iterator cbegin ( ) const noexcept { return begin ( ); }
    [[nodiscard]] iterator begin ( ) noexcept { return const_cast<iterator> ( std::as_const ( *this ).begin ( ) ); }

    [[nodiscard]] const_iterator end ( ) const noexcept { return m_end; }
    [[nodiscard]] const_iterator cend ( ) const noexcept { return end ( ); }
    [[nodiscard]] iterator end ( ) noexcept { return const_cast<iterator> ( std::as_const ( *this ).end ( ) ); }

    [[nodiscard]] const_iterator rbegin ( ) const noexcept { return m_end - 1; }
    [[nodiscard]] const_iterator crbegin ( ) const noexcept { return rbegin ( ); }
    [[nodiscard]] iterator rbegin ( ) noexcept { return const_cast<iterator> ( std::as_const ( *this ).rbegin ( ) ); }

    [[nodiscard]] const_iterator rend ( ) const noexcept { return reinterpret_cast<pointer> ( m_begin ) - 1; }
    [[nodiscard]] const_iterator crend ( ) const noexcept { return rend ( ); }
    [[nodiscard]] iterator rend ( ) noexcept { return const_cast<iterator> ( std::as_const ( *this ).rend ( ) ); }

    [[nodiscard]] const_reference front ( ) const noexcept { return *begin ( ); }
    [[nodiscard]] reference front ( ) noexcept { return const_cast<reference> ( std::as_const ( *this ).front ( ) ); }

    [[nodiscard]] const_reference back ( ) const noexcept { return *rbegin ( ); }
    [[nodiscard]] reference back ( ) noexcept { return const_cast<reference> ( std::as_const ( *this ).back ( ) ); }

    [[nodiscard]] const_reference at ( size_type const i_ ) const {
        if ( HEDLEY_LIKELY ( 0 <= i_ and i_ < size ( ) ) )
            return m_begin[ i_ ];
        else
            throw std::runtime_error ( "virtual_vector: index out of bounds" );
    }
    [[nodiscard]] reference at ( size_type const i_ ) { return const_cast<reference> ( std::as_const ( *this ).at ( i_ ) ); }

    [[nodiscard]] const_reference operator[] ( size_type const i_ ) const noexcept { return m_begin[ i_ ]; }
    [[nodiscard]] reference operator[] ( size_type const i_ ) noexcept {
        return const_cast<reference> ( std::as_const ( *this ).operator[] ( i_ ) );
    }

    private:
    pointer m_begin = nullptr, m_end = nullptr;
    size_type m_committed_in_bytes = 0;
};
} // namespace sax


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
    [[nodiscard]] static SizeType grow ( SizeType const & cap_in_bytes_ ) noexcept { return cap_in_bytes_ << 1; }
    [[nodiscard]] static SizeType shrink ( SizeType const & cap_in_bytes_ ) noexcept { return cap_in_bytes_ >> 1; }
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

    virtual_vector ( ) noexcept = default;

    private:
    public:
    virtual_vector ( virtual_vector const & vv_ ) {
        first_commit_page_impl ( vv_.m_committed_size_in_bytes );
        // todo set up space in this.
        if constexpr ( not std::is_scalar<value_type>::value ) {
            sax::memcpy_sse_16 ( m_begin, vv_.m_begin, vv_.m_committed_size_in_bytes );
        }
        else {
            for ( auto const & v : vv_ )
                new ( m_end++ ) value_type{ v };
        }
    }

    virtual_vector ( virtual_vector && vv_ ) noexcept { std::swap ( std::move ( vv_ ), *this ); }

    ~virtual_vector ( ) noexcept {
        clear_impl ( );
        if ( m_begin ) {
            win::virtual_free ( m_begin, capacity_in_bytes ( ), MEM_RELEASE );
            m_begin = nullptr;
        }
        win::set_privilege ( SE_LOCK_MEMORY_NAME, false );
    }

    void clear ( ) noexcept {
        clear_impl ( );
        m_end                     = m_begin;
        m_committed_size_in_bytes = 0u;
    }

    // Size.

    private:
    [[nodiscard]] constexpr size_type capacity_in_bytes ( ) noexcept { return Capacity * sizeof ( value_type ); }
    // m_committed_size_in_bytes is a variable.
    [[nodiscard]] size_type size_in_bytes ( ) const noexcept {
        return reinterpret_cast<char *> ( m_end ) - reinterpret_cast<char *> ( m_begin );
    }

    public:
    [[nodiscard]] constexpr size_type capacity ( ) noexcept { return Capacity; }
    [[nodiscard]] size_type committed ( ) const noexcept { return m_committed_size_in_bytes / sizeof ( value_type ); }
    [[nodiscard]] size_type size ( ) const noexcept {
        return reinterpret_cast<value_type *> ( m_end ) - reinterpret_cast<value_type *> ( m_begin );
    }
    [[nodiscard]] constexpr size_type max_size ( ) noexcept { return capacity ( ); }

    // Add.

    template<typename... Args>
    reference emplace_back ( Args &&... value_ ) noexcept {
        if ( HEDLEY_LIKELY ( m_begin ) ) {
            if ( HEDLEY_UNLIKELY ( size_in_bytes ( ) == m_committed_size_in_bytes ) ) {
                commit_page_impl ( m_end, m_committed_size_in_bytes );
                m_committed_size_in_bytes = growth_policy::grow ( m_committed_size_in_bytes );
            }
        }
        else {
            first_commit_page_impl ( );
        }
        return *new ( m_end++ ) value_type{ std::forward<Args> ( value_ )... };
    }
    template<typename... Args>
    reference push_back ( Args &&... value_ ) noexcept {
        return emplace_back ( value_type{ std::forward<Args> ( value_ )... } );
    }

    // TODO lowering growth factor when vector becomes really large as compared to free memory.
    // TODO virtual_queue

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
    void first_commit_page_impl ( size_type page_size_in_bytes_ = 0u ) {
        m_committed_size_in_bytes = win::page_size_in_bytes;
        if ( HEDLEY_UNLIKELY ( not win::set_privilege ( SE_LOCK_MEMORY_NAME, true ) ) )
            std::cout << "Could not set lock page privilege to enabled." << nl;
        m_end = m_begin = reinterpret_cast<pointer> (
            win::virtual_alloc ( win::virtual_alloc ( nullptr, capacity_in_bytes ( ), MEM_RESERVE, PAGE_READWRITE ),
                                 m_committed_size_in_bytes, MEM_COMMIT, PAGE_READWRITE ) );
    }

    void commit_page_impl ( pointer ptr_, size_t size_ ) noexcept {
        win::virtual_alloc ( ptr_, size_, MEM_COMMIT, PAGE_READWRITE );
    }
    void decommit_page_impl ( pointer ptr_, size_t size_ ) noexcept {
        win::virtual_alloc ( ptr_, size_, MEM_DECOMMIT, PAGE_NOACCESS );
    }

    void clear_impl ( ) noexcept {
        if ( m_committed_size_in_bytes ) {
            // Destroy objects.
            if constexpr ( not std::is_scalar<value_type>::value ) {
                for ( auto & v : *this )
                    v.~value_type ( );
            }
            // Tear-down committed.
            size_type com  = growth_policy::shrink ( committed ( ) );
            pointer rbegin = m_begin + com;
            for ( ; win::page_size_in_bytes == com; com = growth_policy::shrink ( com ), rbegin -= com )
                decommit_page_impl ( rbegin, com );
            decommit_page_impl ( m_begin, win::page_size_in_bytes );
        }
    }

    // Initialed with valid ptr to reserved memory and size = 0 (the number of committed pages).
    pointer m_begin = nullptr, m_end = nullptr;
    size_type m_committed_size_in_bytes;
};
} // namespace sax

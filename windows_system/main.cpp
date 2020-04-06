
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

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <iomanip>
#include <sax/iostream.hpp>
#include <iterator>
#include <list>
#include <map>
#include <numeric>
#include <random>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

#include <sax/compressed_pair.hpp>
#include <sax/integer.hpp>
#include <sax/stl.hpp>

#include <plf/plf_nanotimer.h>

#include <hedley.hpp>

#include "virtual_vector.hpp"

// extern unsigned long __declspec( dllimport ) __stdcall GetProcessHeaps ( unsigned long NumberOfHeaps, void ** ProcessHeaps );
// extern __declspec( dllimport ) void * __stdcall GetProcessHeap ( );

// -fsanitize=address

/*
    C:\Program Files\LLVM\lib\clang\10.0.0\lib\windows\clang_rt.asan_cxx-x86_64.lib;
    C:\Program Files\LLVM\lib\clang\10.0.0\lib\windows\clang_rt.asan-preinit-x86_64.lib;
    C:\Program Files\LLVM\lib\clang\10.0.0\lib\windows\clang_rt.asan-x86_64.lib
*/

// https://stackoverflow.com/questions/251248/how-can-i-get-the-sid-of-the-current-windows-account#251267

template<bool HAVE_LARGE_PAGES = false>
struct windows_system {

    using void_p = void *;

    // 2'097'152 = 200MB = 2 ^ 21
    //    65'536 =  64KB = 2 ^ 16

    ~windows_system ( ) noexcept {
        if ( HEDLEY_LIKELY ( m_reserved_pointer ) ) {
            sax::win::virtual_free ( m_reserved_pointer, m_reserved_size_ib, MEM_RELEASE );
            m_reserved_pointer       = nullptr;
            m_reserved_size_ib = 0u;
        }
        sax::win::set_privilege ( SE_LOCK_MEMORY_NAME, false );
    }

    [[nodiscard]] void_p reserve_and_commit_page ( size_t const capacity_ib_ ) noexcept {
        if ( HEDLEY_UNLIKELY ( not sax::win::set_privilege ( SE_LOCK_MEMORY_NAME, true ) ) ) {
            std::cout << "Could not set lock page privilege to enabled." << nl;
            return nullptr;
        }
        if constexpr ( HAVE_LARGE_PAGES ) {
            m_reserved_pointer =
                sax::win::virtual_alloc ( nullptr, capacity_ib_, MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE );
        }
        else {
            m_reserved_pointer =
                sax::win::virtual_alloc ( sax::win::virtual_alloc ( nullptr, capacity_ib_, MEM_RESERVE, PAGE_READWRITE ),
                                     page_size_ib, MEM_COMMIT, PAGE_READWRITE );
        }
        m_reserved_size_ib = capacity_ib_;
        return m_reserved_pointer;
    }

    void free_reserved_pages ( ) noexcept {
        if constexpr ( not HAVE_LARGE_PAGES ) {
            if ( m_reserved_pointer ) {
                sax::win::virtual_free ( m_reserved_pointer, m_reserved_size_ib, MEM_RELEASE );
                m_reserved_pointer       = nullptr;
                m_reserved_size_ib = 0u;
            }
        }
        else {
            // null op.
        }
    }

    template<bool HLP = HAVE_LARGE_PAGES, typename = std::enable_if_t<not HLP>>
    static void_p commit_page ( void_p ptr_, size_t size_ ) noexcept {
        return sax::win::virtual_alloc ( ptr_, size_, MEM_COMMIT, PAGE_READWRITE );
    }
    template<bool HLP = HAVE_LARGE_PAGES, typename = std::enable_if_t<not HLP>>
    static void decommit_page ( void_p ptr_, size_t size_ ) noexcept {
        sax::win::virtual_alloc ( ptr_, size_, MEM_DECOMMIT, PAGE_NOACCESS );
    }

    template<bool HLP = HAVE_LARGE_PAGES, typename = std::enable_if_t<not HLP>>
    static void_p reset_page ( void_p ptr_, size_t size_ ) noexcept {
        return sax::win::virtual_alloc ( ptr_, size_, MEM_RESET, PAGE_NOACCESS );
    }
    template<bool HLP = HAVE_LARGE_PAGES, typename = std::enable_if_t<not HLP>>
    static void reset_undo_page ( void_p ptr_, size_t size_ ) noexcept {
        sax::win::virtual_alloc ( ptr_, size_, MEM_RESET_UNDO, PAGE_READWRITE );
    }

    template<typename T>
    constexpr size_t type_page_size ( size_t large_page_size_ = 0u ) noexcept {
        assert ( ( page_size_ib / sizeof ( T ) ) * sizeof ( T ) == page_size_ib );
        return page_size_ib / sizeof ( T );
    }

    private:
    void_p m_reserved_pointer       = nullptr;
    size_t m_reserved_size_ib = 0u;

    public:
    static size_t const page_size_ib;
};
template<bool HAVE_LARGE_PAGES>
size_t const windows_system<HAVE_LARGE_PAGES>::page_size_ib = HAVE_LARGE_PAGES ? sax::win::large_page_minimum ( ) : 65'536ull;

using sys = windows_system<false>;

template<typename SizeType, typename = std::enable_if_t<std::is_unsigned<SizeType>::value>>
struct growth_policy {
    [[nodiscard]] static SizeType grow ( SizeType const & cap_ib_ ) noexcept { return cap_ib_ << 1; }
    [[nodiscard]] static SizeType shrink ( SizeType const & cap_ib_ ) noexcept { return cap_ib_ >> 1; }
};

// Overload std::is_scalar for your type if it can be copied with std::memcpy.

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
    void first_commit_impl ( size_type page_size_ib_ = 0u ) {
        m_committed_ib = page_size_ib_ ? page_size_ib_ : sys::page_size_ib;
        m_end = m_begin = reinterpret_cast<pointer> ( m_sys.reserve_and_commit_page ( Capacity * sizeof ( value_type ) ) );
    }

    public:
    virtual_vector ( virtual_vector const & vv_ ) {
        first_commit_impl ( vv_.m_committed_ib );
        // todo set up space in this.
        if constexpr ( not std::is_scalar<value_type>::value ) {
            // std::memcpy ( m_begin, vv_.m_begin, vv_.m_end - vv_.begin );
            sax::memcpy_sse_16 ( m_begin, vv_.m_begin, vv_.m_committed_ib );
        }
        else {
            for ( auto const & v : vv_ )
                new ( m_end++ ) value_type{ v };
        }
    }

    virtual_vector ( virtual_vector && vv_ ) noexcept { std::swap ( std::move ( vv_ ), *this ); }

    ~virtual_vector ( ) noexcept {
        clear_impl ( );
        m_sys.free_reserved_pages ( );
    }

    private:
    void push_up_committed ( size_type const to_commit_size_ib_ ) noexcept {
        size_type cib = m_committed_ib;
        pointer begin = m_end;
        pointer end   = m_begin + to_commit_size_ib_ / sizeof ( value_type );
        for ( ; begin == end; cib = growth_policy::grow ( cib ), begin += cib )
            sys::commit_page ( begin, cib );
    }
    void tear_down_committed ( size_type const to_commit_size_ib_ = 0u ) noexcept {
        size_type com                = growth_policy::shrink ( committed ( ) );
        pointer rbegin               = m_begin + com;
        size_type const to_committed = std::max ( sys::page_size_ib, to_commit_size_ib_ );
        for ( ; to_committed == com; com = growth_policy::shrink ( com ), rbegin -= com )
            sys::decommit_page ( rbegin, com );
        if ( not to_commit_size_ib_ )
            sys::decommit_page ( m_begin, sys::page_size_ib );
    }

    void clear_impl ( ) noexcept {
        if ( m_committed_ib ) {
            // Destroy objects.
            if constexpr ( not std::is_scalar<value_type>::value ) {
                for ( auto & v : *this )
                    v.~value_type ( );
            }
            tear_down_committed ( );
        }
    }

    public:
    void clear ( ) noexcept {
        clear_impl ( );
        // Reset to default state.
        m_end                = m_begin;
        m_committed_ib = 0u;
    }

    // Size.

    private:
    [[nodiscard]] constexpr size_type capacity_ib ( ) noexcept { return Capacity * m_sys.type_page_size<value_type> ( ); }
    // m_committed_ib is a variable.
    [[nodiscard]] size_type size_ib ( ) const noexcept {
        return reinterpret_cast<char *> ( m_end ) - reinterpret_cast<char *> ( m_begin );
    }

    public:
    [[nodiscard]] static constexpr size_type capacity ( ) noexcept { return Capacity; }
    [[nodiscard]] size_type committed ( ) const noexcept { return m_committed_ib / sizeof ( value_type ); }
    // [[nodiscard]] size_type size ( ) const noexcept { return size_ib ( ) / sizeof ( value_type ); }
    [[nodiscard]] size_type size ( ) const noexcept {
        return reinterpret_cast<value_type *> ( m_end ) - reinterpret_cast<value_type *> ( m_begin );
    }
    [[nodiscard]] static constexpr size_type max_size ( ) noexcept { return capacity ( ); }

    // Add.

    template<typename... Args>
    reference emplace_back ( Args &&... value_ ) noexcept {
        if ( HEDLEY_LIKELY ( m_begin ) ) {
            if ( HEDLEY_UNLIKELY ( size_ib ( ) == m_committed_ib ) ) {
                sys::commit_page ( m_end, m_committed_ib );
                m_committed_ib = growth_policy::grow ( m_committed_ib );
            }
        }
        else {
            first_commit_impl ( );
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
    sys m_sys;
    // Initialed with valid ptr to reserved memory and size = 0 (the number of committed pages).
    pointer m_begin = nullptr, m_end = nullptr;
    size_type m_committed_ib;
};

void handleEptr ( std::exception_ptr eptr ) { // Passing by value is ok.
    try {
        if ( eptr )
            std::rethrow_exception ( eptr );
    }
    catch ( std::exception const & e ) {
        std::cout << "Caught exception \"" << e.what ( ) << "\"\n";
    }
}

template<typename T, size_t S>
using heap_array = std::array<T, S>;

template<typename T, size_t S>
using heap_array_ptr = std::unique_ptr<heap_array<T, S>>;

int main78909 ( ) {

    std::exception_ptr eptr;

    try {
        using type = int;

        constexpr size_t s = 1024ull * 1024ull * 8, c = s * sizeof ( type );

        int * v1 ( ( int * ) std::malloc ( c ) );
        int * v2 ( ( int * ) std::calloc ( 1ull, c ) );

        std::fill ( v1, v1 + s, 123456789 );

        plf::nanotimer t;
        t.start ( );

        for ( int cnt = 0; cnt < 1'024; ++cnt )
            // sax::memcpy_sse_16 ( v2, v1, c );
            // std::memcpy ( v2, v1, c );
            sax::memcpy_avx ( v2, v1, c );

        std::uint64_t time = static_cast<std::uint64_t> ( t.get_elapsed_ms ( ) );

        std::cout << v2[ s - 100 ] << " " << time << " ms" << nl;

        std::free ( v1 );
        std::free ( v2 );
    }
    catch ( ... ) {
        eptr = std::current_exception ( ); // Capture.
    }
    handleEptr ( eptr );

    return EXIT_SUCCESS;
}

int main ( ) {

    std::exception_ptr eptr;

    try {

        /*

        std::vector<int> v{ 5, 9, 7, 3, 1, 6, 4, 8, 0, 2 };

        for ( auto & e : v )
            std::cout << e << ' ';
        std::cout << nl;

        std::make_heap ( v.begin ( ), v.end ( ) );

        for ( auto & e : v )
            std::cout << e << ' ';
        std::cout << nl;

        */

        sax::virtual_vector<int, size_t, 1'000'000> vv;

        for ( int i = 0; i < 16'384; ++i )
            vv.emplace_back ( i );

        std::cout << vv.size ( ) << " " << vv.committed ( ) << nl;

        for ( auto & v : vv )
            std::cout << v << ' ';
        std::cout << nl;

        vv.emplace_back ( 16'384 );

        std::cout << vv.size ( ) << " " << vv.committed ( ) << nl;

        for ( auto & v : vv )
            std::cout << v << ' ';
        std::cout << nl;
    }
    catch ( ... ) {
        eptr = std::current_exception ( ); // Capture.
    }
    handleEptr ( eptr );

    return EXIT_SUCCESS;
}

template<typename T>
struct comp_less {

    bool operator( ) ( T const & l, T const & r ) noexcept {
        if ( l < r )
            std::cout << "took a left" << nl;
        else
            std::cout << "took a right" << nl;
        return l < r;
    };
};

/*
Windows http://www.roylongbottom.org.uk/busspd2k.zip

 xx = (int *)sax::win::virtual_alloc(nullptr, useMemK*1024+256, MEM_COMMIT, PAGE_READWRITE);

Linux http://www.roylongbottom.org.uk/memory_benchmarks.tar.gz

#ifdef Bits64
   array = (long long *)_mm_malloc(memoryKBytes[ipass-1]*1024, 16);
#else
   array = (int *)_mm_malloc(memoryKBytes[ipass-1]*1024, 16);

Results and other links (MP version, Android) are in:

http://www.roylongbottom.org.uk/busspd2k%20results.htm
*/

#include <boost/pfr/precise.hpp>

struct some_person {
    std::string name;
    unsigned birth_year;
};

struct zip_functions {
    template<typename U>
    static U add ( U const & a, U const b ) {
        return a + b;
    }
};

template<typename InIt1, typename InIt2, typename OutIt, typename BinaryOperation>
void zip ( InIt1 bin1, InIt1 ein1, InIt2 bin2, OutIt bout ) {
    std::transform ( bin1, ein1, bin2, bout, BinaryOperation::add );
}

int main78768678 ( ) {
    some_person val{ "Edgar Allan Poe", 1809 };

    std::cout << boost::pfr::get<0> ( val )                           // No macro!
              << " was born in " << boost::pfr::get<1> ( val ) << nl; // Works with any aggregate initializables!

    return EXIT_SUCCESS;
}
#if 0

template<typename Type>
struct vm_committed_ptr {

    public:
    using value_type    = Type;         // void
    using pointer       = value_type *; // void*
    using const_pointer = value_type const *;

    using reference       = value_type &;
    using const_reference = value_type const &;
    using rv_reference    = value_type &&;

    using size_type   = std::size_t;
    using offset_type = std::uint16_t;

    // Constructors.

    explicit vm_committed_ptr ( ) noexcept = default;

    explicit vm_committed_ptr ( vm_committed_ptr const & ) noexcept = delete;

    vm_committed_ptr ( vm_committed_ptr && moving_ ) noexcept { std::swap ( moving_, *this ); }

    template<typename U>
    explicit vm_committed_ptr ( vm_committed_ptr<U> && moving_ ) noexcept {
        vm_committed_ptr<value_type> tmp ( moving_.release ( ) );
        std::swap ( tmp, *this );
    }

    vm_committed_ptr ( vm_handle && h_ ) noexcept : handle ( h_.ptr ? sys::commit_page ( h_ ) : vm_handle{ } ) {}

    // Destruct.

    ~vm_committed_ptr ( ) noexcept {
        if ( handle.ptr )
            sys::decommit_page ( handle );
    }

    // Assignment.

    [[maybe_unused]] vm_committed_ptr & operator= ( vm_committed_ptr const & ) noexcept = delete;

    [[maybe_unused]] vm_committed_ptr & operator= ( vm_committed_ptr && moving_ ) noexcept {
        moving_.swap ( *this );
        return *this;
    }

    template<typename U>
    [[maybe_unused]] vm_committed_ptr & operator= ( vm_committed_ptr<U> && moving_ ) noexcept {
        vm_committed_ptr<value_type> tmp ( moving_.release ( ) );
        std::swap ( tmp, *this );
        return *this;
    }

    [[maybe_unused]] vm_committed_ptr & operator= ( vm_handle && moving_ ) noexcept {
        handle = moving_;
        if ( handle.ptr )
            sys::commit_page ( handle );
        return *this;
    }

    // Get.

    [[nodiscard]] const_pointer operator-> ( ) const noexcept { return get ( ); }
    [[nodiscard]] pointer operator-> ( ) noexcept { return get ( ); }

    // [[nodiscard]] const_reference operator* ( ) const noexcept { return *get ( ); }
    // [[nodiscard]] reference operator* ( ) noexcept { return *get ( ); }

    [[nodiscard]] pointer get ( ) const noexcept { return handle.ptr; }
    [[nodiscard]] pointer get ( ) noexcept { return std::as_const ( *this ).get ( ); }

    [[nodiscard]] static size_type max_size ( ) noexcept { return std::numeric_limits<size_type>::max ( ); }

    void swap ( vm_committed_ptr & src ) noexcept { std::swap ( *this, src ); }

    // Other.

    private:
    [[nodiscard]] vm_committed_ptr release ( ) noexcept {
        vm_committed_ptr tmp;
        std::swap ( tmp, *this );
        return tmp;
    }

    public:
    void reset ( ) noexcept {
        vm_committed_ptr tmp = release ( );
        sys::decommit_page ( tmp );
    }
    void reset ( vm_handle && h_ ) noexcept {
        vm_committed_ptr tmp ( std::move ( h_ ) );
        std::swap ( tmp, *this );
        sys::decommit_page ( tmp );
    }
    template<typename U>
    void reset ( vm_committed_ptr<U> && moving_ ) noexcept {
        vm_committed_ptr<value_type> tmp ( std::move ( moving_ ) );
        std::swap ( tmp, *this );
        sys::decommit_page ( tmp );
    }

    private:
    vm_handle handle;

    [[nodiscard]] pointer addressof_this ( ) const noexcept {
        return reinterpret_cast<pointer> ( const_cast<vm_committed_ptr *> ( this ) );
    }
};
#endif

template<typename T>
struct vm_allocator {

    using value_type      = T;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference       = value_type &;
    using const_reference = value_type const &;
    using pointer         = value_type *;
    using const_pointer   = value_type const *;

    template<class U>
    struct rebind {
        using other = vm_allocator<U>;
    };

    vm_allocator ( ) noexcept                      = default;
    vm_allocator ( vm_allocator const & ) noexcept = default;
    template<class U>
    vm_allocator ( vm_allocator<U> const & ) noexcept {}

    vm_allocator select_on_container_copy_construction ( ) const { return *this; }

    [[nodiscard]] T * allocate ( size_type count ) { return static_cast<T *> ( mi_new_n ( count, sizeof ( T ) ) ); }
    [[nodiscard]] T * allocate ( size_type count, void const * ) { return allocate ( count ); }

    void deallocate ( T * p, size_type ) { mi_free ( p ); }

    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap            = std::true_type;
    using is_always_equal                        = std::true_type;

    template<typename U, typename... Args>
    void construct ( U * p, Args &&... args ) {
        ::new ( p ) U ( std::forward<Args> ( args )... );
    }
    template<typename U>
    void destroy ( U * p ) noexcept {
        p->~U ( );
    }

    [[nodiscard]] size_type max_size ( ) const noexcept {
        return ( std::numeric_limits<std::ptrdiff_t>::max ( ) / sizeof ( value_type ) );
    }

    [[nodiscard]] const_pointer address ( const_reference x ) const noexcept { return std::addressof ( x ); }
    [[nodiscard]] pointer address ( const_reference x ) noexcept {
        return const_cast<pointer> ( std::as_const ( *this ).address ( x ) );
    }
};

template<typename T1, typename T2>
bool operator== ( vm_allocator<T1> const &, vm_allocator<T2> const & ) noexcept {
    return true;
}
template<typename T1, typename T2>
bool operator!= ( vm_allocator<T1> const &, vm_allocator<T2> const & ) noexcept {
    return false;
}

/*

    void clear_impl ( ) noexcept {
        if ( m_committed_size_ib ) {
            // Destroy objects.
            if constexpr ( not std::is_scalar<value_type>::value ) {
                for ( auto & v : *this )
                    v.~value_type ( );
            }
            // Tear-down committed.
            size_type com  = growth_policy::shrink ( committed ( ) );
            pointer rbegin = m_begin + com;
            for ( ; sax::win::page_size_ib == com; com = growth_policy::shrink ( com ), rbegin -= com )
                sax::win::virtual_alloc ( rbegin, com, MEM_DECOMMIT, PAGE_NOACCESS );
            sax::win::virtual_alloc ( m_begin, sax::win::page_size_ib, MEM_DECOMMIT, PAGE_NOACCESS );
        }
    }

*/

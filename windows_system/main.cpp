
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

#include <array>
#include <iomanip>
#include <sax/iostream.hpp>
#include <iterator>
#include <list>
#include <map>
#include <random>
#include <span>
#include <string>
#include <type_traits>
#include <vector>
#include <Windows.h>

// extern unsigned long __declspec( dllimport ) __stdcall GetProcessHeaps ( unsigned long NumberOfHeaps, void ** ProcessHeaps );
// extern __declspec( dllimport ) void * __stdcall GetProcessHeap ( );

// -fsanitize=address
/*
C:\Program Files\LLVM\lib\clang\10.0.0\lib\windows\clang_rt.asan_cxx-x86_64.lib;
C:\Program Files\LLVM\lib\clang\10.0.0\lib\windows\clang_rt.asan-preinit-x86_64.lib;
C:\Program Files\LLVM\lib\clang\10.0.0\lib\windows\clang_rt.asan-x86_64.lib
*/

template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
void print_bits ( T const n_ ) noexcept {
    using Tu = typename std::make_unsigned<T>::type;
    Tu n;
    std::memcpy ( &n, &n_, sizeof ( Tu ) );
    Tu i = Tu ( 1 ) << ( sizeof ( Tu ) * 8 - 1 );
    while ( i ) {
        putchar ( int ( ( n & i ) > 0 ) + int ( 48 ) );
        i >>= 1;
    }
}

template<typename Stream, typename T, typename = std::enable_if_t<std::is_pointer<T>::value>>
Stream & operator<< ( Stream & out_, T const & n_ ) noexcept {
    std::int8_t n[ 8 ];
    std::memcpy ( &n, &n_, sizeof ( T ) );
    std::swap ( n[ 0 ], n[ 6 ] );
    std::swap ( n[ 1 ], n[ 7 ] );
    std::swap ( n[ 2 ], n[ 4 ] );
    std::swap ( n[ 3 ], n[ 5 ] );
    out_ << "0x" << std::setfill ( '0' ) << std::setw ( 4 ) << std::hex << std::uppercase
         << ( std::uint32_t ) ( ( std::uint16_t * ) n )[ 0 ];
    for ( int i = 1; i < 4; ++i )
        out_ << '\'' << std::setw ( 4 ) << ( std::uint32_t ) ( ( std::uint16_t * ) n )[ i ];
    out_ << std::setfill ( ' ' ) << std::setw ( 0 ) << std::dec << std::nouppercase << lf;
    return out_;
}

constexpr size_t page_size_in_bytes ( ) noexcept { return 65'536ull; }

template<typename T>
constexpr size_t type_page_size ( ) noexcept {
    return page_size_in_bytes ( ) / sizeof ( T );
}

struct vm_handle {
    using void_p = void *;
    void_p ptr   = nullptr;
    size_t size  = 0;
};

template<typename SizeType = unsigned long>
struct windows_system {

    using size_type = typename std::make_unsigned<SizeType>::type;
    using void_p    = void *;

    static vm_handle reserved;

    windows_system ( ) = delete;

    ~windows_system ( ) noexcept {
        if ( reserved.ptr ) {
            VirtualFree ( windows_system::reserved.ptr, windows_system::reserved.size, MEM_RELEASE );
            windows_system::reserved = vm_handle{ };
        }
    }

    [[nodiscard]] static vm_handle reserve_pages ( size_t n_ ) noexcept {
        return { ( windows_system::reserved = { VirtualAlloc ( nullptr, n_ * page_size_in_bytes ( ), MEM_RESERVE, PAGE_NOACCESS ),
                                                n_ } )
                     .ptr,
                 size_t{ 0 } };
    }

    static void free_reserved_pages ( ) noexcept {
        VirtualFree ( windows_system::reserved.ptr, windows_system::reserved.size * page_size_in_bytes ( ), MEM_RELEASE );
        windows_system::reserved = vm_handle{ };
    }

    public:
    static void_p commit_page ( void_p ptr_, size_t size_ ) noexcept {
        return VirtualAlloc ( ptr_, size_, MEM_COMMIT, PAGE_READWRITE );
    }

    static void decommit_page ( void_p ptr_, size_t size_ ) noexcept { VirtualFree ( ptr_, size_, MEM_DECOMMIT ); }

    // Decommit memory for 3rd page of addresses.

    // Commit memory for 3rd page of addresses.
    // lpPage3 = VirtualAlloc ( lpBase + ( 2 * 4096 ), 4096, MEM_COMMIT, PAGE_READWRITE );
    // static_assert ( sizeof ( SizeType ) >= sizeof ( unsigned long ), "SizeType too small" );

    [[nodiscard]] static size_type virtual_page_size ( ) noexcept { return windows_system::info.dwPageSize; }
    [[nodiscard]] static size_type virtual_size ( ) noexcept {
        return reinterpret_cast<char *> ( windows_system::info.lpMaximumApplicationAddress ) -
               reinterpret_cast<char *> ( windows_system::info.lpMinimumApplicationAddress );
    }
    [[nodiscard]] static size_type granularity ( ) noexcept { return windows_system::info.dwAllocationGranularity; }
    [[nodiscard]] static std::pair<void_p, void_p> application_memory_bounds ( ) noexcept {
        return { windows_system::info.lpMinimumApplicationAddress, windows_system::info.lpMaximumApplicationAddress };
    }
    [[nodiscard]] static size_type number_virtual_cores ( ) noexcept { return windows_system::info.dwNumberOfProcessors; }

    static void update ( ) noexcept { windows_system::info = get_system_information ( ); }

    [[nodiscard]] static std::vector<void_p> heaps ( ) noexcept {
        void_p h[ 16 ];
        size_type s = GetProcessHeaps ( 0, NULL );
        while ( GetProcessHeaps ( s, h ) != s )
            s = GetProcessHeaps ( 0, NULL );
        return { h, h + s };
    }

    // Default heap.
    [[nodiscard]] static void_p heap ( ) noexcept { return GetProcessHeap ( ); }

    [[nodiscard]] static void_p stack ( ) noexcept {
        volatile void * p = std::addressof ( p );
        return const_cast<void *> ( p );
    }

    private:
    static SYSTEM_INFO info;

    static SYSTEM_INFO get_system_information ( ) noexcept {
        assert ( virtual_page_size ( ) == std::numeric_limits<std::uint16_t>::max ( ) );
        SYSTEM_INFO si;
        GetSystemInfo ( &si );
        return si;
    }

    /*

        typedef struct _SYSTEM_INFO {
            union {
                DWORD dwOemId;
                struct {
                    WORD wProcessorArchitecture;
                    WORD wReserved;
                } DUMMYSTRUCTNAME;
            } DUMMYUNIONNAME;
            DWORD dwPageSize;
            LPVOID lpMinimumApplicationAddress;
            LPVOID lpMaximumApplicationAddress;
            DWORD_PTR dwActiveProcessorMask;
            DWORD dwNumberOfProcessors;
            DWORD dwProcessorType;
            DWORD dwAllocationGranularity;
            WORD wProcessorLevel;
            WORD wProcessorRevision;
        } SYSTEM_INFO, *LPSYSTEM_INFO;

    */
};

template<typename SizeType>
vm_handle windows_system<SizeType>::reserved;

template<typename SizeType>
SYSTEM_INFO windows_system<SizeType>::info = get_system_information ( );

using sys = windows_system<unsigned long>;

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

    [[nodiscard]] static int pointer_alignment ( void * ptr_ ) noexcept {
        return ( int ) ( ( ( std::uintptr_t ) ptr_ ) & ( ( std::uintptr_t ) ( -( ( std::intptr_t ) ptr_ ) ) ) );
    }
};
#endif
template<typename SizeType> //, typename = std::enable_if_t<std::is_unsigned<SizeType>::value>>
struct growth_policy {
    [[nodiscard]] static SizeType grow ( SizeType const & cap_in_bytes_ ) noexcept { return cap_in_bytes_ << 1; }
    [[nodiscard]] static SizeType shrink ( SizeType const & cap_in_bytes_ ) noexcept { return cap_in_bytes_ >> 1; }
};

// Overload std::is_scalar for your type if it can be copied with std::memcpy.

template<typename ValueType, typename SizeType, SizeType Capacity, typename GrowthPolicy = growth_policy<SizeType>>
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
    void first_commit_impl ( size_type const & committed_in_bytes_ ) {
        m_committed_in_bytes = page_size_in_bytes ( );
        m_end = m_begin = reinterpret_cast<pointer> (
            sys::commit_page ( reinterpret_cast<pointer> ( sys::reserve_pages ( Capacity / type_page_size<value_type> ( ) ).ptr ),
                               committed_in_bytes_ ) );
    }

    public:
    virtual_vector ( virtual_vector const & vv_ ) {
        first_commit_impl ( vv_.m_committed_in_bytes );
        if constexpr ( not std::is_scalar<value_type>::value ) {
            std::memcpy ( m_begin, vv_.m_begin, vv_.m_end - vv_.begin );
        }
        else {
            for ( auto const & v : vv_ )
                new ( m_end++ ) value_type{ v };
        }
    }

    virtual_vector ( virtual_vector && vv_ ) noexcept { std::swap ( std::move ( vv_ ), *this ); }

    ~virtual_vector ( ) noexcept {
        clear_impl ( );
        sys::free_reserved_pages ( );
    }
    /*
    [[maybe_unused]] virtual_vector const & operator= ( virtual_vector const & vv_ ) const {
        // Copy to the elements of this if they are exist (on both sides).
        if ( size ( ) < vv_.size ( ) ) {
            auto const area = std::span<value_type>{ vv_.m_begin, vv_.m_begin + size ( ) };
            pointer end     = m_begin;
            for ( auto const & v : area )
                *end++ = v;
            // Adjust committed.
            if ( committed ( ) < vv_.commited ( ) ) {
                // Grow this.
            }
            else {
            }

            area = std::span<value_type>{ vv_.m_begin + size ( ), vv_.m_end };
            for ( auto const & v : area )
                push_back ( v );
        }
        else {
        }

        first_commit_impl ( vv_.m_committed_in_bytes );
        if constexpr ( not std::is_scalar<value_type>::value ) {
            std::memcpy ( m_begin, vv_.m_begin, vv_.m_end - vv_.begin );
        }
        else {
            for ( auto const & v : vv_ )
                new ( m_end++ ) value_type{ v };
        }
    }
    */
    private:
    void push_up_committed ( size_type const to_commit_size_in_bytes_ ) noexcept {
        size_type cib = m_committed_in_bytes;
        pointer begin = m_end;
        pointer end   = m_begin + to_commit_size_in_bytes_ / sizeof ( value_type );
        for ( ; begin == end; cib = GrowthPolicy::grow ( cib ), begin += cib )
            sys::commit_page ( begin, cib );
    }

    // Tear down committed.
    void tear_down_committed ( size_type const to_commit_size_in_bytes_ = 0u ) noexcept {
        size_type committed          = GrowthPolicy::shrink ( committed );
        pointer rbegin               = m_begin + committed;
        size_type const to_committed = std::max ( page_size_in_bytes ( ), to_commit_size_in_bytes_ );
        for ( ; to_committed == committed; committed = GrowthPolicy::shrink ( committed ), rbegin -= committed )
            sys::decommit_page ( rbegin, committed );
        if ( not to_commit_size_in_bytes_ )
            sys::decommit_page ( m_begin, page_size_in_bytes ( ) );
    }

    void clear_impl ( ) noexcept {
        if ( m_committed_in_bytes ) {
            // Destroy object.
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
        m_committed_in_bytes = 0u;
    }

    private:
    [[nodiscard]] static constexpr size_type capacity_in_bytes ( ) noexcept { return Capacity * type_page_size<value_type> ( ); }
    // m_committed_in_bytes is a variable.
    [[nodiscard]] size_type size_in_bytes ( ) const noexcept {
        return reinterpret_cast<char *> ( m_end ) - reinterpret_cast<char *> ( m_begin );
    }

    public:
    [[nodiscard]] static constexpr size_type capacity ( ) noexcept { return Capacity; }
    [[nodiscard]] size_type committed ( ) const noexcept { return m_committed_in_bytes / sizeof ( value_type ); }
    [[nodiscard]] size_type size ( ) const noexcept { return size_in_bytes ( ) / sizeof ( value_type ); }

    [[nodiscard]] static constexpr size_type max_size ( ) noexcept { return capacity ( ); }

    template<typename... Args>
    reference emplace_back ( Args &&... value_ ) noexcept {
        if ( m_begin ) {
            if ( size_in_bytes ( ) == m_committed_in_bytes ) {
                sys::commit_page ( m_end, m_committed_in_bytes );
                m_committed_in_bytes = GrowthPolicy::grow ( m_committed_in_bytes );
            }
        }
        else {
            m_committed_in_bytes = page_size_in_bytes ( );
            m_end = m_begin = reinterpret_cast<pointer> ( sys::commit_page (
                reinterpret_cast<pointer> ( sys::reserve_pages ( Capacity / type_page_size<value_type> ( ) ).ptr ),
                m_committed_in_bytes ) );
        }
        return *new ( m_end++ ) value_type{ std::forward<Args> ( value_ )... };
    }
    template<typename... Args>
    reference push_back ( Args &&... value_ ) noexcept {
        return emplace_back ( value_type{ std::forward<Args> ( value_ )... } );
    }

    // TODO vectorized std::copy.
    // TODO lowering growth factor when vector becomes really large as compared to free memory.
    // TODO virtual_queue

    /*
    template<typename Pair>
    struct map_comaparator {
        bool operator( ) ( Pair const & a, Pair const & b ) const noexcept { return a.first < b.first; }
    };
    iterator binary_find ( key_type const & key_ ) const noexcept {
        auto first = std::lower_bound ( begin ( ), end ( ), key_, map_comaparator<key_value_type> ( ) );
        return first != end ( ) and not map_comaparator<key_value_type> ( key_, *first ) ? first : end ( );
    }
    iterator linear_lowerbound ( key_type const & key ) const noexcept {
        for ( key_value_type const & kv : *this )
            if ( kv.first >= key )
                return std::addressof ( kv );
        return end ( );
    };
    iterator linear_find ( key_type const & key_ ) const noexcept {
        auto first = linear_lowerbound ( key_ );
        return first != end ( ) and key_ == *first ? first : end ( );
    }
    iterator find ( value_type const & val_ ) const noexcept {
        for ( key_value_type const & kv : *this )
            if ( kv.second == val_ )
                return std::addressof ( kv );
        return end ( );
    }
    */
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
        if ( 0 <= i_ and i_ < size ( ) )
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
    // Initialed with valid ptr to reserved memory and size = 0 (the number of committed pages).
    pointer m_begin = nullptr, m_end = nullptr;
    std::size_t m_committed_in_bytes;
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

#include <immintrin.h>

namespace sax {
// dst and src must be 32-byte aligned.
// size must be multiple of 32*2 = 64 bytes.
inline void memcpy_avx ( void * dst, void const * src, size_t size ) noexcept {
    // https://hero.handmade.network/forums/code-discussion/t/157-memory_bandwidth_+_implementing_memcpy
    constexpr size_t stride = 2 * sizeof ( __m256i );
    while ( size ) {
        __m256i a = _mm256_load_si256 ( ( __m256i * ) src + 0 );
        __m256i b = _mm256_load_si256 ( ( __m256i * ) src + 1 );
        _mm256_stream_si256 ( ( __m256i * ) dst + 0, a );
        _mm256_stream_si256 ( ( __m256i * ) dst + 1, b );
        size -= stride;
        src = reinterpret_cast<uint8_t const *> ( src ) + stride;
        dst = reinterpret_cast<uint8_t *> ( dst ) + stride;
    }
}
// dst and src must be 16-byte aligned
// size must be multiple of 16*2 = 32 bytes
inline void memcpy_sse ( void * dst, void const * src, size_t size ) noexcept {
    size_t stride = 2 * sizeof ( __m128 );
    while ( size ) {
        __m128 a = _mm_load_ps ( ( float * ) ( reinterpret_cast<uint8_t const *> ( src ) + 0 * sizeof ( __m128 ) ) );
        __m128 b = _mm_load_ps ( ( float * ) ( reinterpret_cast<uint8_t const *> ( src ) + 1 * sizeof ( __m128 ) ) );
        _mm_stream_ps ( ( float * ) ( reinterpret_cast<uint8_t *> ( dst ) + 0 * sizeof ( __m128 ) ), a );
        _mm_stream_ps ( ( float * ) ( reinterpret_cast<uint8_t *> ( dst ) + 1 * sizeof ( __m128 ) ), b );
        size -= stride;
        src = reinterpret_cast<uint8_t const *> ( src ) + stride;
        dst = reinterpret_cast<uint8_t *> ( dst ) + stride;
    }
}

// Pointer alignment.
[[nodiscard]] static inline int pointer_alignment ( void * ptr_ ) noexcept {
    return ( int ) ( ( std::uintptr_t ) ptr_ & ( std::uintptr_t ) - ( ( std::intptr_t ) ptr_ ) );
}
} // namespace sax

#include <plf/plf_nanotimer.h>

#if 0
#    define mc sax::memcpy_avx

#else
#    define mc sax::memcpy_sse
#endif
int main ( ) {

    std::exception_ptr eptr;

    try {

        constexpr size_t size = 65536 * 2048;

        char * a = reinterpret_cast<char *> ( _aligned_malloc ( size, 32 ) );
        char * b = reinterpret_cast<char *> ( _aligned_malloc ( size, 32 ) );

        for ( int i = 0; i < size; ++i ) {
            a[ i ] = 123;
            b[ i ] = 0;
        }

        plf::nanotimer t;

        t.start ( );
        mc ( b, a, size );
        auto r = t.get_elapsed_ms ( );

        std::cout << ( size_t ) r << " ms " << ( int ) b[ 1024 * 1024 - 1 ] << nl;

        for ( int i = 0; i < size; ++i )
            b[ i ] = 0;

        t.start ( );
        mc ( b, a, size );
        r = t.get_elapsed_ms ( );

        std::cout << ( size_t ) r << " ms " << ( int ) b[ 1024 * 1024 - 1 ] << nl;

        for ( int i = 0; i < size; ++i )
            b[ i ] = 0;

        mc ( b, a, size );
        r = t.get_elapsed_ms ( );

        std::cout << ( size_t ) r << " ms " << ( int ) b[ 1024 * 1024 - 1 ] << nl;

        for ( int i = 0; i < size; ++i )
            b[ i ] = 0;

        t.start ( );
        mc ( b, a, size );
        r = t.get_elapsed_ms ( );

        std::cout << ( size_t ) r << " ms " << ( int ) b[ 1024 * 1024 - 1 ] << nl;

        exit ( 0 );

        /*
        void * r1 = sys::commit_page ( sys::reserve_pages ( 1'000'000 ).ptr, 1 * page_size_in_bytes ( ) );

        void * r2 = sys::commit_page ( reinterpret_cast<int *> ( r1 ) + 1 * type_page_size<int> ( ), 1 * page_size_in_bytes ( )
        );

        void * r3 = sys::commit_page ( reinterpret_cast<int *> ( r1 ) + 2 * type_page_size<int> ( ), 2 * page_size_in_bytes ( )
        );

        std::span<int> vvv{ reinterpret_cast<int *> ( r1 ), 4 * type_page_size<int> ( ) };

        int i = 0;
        for ( auto & v : vvv )
            new ( std::addressof ( v ) ) int{ i++ };
        */
        virtual_vector<int, size_t, 1'000'000> vv;

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

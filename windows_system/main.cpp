
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
    out_ << std::setfill ( ' ' ) << std::setw ( 0 ) << std::dec << std::nouppercase;
    return out_;
}

template<typename T>
constexpr size_t type_page_size ( ) noexcept {
    return 65'384ull / sizeof ( T );
}
constexpr size_t page_size ( ) noexcept { return 65'384ull; }

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

    // Reserve a 10 MB range of addresses.
    [[nodiscard]] static vm_handle reserve_pages ( size_t n_ ) noexcept {
        return { ( windows_system::reserved = { VirtualAlloc ( nullptr, n_ * ( std::numeric_limits<std::uint16_t>::max ( ) + 1 ),
                                                               MEM_RESERVE, PAGE_NOACCESS ),
                                                n_ } )
                     .ptr,
                 size_t{ 0 } };
    }

    static void free_reserved_pages ( ) noexcept {
        VirtualFree ( windows_system::reserved.ptr,
                      windows_system::reserved.size * ( std::numeric_limits<std::uint16_t>::max ( ) + 1 ), MEM_DECOMMIT,
                      PAGE_NOACCESS );
        windows_system::reserved = vm_handle{ };
    }

    public:
    [[nodiscard]] static void_p commit_page ( vm_handle const & handle_ ) noexcept {

        std::cout << "commit page at " << handle_.ptr << " with size " << std::hex << std::uppercase << handle_.size
                  << std::nouppercase << std::dec << nl;

        return VirtualAlloc ( handle_.ptr, handle_.size, MEM_COMMIT, PAGE_READWRITE );
    }
    static void decommit_page ( vm_handle & handle_ ) noexcept {
        VirtualFree ( handle_.ptr, handle_.size * ( std::numeric_limits<std::uint16_t>::max ( ) + 1 ), MEM_DECOMMIT,
                      PAGE_NOACCESS );
    }

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

using win_system = windows_system<unsigned long>;

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

    vm_committed_ptr ( vm_handle && h_ ) noexcept : handle ( h_.ptr ? win_system::commit_page ( h_ ) : vm_handle{ } ) {}

    // Destruct.

    ~vm_committed_ptr ( ) noexcept {
        if ( handle.ptr )
            win_system::decommit_page ( handle );
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
            win_system::commit_page ( handle );
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
        win_system::decommit_page ( tmp );
    }
    void reset ( vm_handle && h_ ) noexcept {
        vm_committed_ptr tmp ( std::move ( h_ ) );
        std::swap ( tmp, *this );
        win_system::decommit_page ( tmp );
    }
    template<typename U>
    void reset ( vm_committed_ptr<U> && moving_ ) noexcept {
        vm_committed_ptr<value_type> tmp ( std::move ( moving_ ) );
        std::swap ( tmp, *this );
        win_system::decommit_page ( tmp );
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

template<typename Value, size_t Capacity>
struct virtual_vector {

    public:
    using value_type = Value;

    using pointer       = value_type *;
    using const_pointer = value_type const *;

    using reference       = value_type &;
    using const_reference = value_type const &;
    using rv_reference    = value_type &&;

    using size_type       = std::size_t;
    using difference_type = std::make_signed<size_type>;

    using iterator               = pointer;
    using const_iterator         = const_pointer;
    using reverse_iterator       = pointer;
    using const_reverse_iterator = const_pointer;

    virtual_vector ( ) : m_begin ( win_system::reserve_pages ( Capacity ) ), m_end ( reinterpret_cast<pointer> ( m_begin.ptr ) ) {}

    ~virtual_vector ( ) {}

    void clear ( ) noexcept {
        if constexpr ( not std::is_scalar<Value>::value ) {
            for ( auto & v : *this )
                v.~value_type ( );
        }
        m_end = reinterpret_cast<pointer> ( m_begin.ptr );
    }

    [[nodiscard]] static constexpr size_type capacity ( ) noexcept {
        return Capacity * ( ( std::numeric_limits<std::uint16_t>::max ( ) + 1 ) / sizeof ( value_type ) );
    }
    [[nodiscard]] static constexpr size_type max_size ( ) noexcept { return capacity ( ); }

    [[nodiscard]] size_type size ( ) const noexcept { return m_end - reinterpret_cast<pointer> ( m_begin.ptr ); }

    [[nodiscard]] size_type committed ( ) const noexcept {
        return m_begin.size * ( ( std::numeric_limits<std::uint16_t>::max ( ) + 1 ) / sizeof ( value_type ) );
    }

    template<typename... Args>
    reference emplace_back ( Args &&... value_ ) noexcept {
        if ( m_begin.ptr ) {
            if ( size ( ) == committed ( ) ) {
                std::cout << "inc" << ' ';
                win_system::commit_page ( vm_handle{ m_end, m_begin.size } ).ptr;
                m_begin.size <<= 1;
            }
        }
        else {
            std::cout << "init" << nl;
            m_begin.size = 1;
            m_begin.ptr  = win_system::commit_page ( m_begin ).ptr;
            m_end        = reinterpret_cast<pointer> ( m_begin.ptr );
        }
        return *new ( m_end++ ) value_type{ std::forward<Args> ( value_ )... };
    }

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

    [[nodiscard]] const_pointer data ( ) const noexcept { return reinterpret_cast<pointer> ( m_begin.ptr ); }
    [[nodiscard]] pointer data ( ) noexcept { return const_cast<pointer> ( std::as_const ( *this ).data ( ) ); }

    // Iterators.

    [[nodiscard]] const_iterator begin ( ) const noexcept { return reinterpret_cast<pointer> ( m_begin.ptr ); }
    [[nodiscard]] const_iterator cbegin ( ) const noexcept { return begin ( ); }
    [[nodiscard]] iterator begin ( ) noexcept { return const_cast<iterator> ( std::as_const ( *this ).begin ( ) ); }

    [[nodiscard]] const_iterator end ( ) const noexcept { return m_end; }
    [[nodiscard]] const_iterator cend ( ) const noexcept { return end ( ); }
    [[nodiscard]] iterator end ( ) noexcept { return const_cast<iterator> ( std::as_const ( *this ).end ( ) ); }

    [[nodiscard]] const_iterator rbegin ( ) const noexcept { return m_end - 1; }
    [[nodiscard]] const_iterator crbegin ( ) const noexcept { return rbegin ( ); }
    [[nodiscard]] iterator rbegin ( ) noexcept { return const_cast<iterator> ( std::as_const ( *this ).rbegin ( ) ); }

    [[nodiscard]] const_iterator rend ( ) const noexcept { return reinterpret_cast<pointer> ( m_begin.ptr ) - 1; }
    [[nodiscard]] const_iterator crend ( ) const noexcept { return rend ( ); }
    [[nodiscard]] iterator rend ( ) noexcept { return const_cast<iterator> ( std::as_const ( *this ).rend ( ) ); }

    [[nodiscard]] const_reference front ( ) const noexcept { return *begin ( ); }
    [[nodiscard]] reference front ( ) noexcept { return const_cast<reference> ( std::as_const ( *this ).front ( ) ); }

    [[nodiscard]] const_reference back ( ) const noexcept { return *rbegin ( ); }
    [[nodiscard]] reference back ( ) noexcept { return const_cast<reference> ( std::as_const ( *this ).back ( ) ); }

    [[nodiscard]] const_reference at ( size_type const i_ ) const {
        if ( 0 <= i_ and i_ < size ( ) )
            return m_begin.ptr[ i_ ];
        else
            throw std::runtime_error ( "virtual_vector: index out of bounds" );
    }
    [[nodiscard]] reference at ( size_type const i_ ) { return const_cast<reference> ( std::as_const ( *this ).at ( i_ ) ); }

    [[nodiscard]] const_reference operator[] ( size_type const i_ ) const noexcept { return m_begin.ptr[ i_ ]; }
    [[nodiscard]] reference operator[] ( size_type const i_ ) noexcept {
        return const_cast<reference> ( std::as_const ( *this ).operator[] ( i_ ) );
    }

    vm_handle m_begin; // Initialed with valid ptr to reserved memory and size = 0 (the number of committed pages).
    pointer m_end = nullptr;
};

#endif

void handleEptr ( std::exception_ptr eptr ) { // Passing by value is ok.
    try {
        if ( eptr )
            std::rethrow_exception ( eptr );
    }
    catch ( std::exception const & e ) {
        std::cout << "Caught exception \"" << e.what ( ) << "\"\n";
    }
}

int main ( ) {

    std::exception_ptr eptr;

    try {

        void * r1 = win_system::commit_page ( vm_handle{ win_system::reserve_pages ( 1'000'000 ).ptr, 1 * page_size ( ) } );

        void * r2 = win_system::commit_page (
            vm_handle{ reinterpret_cast<int *> ( r1 ) + 1 * type_page_size<int> ( ), 1 * page_size ( ) } );

        void * r3 = win_system::commit_page (
            vm_handle{ reinterpret_cast<int *> ( r1 ) + 2 * type_page_size<int> ( ), 2 * page_size ( ) } );

        std::span<int> vv{ reinterpret_cast<int *> ( r1 ), 4 * type_page_size<int> ( ) };

        int i = 0;
        for ( auto & v : vv )
            new ( std::addressof ( v ) ) int{ i++ };

        /*
        virtual_vector<int, 1'000'000'000'000> vv;

        for ( int i = 0; i < 16'384; ++i )
            vv.emplace_back ( i );

        std::cout << vv.size ( ) << " " << vv.committed ( ) << nl;

        for ( auto & v : vv )
            std::cout << v << ' ';
        std::cout << nl;

        vv.emplace_back ( 16'384 );

        std::cout << vv.size ( ) << " " << vv.committed ( ) << nl;
        */
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


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
#include <string>
#include <type_traits>
#include <vector>
#include <Windows.h>

// extern unsigned long __declspec( dllimport ) __stdcall GetProcessHeaps ( unsigned long NumberOfHeaps, void ** ProcessHeaps );
// extern __declspec( dllimport ) void * __stdcall GetProcessHeap ( );

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
    std::uint16_t n[ 4 ];
    std::memcpy ( &n, &n_, sizeof ( T ) );
    out_ << "0x" << std::setfill ( '0' ) << std::setw ( 4 ) << std::hex << ( std::uint32_t ) n[ 0 ];
    for ( int i = 1; i < 4; ++i )
        out_ << '\'' << std::setfill ( '0' ) << std::setw ( 4 ) << std::hex << ( std::uint32_t ) n[ i ];
    return out_;
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
            VirtualFree ( reserved.ptr, reserved.size, MEM_RELEASE );
            reserved = vm_handle{ };
        }
    }

    private:
    // Reserve a 10 MB range of addresses.
    [[nodiscard]] static vm_handle reserve_pages ( size_t n_ ) {
        return { VirtualAlloc ( NULL, n_ * 65'535ull, MEM_RESERVE, PAGE_NOACCESS ), n_ };
    }

    public:
    [[nodiscard]] static vm_handle commit_page ( vm_handle const & handle_ ) {
        return { VirtualAlloc ( handle_.ptr, handle_.size * 65'535ull, MEM_COMMIT, PAGE_READWRITE ), handle_.size };
    }
    [[nodiscard]] static void decommit_page ( vm_handle & handle_ ) {
        VirtualFree ( handle_.ptr, handle_.size * 65'535ull, MEM_DECOMMIT, PAGE_NOACCESS );
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

    using system = windows_system<unsigned long>;

    // Default heap.
    [[nodiscard]] static void_p heap ( ) noexcept { return GetProcessHeap ( ); }

    [[nodiscard]] static void_p stack ( ) noexcept {
        volatile void * p = std::addressof ( p );
        return const_cast<void *> ( p );
    }

    private:
    static SYSTEM_INFO info;

    static SYSTEM_INFO get_system_information ( ) noexcept {
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
SYSTEM_INFO windows_system<SizeType>::info = get_system_information ( );

template<typename SizeType>
vm_handle windows_system<SizeType>::reserved;

using win_system = windows_system<unsigned long>;

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

    public
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
        vm_committed_ptr<T> tmp ( std::move ( moving_ ) );
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

int main ( ) { return EXIT_SUCCESS; }

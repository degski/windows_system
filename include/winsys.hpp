
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

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <sax/iostream.hpp>

#include <hedley.hpp>

#include <processthreadsapi.h>

#pragma comment( lib, "Advapi32.lib" )

namespace sax::win {

inline SYSTEM_INFO get_system_information ( ) noexcept {
    SYSTEM_INFO si;
    GetSystemInfo ( std::addressof ( si ) );
    return si;
}

inline SYSTEM_INFO const info = get_system_information ( );

inline constexpr std::size_t page_size_in_bytes = 65'536ull;

[[nodiscard]] inline void * get_token_handle ( ) {
    void * token_handle = nullptr;
    OpenThreadToken ( GetCurrentThread ( ), TOKEN_ADJUST_PRIVILEGES, false, std::addressof ( token_handle ) );
    if ( HEDLEY_UNLIKELY ( GetLastError ( ) == ERROR_NO_TOKEN ) ) {
        if ( HEDLEY_UNLIKELY (
                 not OpenProcessToken ( GetCurrentProcess ( ), TOKEN_ADJUST_PRIVILEGES, std::addressof ( token_handle ) ) ) )
            std::cout << "OpenProcessToken error:" << GetLastError ( ) << nl;
    }
    return token_handle;
}

[[maybe_unused]] inline bool set_privilege_impl ( void * token,                         // access token handle
                                                  wchar_t const * const privilege_name, // name of privilege to enable/disable
                                                  bool enable_privilige ) noexcept {    // to enable or disable privilege
    // https://docs.microsoft.com/en-us/windows/win32/secauthz/enabling-and-disabling-privileges-in-c--
    TOKEN_PRIVILEGES tp;
    LUID luid;
    if ( HEDLEY_UNLIKELY ( not LookupPrivilegeValue ( nullptr,                        // lookup privilege on local system
                                                      privilege_name,                 // privilege to lookup
                                                      std::addressof ( luid ) ) ) ) { // receives LUID of privilege
        std::cout << "LookupPrivilegeValue error: " << GetLastError ( ) << nl;
        return false;
    }
    tp.PrivilegeCount       = 1;
    tp.Privileges[ 0 ].Luid = luid;
    if ( HEDLEY_LIKELY ( enable_privilige ) )
        tp.Privileges[ 0 ].Attributes = SE_PRIVILEGE_ENABLED;
    else
        tp.Privileges[ 0 ].Attributes = 0;
    // Enable the privilege or disable all privileges.
    if ( HEDLEY_UNLIKELY (
             not AdjustTokenPrivileges ( token, false, std::addressof ( tp ), sizeof ( TOKEN_PRIVILEGES ), nullptr, nullptr ) ) ) {
        std::cout << "AdjustTokenPrivileges error:" << GetLastError ( ) << nl;
        return false;
    }
    if ( HEDLEY_UNLIKELY ( GetLastError ( ) == ERROR_NOT_ALL_ASSIGNED ) ) {
        std::cout << "The token does not have the specified privilege." << nl;
        return false;
    }
    return true;
}

[[maybe_unused]] inline bool set_privilege ( wchar_t const * const privilege_name, // name of privilege to enable/disable
                                             bool enable_privilige ) noexcept {
    return set_privilege_impl ( get_token_handle ( ), privilege_name, enable_privilige );
}

[[nodiscard]] inline size_t large_page_minimum ( ) noexcept { return GetLargePageMinimum ( ); }
[[nodiscard]] inline size_t virtual_page_size ( ) noexcept { return info.dwPageSize; }

[[maybe_unused]] inline LPVOID virtual_alloc ( LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect ) noexcept {
    return VirtualAlloc ( lpAddress, dwSize, flAllocationType, flProtect );
}
[[maybe_unused]] inline BOOL virtual_free ( LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType ) noexcept {
    return VirtualFree ( lpAddress, dwSize, dwFreeType );
}

} // namespace win

// using struct _SYSTEM_INFO {
//      union {
//          DWORD dwOemId;
//          struct {
//              WORD wProcessorArchitecture;
//              WORD wReserved;
//          } DUMMYSTRUCTNAME;
//      } DUMMYUNIONNAME;
//      DWORD dwPageSize;
//      LPVOID lpMinimumApplicationAddress;
//      LPVOID lpMaximumApplicationAddress;
//      DWORD_PTR dwActiveProcessorMask;
//      DWORD dwNumberOfProcessors;
//      DWORD dwProcessorType;
//      DWORD dwAllocationGranularity;
//      WORD wProcessorLevel;
//      WORD wProcessorRevision;
//  } SYSTEM_INFO, *LPSYSTEM_INFO;


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

#include <processthreadsapi.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <charconv>
#include <memory>
#include <stdexcept>
#include <string>

#include <hedley.hpp>

#pragma comment( lib, "Advapi32.lib" )

namespace sax::win {

inline SYSTEM_INFO get_system_information ( ) noexcept {
    SYSTEM_INFO si;
    GetSystemInfo ( std::addressof ( si ) );
    return si;
}

inline SYSTEM_INFO const info = get_system_information ( );

inline constexpr std::size_t page_size_b = 16ull * 65'536ull;

std::string last_error ( ) noexcept {
    std::string str{ '\0', 15 }; // SSO limit VS.
    if ( auto [ p, ec ] = std::to_chars ( str.data ( ), str.data ( ) + str.size ( ), GetLastError ( ) ); ec == std::errc ( ) ) {
        str.resize ( static_cast<std::size_t> ( p - str.data ( ) ) );
        return str;
    }
    return { };
}

[[nodiscard]] inline void * get_token_handle ( ) {
    void * token_handle = nullptr;
    OpenThreadToken ( GetCurrentThread ( ), TOKEN_ADJUST_PRIVILEGES, false, std::addressof ( token_handle ) );
    if ( HEDLEY_UNLIKELY ( GetLastError ( ) == ERROR_NO_TOKEN ) ) {
        if ( HEDLEY_UNLIKELY (
                 not OpenProcessToken ( GetCurrentProcess ( ), TOKEN_ADJUST_PRIVILEGES, std::addressof ( token_handle ) ) ) )
            throw std::runtime_error ( "OpenProcessToken error: " + last_error ( ) );
    }
    return token_handle;
}

[[maybe_unused]] inline void set_privilege_impl ( void * token,                         // access token handle
                                                  wchar_t const * const privilege_name, // name of privilege to enable/disable
                                                  bool enable_privilige ) {             // to enable or disable privilege
    // https://docs.microsoft.com/en-us/windows/win32/secauthz/enabling-and-disabling-privileges-in-c--
    TOKEN_PRIVILEGES tp;
    LUID luid;
    if ( HEDLEY_UNLIKELY ( not LookupPrivilegeValue ( nullptr,                      // lookup privilege on local system
                                                      privilege_name,               // privilege to lookup
                                                      std::addressof ( luid ) ) ) ) // receives LUID of privilege
        throw std::runtime_error ( "LookupPrivilegeValue error: " + last_error ( ) );
    tp.PrivilegeCount       = 1;
    tp.Privileges[ 0 ].Luid = luid;
    if ( HEDLEY_LIKELY ( enable_privilige ) )
        tp.Privileges[ 0 ].Attributes = SE_PRIVILEGE_ENABLED;
    else
        tp.Privileges[ 0 ].Attributes = 0;
    // Enable the privilege or disable all privileges.
    if ( HEDLEY_UNLIKELY (
             not AdjustTokenPrivileges ( token, false, std::addressof ( tp ), sizeof ( TOKEN_PRIVILEGES ), nullptr, nullptr ) ) )
        throw std::runtime_error ( "AdjustTokenPrivileges error: " + last_error ( ) );
    if ( HEDLEY_UNLIKELY ( GetLastError ( ) == ERROR_NOT_ALL_ASSIGNED ) )
        throw std::runtime_error ( "the token does not have the specified privilege" );
}

[[maybe_unused]] inline void set_privilege ( wchar_t const * const privilege_name, // name of privilege to enable/disable
                                             bool enable_privilige ) {
    set_privilege_impl ( get_token_handle ( ), privilege_name, enable_privilige );
}

[[nodiscard]] inline size_t large_page_minimum ( ) noexcept { return GetLargePageMinimum ( ); }

[[maybe_unused]] inline LPVOID virtual_alloc ( LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect ) noexcept {
    return VirtualAlloc ( lpAddress, dwSize, flAllocationType, flProtect );
}
[[maybe_unused]] inline BOOL virtual_free ( LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType ) noexcept {
    return VirtualFree ( lpAddress, dwSize, dwFreeType );
}

} // namespace sax::win

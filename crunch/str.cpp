/*
 
 MIT License
 
 Copyright (c) 2017 Chevy Ray Johnston
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 
 */

#include "str.hpp"

#if defined _MSC_VER || defined __MINGW32__
#include <locale>
#include <codecvt>
wstring StrToPath(const string& str)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.from_bytes(str);
}
string PathToStr(const wstring& str)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.to_bytes(str);
}
#else
const string& StrToPath(const string& str)
{
    return str;
}
const string& PathToStr(const string& str)
{
    return str;
}
#endif

#if defined _MSC_VER || defined __MINGW32__
#include <windows.h>
#include <vector>

string NormalizePath(const string& path)
{
    if (path.empty()) {
        return "";
    }
    DWORD bufferLength = GetFullPathNameA(path.c_str(), 0, nullptr, nullptr);
    if (bufferLength == 0) {
        // Error getting length, return original path or handle error
        return path;
    }

    std::vector<char> buffer(bufferLength);
    DWORD result = GetFullPathNameA(path.c_str(), bufferLength, buffer.data(), nullptr);
    if (result == 0 || result > bufferLength) {
        // Error getting path, return original path or handle error
        return path;
    }
    
    string fullPath = buffer.data();
    // Replace backslashes with forward slashes for consistency internally
    for (char& c : fullPath) {
        if (c == '\\') {
            c = '/';
        }
    }
    // If it's a directory and doesn't end with a slash, add one.
    // This might be too aggressive, consider if tinydir handles this.
    // For now, let's ensure it for consistency with how paths might have been manually entered.
    // Check if it's likely a directory (no extension or ends with / already)
    // This check is heuristic. A more robust way is to check if it IS a directory after GetFullPathNameA.
    // However, for input paths to packer, they are expected to be directories.
    size_t lastDot = fullPath.rfind('.');
    size_t lastSlash = fullPath.rfind('/');
    if (fullPath.back() != '/' && (lastDot == string::npos || (lastSlash != string::npos && lastDot < lastSlash))) {
         // Heuristic: if no dot, or last dot is before last slash, it's likely a dir.
         // Or if it was a directory path like ".\Orc\Orc" (no trailing slash)
         // We need to be careful not to add a slash to a file name output prefix.
         // The problem description was about INPUT directories.
         // Let's only ensure trailing slash for paths that were identified as input directories.
         // This function is generic, so maybe the trailing slash logic belongs in main.cpp
         // For now, just return the GetFullPathNameA result with forward slashes.
    }

    return fullPath;
}
#else
string NormalizePath(const string& path)
{
    // On non-Windows, realpath() could be used, but for now, return as is.
    // tinydir should handle relative paths fine on POSIX.
    // If issues arise, implement realpath here.
    string tempPath = path;
    // Replace backslashes with forward slashes for consistency internally
    for (char& c : tempPath) {
        if (c == '\\') {
            c = '/';
        }
    }
    return tempPath;
}
#endif

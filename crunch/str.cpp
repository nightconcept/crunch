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

#if defined _MSC_VER || defined __MINGW32__
#include <direct.h> // For _mkdir, _wmkdir
#include <sys/stat.h> // For _stat, stat
#include <io.h> // For _access
#else
#include <sys/stat.h> // For mkdir, stat
#include <unistd.h> // For access
#endif
#include <iostream> // For cerr

void EnsureDirectoryExists(const string& path) {
    if (path.empty()) {
        return;
    }

    string normalizedPath = path; // Use a mutable copy
    // Ensure consistent forward slashes
    for (char& c : normalizedPath) {
        if (c == '\\') {
            c = '/';
        }
    }
    // Remove trailing slash if present, as we process segments
    if (normalizedPath.back() == '/') {
        normalizedPath.pop_back();
        if (normalizedPath.empty()) return; // Path was just "/" or "\"
    }

    size_t pos = 0;
    string current_path_segment;

    // Iterate through path segments and create them if they don't exist
    while ((pos = normalizedPath.find('/', pos)) != string::npos) {
        current_path_segment = normalizedPath.substr(0, pos);
        if (!current_path_segment.empty()) {
            // Skip drive letters like "C:"
            if (current_path_segment.length() == 2 && current_path_segment[1] == ':') {
                pos++;
                continue;
            }
            #if defined _MSC_VER || defined __MINGW32__
                // Check if directory exists
                struct _stat info;
                if (_wstat(StrToPath(current_path_segment).c_str(), &info) != 0) { // Path doesn't exist
                    if (_wmkdir(StrToPath(current_path_segment).c_str()) != 0) {
                        cerr << "Error creating directory (segment): " << current_path_segment << " (Error: " << strerror(errno) << ")" << endl;
                        // Optionally, throw an exception or return an error code
                        return;
                    }
                } else if (!(info.st_mode & S_IFDIR)) { // Path exists but is not a directory
                    cerr << "Error: Path exists but is not a directory: " << current_path_segment << endl;
                    return;
                }
            #else
                struct stat info;
                if (stat(current_path_segment.c_str(), &info) != 0) { // Path doesn't exist
                    if (mkdir(current_path_segment.c_str(), 0777) != 0 && errno != EEXIST) { // Check EEXIST in case of race condition
                        cerr << "Error creating directory (segment): " << current_path_segment << " (Error: " << strerror(errno) << ")" << endl;
                        return;
                    }
                } else if (!S_ISDIR(info.st_mode)) { // Path exists but is not a directory
                    cerr << "Error: Path exists but is not a directory: " << current_path_segment << endl;
                    return;
                }
            #endif
        }
        pos++; // Move past the found slash
    }

    // Create the final directory component (the full path)
    #if defined _MSC_VER || defined __MINGW32__
        struct _stat info;
        if (_wstat(StrToPath(normalizedPath).c_str(), &info) != 0) {
            if (_wmkdir(StrToPath(normalizedPath).c_str()) != 0) {
                cerr << "Error creating directory (final): " << normalizedPath << " (Error: " << strerror(errno) << ")" << endl;
            }
        } else if (!(info.st_mode & S_IFDIR)) {
             cerr << "Error: Path exists but is not a directory: " << normalizedPath << endl;
        }
    #else
        struct stat info;
        if (stat(normalizedPath.c_str(), &info) != 0) {
            if (mkdir(normalizedPath.c_str(), 0777) != 0 && errno != EEXIST) {
                 cerr << "Error creating directory (final): " << normalizedPath << " (Error: " << strerror(errno) << ")" << endl;
            }
        } else if (!S_ISDIR(info.st_mode)) {
            cerr << "Error: Path exists but is not a directory: " << normalizedPath << endl;
        }
    #endif
}

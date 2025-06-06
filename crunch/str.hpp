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

#ifndef str_hpp
#define str_hpp

#include <string>
using namespace std;

#if defined _MSC_VER || defined __MINGW32__
#include <locale>
#include <codecvt>
wstring StrToPath(const string& str);
string PathToStr(const wstring& str);
#else
const string& StrToPath(const string& str);
const string& PathToStr(const string& str);
#endif

// Normalizes a path, converting to absolute and resolving . and ..
string NormalizePath(const string& path);

// Ensures the specified directory path exists, creating it if necessary (including parent directories)
void EnsureDirectoryExists(const string& path);

#endif

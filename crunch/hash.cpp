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

#include "hash.hpp"
#include <fstream>
#include <vector>
#include <iostream>
#include <sstream>
#include "tinydir.h"
#include "str.hpp"

template <class T>
void HashCombine(std::size_t& hash, const T& v)
{
    std::hash<T> hasher;
    hash ^= hasher(v) + 0x9e3779b9 + (hash<<6) + (hash>>2);
}
void HashCombine(std::size_t& hash, size_t v)
{
    hash ^= v + 0x9e3779b9 + (hash<<6) + (hash>>2);
}

void HashString(size_t& hash, const string& str)
{
    HashCombine(hash, str);
}

void HashFile(size_t& hash, const string& file)
{
    ifstream stream(file, ios::binary | ios::ate);
    streamsize size = stream.tellg();
    stream.seekg(0, ios::beg);
    vector<char> buffer(size + 1);
    if (!stream.read(buffer.data(), size))
    {
        cerr << "failed to read file: " << file << endl;
        exit(EXIT_FAILURE);
    }
    buffer[size] = '\0';
    string text(buffer.begin(), buffer.end());
    HashCombine(hash, text);
}

void HashFiles(size_t& hash, const string& root)
{
    static string dot1 = ".";
    static string dot2 = "..";
    
    tinydir_dir dir;
    // root is expected to be a normalized std::string.
    // tinydir_open on Windows (with UNICODE) expects const wchar_t*.
    if (tinydir_open(&dir, StrToPath(root).c_str()) == -1) {
        cerr << "Error opening directory for hashing: " << root << endl;
        return;
    }
    
    while (dir.has_next)
    {
        tinydir_file file;
        tinydir_readfile(&dir, &file);
        
        // file.name, file.path, file.extension are TCHAR[] (wchar_t[] on Windows UNICODE)
        // Convert them back to std::string using PathToStr
        string current_file_name = PathToStr(file.name);
        string current_file_path = PathToStr(file.path);
        string current_file_ext = PathToStr(file.extension);


        if (file.is_dir)
        {
            if (dot1 != current_file_name && dot2 != current_file_name)
                HashFiles(hash, current_file_path); // current_file_path is now std::string
        }
        else if (current_file_ext == "png") // PathToStr(file.extension) gives "png" not ".png"
            HashFile(hash, current_file_path);
        
        tinydir_next(&dir);
    }
    
    tinydir_close(&dir);
}

void HashData(size_t& hash, const char* data, size_t size)
{
    string str(data, size);
    HashCombine(hash, str);
}

bool LoadHash(size_t& hash, const string& file)
{
    ifstream stream(file);
    if (stream)
    {
        stringstream ss;
        ss << stream.rdbuf();
        ss >> hash;
        return true;
    }
    return false;
}

void SaveHash(size_t hash, const string& file)
{
    ofstream stream(file);
    stream << hash;
}

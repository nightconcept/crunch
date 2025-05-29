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
 
 crunch - command line texture packer
 ====================================
 
 usage:
    crunch -o <OUTPUT_PREFIX> -i <INPUT_DIR1,INPUT_DIR2,...> [OPTIONS...]
 
 example:
    crunch -o bin/atlases/atlas -i assets/characters,assets/tiles -p -t -v -u -r
 
 options:
    -d  --default           use default settings (-x -p -t -u)
    -x  --xml               saves the atlas data as a .xml file
    -b  --binary            saves the atlas data as a .bin file
    -j  --json              saves the atlas data as a .json file
    -p  --premultiply       premultiplies the pixels of the bitmaps by their alpha channel
    -t  --trim              trims excess transparency off the bitmaps
    -v  --verbose           print to the debug console as the packer works
    -f  --force             ignore the hash, forcing the packer to repack
    -u  --unique            remove duplicate bitmaps from the atlas
    -r  --rotate            enabled rotating bitmaps 90 degrees clockwise when packing
    -s# --size#             max atlas size (# can be 4096, 2048, 1024, 512, 256, 128, or 64)
    -p# --pad#              padding between images (# can be from 0 to 16)
 
 binary format:
    [int16] num_textures (below block is repeated this many times)
        [string] name
        [int16] num_images (below block is repeated this many times)
            [string] img_name
            [int16] img_x
            [int16] img_y
            [int16] img_width
            [int16] img_height
            [int16] img_frame_x         (if --trim enabled)
            [int16] img_frame_y         (if --trim enabled)
            [int16] img_frame_width     (if --trim enabled)
            [int16] img_frame_height    (if --trim enabled)
            [byte] img_rotated          (if --rotate enabled)
 */

#include <iostream>
#include <fstream>
#include <streambuf>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include "tinydir.h"
#include "bitmap.hpp"
#include "packer.hpp"
#include "binary.hpp"
#include "hash.hpp"
#include "str.hpp"

using namespace std;

static int optSize;
static int optPadding;
static bool optXml;
static bool optBinary;
static bool optJson;
static bool optPremultiply;
static bool optTrim;
static bool optVerbose;
static bool optForce;
static bool optUnique;
static bool optRotate;
static vector<Bitmap*> bitmaps;
static vector<Packer*> packers;

static void SplitFileName(const string& path, string* dir, string* name, string* ext)
{
    size_t si = path.rfind('/') + 1;
    if (si == string::npos)
        si = 0;
    size_t di = path.rfind('.');
    if (dir != nullptr)
    {
        if (si > 0)
            *dir = path.substr(0, si);
        else
            *dir = "";
    }
    if (name != nullptr)
    {
        if (di != string::npos)
            *name = path.substr(si, di - si);
        else
            *name = path.substr(si);
    }
    if (ext != nullptr)
    {
        if (di != string::npos)
            *ext = path.substr(di);
        else
            *ext = "";
    }
}

static string GetFileName(const string& path)
{
    string name;
    SplitFileName(path, nullptr, &name, nullptr);
    return name;
}

static void LoadBitmap(const string& prefix, const string& path)
{
    if (optVerbose)
        cout << '\t' << path << endl;
    
    bitmaps.push_back(new Bitmap(path, prefix + GetFileName(path), optPremultiply, optTrim));
}

static void LoadBitmaps(const string& root, const string& prefix)
{
    static string dot1 = ".";
    static string dot2 = "..";
    
    tinydir_dir dir;
    // root is expected to be a normalized std::string.
    // tinydir_open on Windows (with UNICODE) expects const wchar_t*.
    if (tinydir_open(&dir, StrToPath(root).c_str()) == -1) {
        cerr << "Error opening directory: " << root << endl;
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
                LoadBitmaps(current_file_path, prefix + current_file_name + "/");
        }
        else if (current_file_ext == "png") // PathToStr(file.extension) gives "png"
            LoadBitmap(prefix, current_file_path);
        
        tinydir_next(&dir);
    }
    
    tinydir_close(&dir);
}

static void RemoveFile(string file)
{
    remove(file.data());
}

static int GetPackSize(const string& str)
{
    if (str == "4096")
        return 4096;
    if (str == "2048")
        return 2048;
    if (str == "1024")
        return 1024;
    if (str == "512")
        return 512;
    if (str == "256")
        return 256;
    if (str == "128")
        return 128;
    if (str == "64")
        return 64;
    cerr << "invalid size: " << str << endl;
    exit(EXIT_FAILURE);
    return 0;
}

static int GetPadding(const string& str)
{
    for (int i = 0; i <= 16; ++i)
        if (str == to_string(i))
            return i;
    cerr << "invalid padding value: " << str << endl;
    exit(EXIT_FAILURE);
    return 1;
}

int main(int argc, const char* argv[])
{
    //Print out passed arguments
    for (int i = 0; i < argc; ++i)
        cout << argv[i] << ' ';
    cout << endl;

    string rawOutputPathStr; // Store the raw path first
    string rawInputPathStr;  // Store the raw path first
    vector<string> cli_options;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "-o") {
            if (i + 1 < argc) {
                rawOutputPathStr = argv[++i];
            } else {
                cerr << "Error: -o option requires a value." << endl;
                // Usage string will be printed by the later check
                return EXIT_FAILURE;
            }
        } else if (arg == "-i") {
            if (i + 1 < argc) {
                rawInputPathStr = argv[++i];
            } else {
                cerr << "Error: -i option requires a value." << endl;
                // Usage string will be printed by the later check
                return EXIT_FAILURE;
            }
        } else {
            cli_options.push_back(arg);
        }
    }

    string usage_string = "usage:\n   crunch -o <OUTPUT_PREFIX> -i <INPUT_DIR1,INPUT_DIR2,...> [OPTIONS...]\n\nexample:\n   crunch -o bin/atlases/atlas -i assets/characters,assets/tiles -p -t -v -u -r\n\noptions:\n   -d  --default           use default settings (-x -p -t -u)\n   -x  --xml               saves the atlas data as a .xml file\n   -b  --binary            saves the atlas data as a .bin file\n   -j  --json              saves the atlas data as a .json file\n   -p  --premultiply       premultiplies the pixels of the bitmaps by their alpha channel\n   -t  --trim              trims excess transparency off the bitmaps\n   -v  --verbose           print to the debug console as the packer works\n   -f  --force             ignore the hash, forcing the packer to repack\n   -u  --unique            remove duplicate bitmaps from the atlas\n   -r  --rotate            enabled rotating bitmaps 90 degrees clockwise when packing\n   -s# --size#             max atlas size (# can be 4096, 2048, 1024, 512, 256, 128, or 64)\n   -p# --pad#              padding between images (# can be from 0 to 16)";

    if (rawOutputPathStr.empty() || rawInputPathStr.empty()) { // Check raw paths
        cerr << "Error: Both -o (output prefix) and -i (input directories) arguments are required." << endl;
        cerr << usage_string << endl;
        return EXIT_FAILURE;
    }

    string outputPathStr = NormalizePath(rawOutputPathStr); // Normalize after check

    //Get the output directory and name
    string outputDir, name;
    SplitFileName(outputPathStr, &outputDir, &name, nullptr); // Use normalized outputPathStr
    if (!outputDir.empty() && outputDir.back() != '/') {
        outputDir += '/';
    }
    
    //Get all the input files and directories
    vector<string> inputs;
    stringstream ss(rawInputPathStr); // Parse the raw input string for inputs
    while (ss.good())
    {
        string inputStrItem;
        getline(ss, inputStrItem, ',');
        if (!inputStrItem.empty()) {
            string normalizedInput = NormalizePath(inputStrItem);
            // Ensure input directories end with a slash for consistency.
            // tinydir_open might be fine without it, but LoadBitmaps and HashFiles might expect it.
            if (!normalizedInput.empty() && normalizedInput.back() != '/') {
                normalizedInput += '/';
            }
            inputs.push_back(normalizedInput);
        }
    }

    if (inputs.empty()) {
        cerr << "Error: No input directories specified with -i value." << endl;
        cerr << usage_string << endl;
        return EXIT_FAILURE;
    }
    
    //Get the options
    optSize = 4096;
    optPadding = 1;
    optXml = false;
    optBinary = false;
    optJson = false;
    optPremultiply = false;
    optTrim = false;
    optVerbose = false;
    optForce = false;
    optUnique = false;
    for (const string& arg : cli_options)
    {
        if (arg == "-d" || arg == "--default")
            optXml = optPremultiply = optTrim = optUnique = true;
        else if (arg == "-x" || arg == "--xml")
            optXml = true;
        else if (arg == "-b" || arg == "--binary")
            optBinary = true;
        else if (arg == "-j" || arg == "--json")
            optJson = true;
        else if (arg == "-p" || arg == "--premultiply")
            optPremultiply = true;
        else if (arg == "-t" || arg == "--trim")
            optTrim = true;
        else if (arg == "-v" || arg == "--verbose")
            optVerbose = true;
        else if (arg == "-f" || arg == "--force")
            optForce = true;
        else if (arg == "-u" || arg == "--unique")
            optUnique = true;
        else if (arg == "-r" || arg == "--rotate")
            optRotate = true;
        else if (arg.find("--size") == 0)
            optSize = GetPackSize(arg.substr(6));
        else if (arg.find("-s") == 0)
            optSize = GetPackSize(arg.substr(2));
        else if (arg.find("--pad") == 0)
            optPadding = GetPadding(arg.substr(5));
        else if (arg.find("-p") == 0)
            optPadding = GetPadding(arg.substr(2));
        else
        {
            cerr << "unexpected argument: " << arg << endl;
            return EXIT_FAILURE;
        }
    }
    
    //Hash the arguments and input directories
    size_t newHash = 0;
    // Hash the canonical output and input path strings
    HashString(newHash, outputPathStr); // outputPathStr is already normalized

    // Create a single, sorted string of normalized input paths for hashing
    // This ensures the hash is consistent regardless of original input order or relative/absolute parts.
    string allNormalizedInputsForHash;
    vector<string> sorted_normalized_inputs = inputs; // 'inputs' vector now holds normalized paths
    sort(sorted_normalized_inputs.begin(), sorted_normalized_inputs.end());
    for(const string& s : sorted_normalized_inputs) {
        allNormalizedInputsForHash += s; // Concatenate them; comma was for parsing, not essential for hash string
    }
    HashString(newHash, allNormalizedInputsForHash);

    // Sort other_options to make hash order-independent for options, then hash them
    vector<string> sorted_cli_options = cli_options;
    sort(sorted_cli_options.begin(), sorted_cli_options.end());
    for (const string& opt : sorted_cli_options) {
        HashString(newHash, opt);
    }

    // Hash the content of input files/directories (this part remains the same)
    for (size_t i = 0; i < inputs.size(); ++i)
    {
        if (inputs[i].rfind('.') == string::npos)
            HashFiles(newHash, inputs[i]);
        else
            HashFile(newHash, inputs[i]);
    }
    
    //Load the old hash
    size_t oldHash;
    if (LoadHash(oldHash, outputDir + name + ".hash"))
    {
        if (!optForce && newHash == oldHash)
        {
            cout << "atlas is unchanged: " << name << endl;
            return EXIT_SUCCESS;
        }
    }
    
    /*-d  --default           use default settings (-x -p -t -u)
    -x  --xml               saves the atlas data as a .xml file
    -b  --binary            saves the atlas data as a .bin file
    -j  --json              saves the atlas data as a .json file
    -p  --premultiply       premultiplies the pixels of the bitmaps by their alpha channel
    -t  --trim              trims excess transparency off the bitmaps
    -v  --verbose           print to the debug console as the packer works
    -f  --force             ignore the hash, forcing the packer to repack
    -u  --unique            remove duplicate bitmaps from the atlas
    -r  --rotate            enabled rotating bitmaps 90 degrees clockwise when packing
    -s# --size#             max atlas size (# can be 4096, 2048, 1024, 512, or 256)
    -p# --pad#              padding between images (# can be from 0 to 16)*/
    
    if (optVerbose)
    {
        cout << "options..." << endl;
        cout << "\t--xml: " << (optXml ? "true" : "false") << endl;
        cout << "\t--binary: " << (optBinary ? "true" : "false") << endl;
        cout << "\t--json: " << (optJson ? "true" : "false") << endl;
        cout << "\t--premultiply: " << (optPremultiply ? "true" : "false") << endl;
        cout << "\t--trim: " << (optTrim ? "true" : "false") << endl;
        cout << "\t--verbose: " << (optVerbose ? "true" : "false") << endl;
        cout << "\t--force: " << (optForce ? "true" : "false") << endl;
        cout << "\t--unique: " << (optUnique ? "true" : "false") << endl;
        cout << "\t--rotate: " << (optRotate ? "true" : "false") << endl;
        cout << "\t--size: " << optSize << endl;
        cout << "\t--pad: " << optPadding << endl;
    }
    
    //Remove old files
    RemoveFile(outputDir + name + ".hash");
    RemoveFile(outputDir + name + ".bin");
    RemoveFile(outputDir + name + ".xml");
    // Remove single json file from previous run
    RemoveFile(outputDir + name + ".json");
    // Remove potentially multiple json files from previous run (e.g., atlas0.json, atlas1.json)
    for (size_t i = 0; i < 16; ++i) // Assuming max 16 pages for cleanup, same as pngs
    {
        RemoveFile(outputDir + name + to_string(i) + ".json");
        RemoveFile(outputDir + name + to_string(i) + ".png");
    }
    
    //Load the bitmaps from all the input files and directories
    if (optVerbose)
        cout << "loading images..." << endl;
    for (size_t i = 0; i < inputs.size(); ++i)
    {
        if (inputs[i].rfind('.') != string::npos)
            LoadBitmap("", inputs[i]);
        else
            LoadBitmaps(inputs[i], "");
    }
    
    //Sort the bitmaps by area
    sort(bitmaps.begin(), bitmaps.end(), [](const Bitmap* a, const Bitmap* b) {
        return (a->width * a->height) < (b->width * b->height);
    });
    
    //Pack the bitmaps
    while (!bitmaps.empty())
    {
        if (optVerbose)
            cout << "packing " << bitmaps.size() << " images..." << endl;
        auto packer = new Packer(optSize, optSize, optPadding);
        packer->Pack(bitmaps, optVerbose, optUnique, optRotate);
        packers.push_back(packer);
        if (optVerbose)
            cout << "finished packing: " << name << to_string(packers.size() - 1) << " (" << packer->width << " x " << packer->height << ')' << endl;
    
        if (packer->bitmaps.empty())
        {
            cerr << "packing failed, could not fit bitmap: " << (bitmaps.back())->name << endl;
            return EXIT_FAILURE;
        }
    }
    
    //Save the atlas image
    for (size_t i = 0; i < packers.size(); ++i)
    {
        string currentPngFileName = outputDir + name;
        if (packers.size() > 1)
        {
            currentPngFileName += to_string(i);
        }
        currentPngFileName += ".png";

        if (optVerbose)
            cout << "writing png: " << currentPngFileName << endl;
        packers[i]->SavePng(currentPngFileName);
    }
    
    //Save the atlas binary
    if (optBinary)
    {
        if (optVerbose)
            cout << "writing bin: " << outputDir << name << ".bin" << endl;
        
        ofstream bin(outputDir + name + ".bin", ios::binary);
        WriteShort(bin, (int16_t)packers.size());
        for (size_t i = 0; i < packers.size(); ++i)
            packers[i]->SaveBin(name + to_string(i), bin, optTrim, optRotate);
        bin.close();
    }
    
    //Save the atlas xml
    if (optXml)
    {
        if (optVerbose)
            cout << "writing xml: " << outputDir << name << ".xml" << endl;
        
        ofstream xml(outputDir + name + ".xml");
        xml << "<atlas>" << endl;
        for (size_t i = 0; i < packers.size(); ++i)
            packers[i]->SaveXml(name + to_string(i), xml, optTrim, optRotate);
        xml << "</atlas>";
    }
    
    //Save the atlas json
    if (optJson)
    {
        for (size_t i = 0; i < packers.size(); ++i)
        {
            string currentJsonFileName = outputDir + name;
            string currentAtlasNameBase = name;

            if (packers.size() > 1)
            {
                currentJsonFileName += to_string(i);
                currentAtlasNameBase += to_string(i);
            }
            currentJsonFileName += ".json";
            string internalAtlasName = currentAtlasNameBase + "_atlas";

            if (optVerbose)
                cout << "writing json: " << currentJsonFileName << " (Atlas Name: " << internalAtlasName << ")" << endl;
            
            ofstream jsonFile(currentJsonFileName);
            if (!jsonFile.is_open())
            {
                cerr << "Failed to open json file for writing: " << currentJsonFileName << endl;
                // Potentially skip or return EXIT_FAILURE
                continue;
            }
            packers[i]->SaveJson(internalAtlasName, jsonFile, optTrim, optRotate);
            jsonFile.close();
        }
    }
    
    //Save the new hash
    SaveHash(newHash, outputDir + name + ".hash");
    
    return EXIT_SUCCESS;
}

#include "types.hpp"
#include "3gx.hpp"
#include "ElfConvert.hpp"
#include <yaml.h>
#include "cxxopts.hpp"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <string>
#include <cstdio>
#include <cctype>

#define TOOL_VERSION "v0.0.1"
using namespace std;

static bool g_silentMode = false;
static bool g_discardSymbols = false;
static string g_author;
static string g_title;
static string g_summary;
static string g_description;
static vector<u32> g_targets;
string g_enclibpath{""};

#define MAKE_VERSION(major, minor, revision) \
    (((major) << 24)|((minor) << 16)|((revision) << 8))

void CheckOptions(int &argc, const char **argv) {
    cxxopts::Options options(argv[0], "");

    options.add_options()
        ("d,discard-symbols", "Don't include the symbols in the file")
        ("s,silent", "Don't display the text (except errors)")
        ("e,enclib", "Encryption shared library", cxxopts::value<string>())
        ("h,help", "Print help");

    auto result = options.parse(argc, argv);

    if (result.count("help")) {
      cout <<  " - Builds plugin files to be used by Luma3DS\n" \
                    "Usage:\n"
                <<  argv[0] << " [OPTION...] <input.bin> <settings.plgInfo> <output.3gx>" << endl;
      exit(0);
    }

    g_silentMode = result.count("silent");
    g_discardSymbols = result.count("discard-symbols");

    if (result.count("enclib"))
        g_enclibpath = result["enclib"].as<string>();
}

u32 GetVersion(YAML::Node &settings) {
    u32 major = 0, minor = 0, revision = 0;

    if (settings["Version"]) {
        auto map = settings["Version"];

        if (map["Major"])
            major = map["Major"].as<u32>();

        if (map["Minor"])
            minor = map["Minor"].as<u32>();

        if (map["Revision"])
            revision = map["Revision"].as<u32>();
    }

    return MAKE_VERSION(major, minor, revision);
}

void GetInfos(YAML::Node &settings) {
    if (settings["Author"])
        g_author = settings["Author"].as<string>();

    if (settings["Title"])
        g_title = settings["Title"].as<string>();

    if (settings["Summary"])
        g_summary = settings["Summary"].as<string>();

    if (settings["Description"])
        g_description = settings["Description"].as<string>();
}

_3gx_Infos::Compatibility GetCompatibility(YAML::Node &settings) {
    if (settings["Compatibility"]) {
        string value = settings["Compatibility"].as<string>();
        transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c){ return tolower(c);});

        if (value == "console")
            return _3gx_Infos::Compatibility::CONSOLE;

        else if (value == "citra")
            return _3gx_Infos::Compatibility::CITRA;

        else if (value == "any")
            return _3gx_Infos::Compatibility::CONSOLE_CITRA;

        cout << "Invalid compatibility entry in the plugin info file." \
        "Please set the \"Compatibility\" configuration. (Possible values: \"Console\", \"Citra\", \"Any\")." \
        "Assuming compatibility mode: \"Any\"" << endl;
        return _3gx_Infos::Compatibility::CONSOLE_CITRA;
    }

    else {
        cout << "Missing compatibility entry in the plugin info file. " \
        "Please set the \"Compatibility\" configuration. (Possible values: \"Console\", \"Citra\", \"Any\"). " \
        "Assuming compatibility mode: \"Any\"" << endl;
        return _3gx_Infos::Compatibility::CONSOLE_CITRA;
    }
}

_3gx_Infos::MemorySize GetMemorySize(YAML::Node &settings) {
    if (settings["MemorySize"]) {
        string value = settings["MemorySize"].as<string>();
        transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c){ return tolower(c);});

        if (value == "2mib")
            return _3gx_Infos::MemorySize::_2MiB;

        else if (value == "5mib")
            return _3gx_Infos::MemorySize::_5MiB;

        else if (value == "10mib")
            return _3gx_Infos::MemorySize::_10MiB;

        cout << "Invalid memory size entry in the plugin info file." \
        "Please set the \"MemorySize\" configuration. (Possible values: \"2MiB\", \"5MiB\", \"10MiB\")." \
        "Assuming memory size: \"5MiB\"" << endl;
        return _3gx_Infos::MemorySize::_5MiB;
    }

    else {
        cout << "Missing memory size entry in the plugin info file." \
        "Please set the \"MemorySize\" configuration. (Possible values: \"2MiB\", \"5MiB\", \"10MiB\")." \
        "Assuming memory size: \"5MiB\"" << endl;
        return _3gx_Infos::MemorySize::_5MiB;
    }
}

bool GetEventsSelfManaged(YAML::Node &settings) {
    if (settings["EventsSelfManaged"]) {
        string value = settings["EventsSelfManaged"].as<string>();
        transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c){ return tolower(c);});

        return value == "true";
    }

    return false;
}

bool GetSwapNotNeeded(YAML::Node &settings) {
    if (settings["SwapNotNeeded"]) {
        string value = settings["SwapNotNeeded"].as<string>();
        transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c){ return tolower(c);});

        return value == "true";
    }

    return false;
}

void GetTitles(YAML::Node &settings) {
    if (settings["Targets"]) {
        auto list =  settings["Targets"];

        if (list.size() >= 1 && list[0].as<u32>() != 0) {
            for (u32 i = 0; i < list.size(); ++i)
                g_targets.push_back(list[i].as<u32>());
        }
    }
}

int main(int argc, const char **argv) {
    int ret = 0;

    try {
        CheckOptions(argc, argv);

        if (!g_silentMode)
            cout   <<  "\n" \
                            "3DS Game eXtension Tool " TOOL_VERSION "\n" \
                            "--------------------------\n\n";

        if (argc < 4) {
            if (!g_silentMode)
                cout   <<  " - Builds plugin files to be used by Luma3DS\n" \
                                "Usage:\n"
                            <<  argv[0] << " [OPTION...] <input.bin> <settings.plgInfo> <output.3gx>" << endl;
            ret = -1;
            goto exit;
        }

        _3gx_Header header;
        ElfConvert elfConvert(argv[1]);
        YAML::Node settings;
        ifstream settingsFile;
        ifstream codeFile;
        ofstream outputFile;

        // Open files
        settingsFile.open(argv[2], ios::in);
        outputFile.open(argv[3], ios::out | ios::trunc | ios::binary);

        if (!settingsFile.is_open()) {
            cerr << "couldn't open: " << argv[2] << endl;
            ret = -1;
            goto exit;
        }

        if (!outputFile.is_open()) {
            cerr << "couldn't open: " << argv[3] << endl;
            ret = -1;
            goto exit;
        }

        if (!g_silentMode)
            cout << "Processing settings..." << endl;

        // Parse yaml
        settings = YAML::Load(settingsFile);

        // Fetch version
        header.version = GetVersion(settings);

        // Fetch Infos
        GetInfos(settings);

        // Fetch titles
        GetTitles(settings);

        // Fetch compatibility and memory size
        header.infos.compatibility = static_cast<u32>(GetCompatibility(settings));
        header.infos.memoryRegionSize = static_cast<u32>(GetMemorySize(settings));
        header.infos.eventsSelfManaged = static_cast<u32>(GetEventsSelfManaged(settings));
        header.infos.swapNotNeeded = static_cast<u32>(GetSwapNotNeeded(settings));

        if (!g_silentMode)
            cout << "Creating file..." << endl;

        // Reserve header place
        outputFile.write((const char *)&header, sizeof(_3gx_Header));
        outputFile.flush();

        if (!g_title.empty()) {
            header.infos.titleLen = g_title.size() + 1;
            header.infos.titleMsg = (u32)outputFile.tellp();
            outputFile << g_title << '\0';
            outputFile.flush();
        }

        if (!g_author.empty()) {
            header.infos.authorLen = g_author.size() + 1;
            header.infos.authorMsg = (u32)outputFile.tellp();
            outputFile << g_author << '\0';
            outputFile.flush();
        }

        if (!g_summary.empty()) {
            header.infos.summaryLen = g_summary.size() + 1;
            header.infos.summaryMsg = (u32)outputFile.tellp();
            outputFile << g_summary << '\0';
            outputFile.flush();
        }

        if (!g_description.empty()) {
            header.infos.descriptionLen = g_description.size() + 1;
            header.infos.descriptionMsg = (u32)outputFile.tellp();
            outputFile << g_description << '\0';
            outputFile.flush();
        }

        if (!g_targets.empty()) {
            header.targets.count = g_targets.size();
            header.targets.titles = (u32)outputFile.tellp();
            outputFile.write((const char *)g_targets.data(), 4 * g_targets.size());
            outputFile.flush();
        }

        elfConvert.WriteToFile(header, outputFile, !g_discardSymbols);

        // Write updated header to file
        outputFile.seekp(0, ios::beg);
        outputFile.write((const char *)&header, sizeof(_3gx_Header));
        outputFile.flush();
        codeFile.close();
        outputFile.close();

        if (!g_silentMode)
            cout << "Done" << endl;
    }

    catch (exception &e) {
        remove(argv[3]);
        cerr << "An exception occured: " << e.what() << endl;
        ret = -1;
        goto exit;
    }

    exit:
    return ret;
}
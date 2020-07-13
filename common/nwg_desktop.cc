/*
 * Desktop file wrangling for nwg-launchers
 * Copyright (c) 2020 Érico Nogueira
 * e-mail: ericonr@disroot.org
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <algorithm>
#include <fstream>
#include <iostream>

#include "nwg_namespaces.h"
#include "nwg_tools.h"
#include "nwg_desktop.h"

/*
 * Returns locations of .desktop files
 * */
static std::vector<std::string> get_app_dirs() {
    std::string homedir = getenv("HOME");
    std::vector<std::string> result = {homedir + "/.local/share/applications", "/usr/share/applications",
        "/usr/local/share/applications"};

    auto xdg_data_dirs = getenv("XDG_DATA_DIRS");
    if (xdg_data_dirs != NULL) {
        std::vector<std::string> dirs = split_string(xdg_data_dirs, ":");
        for (std::string dir : dirs) {
            result.push_back(dir);
        }
    }
    return result;
}

/*
 * Returns all .desktop files paths
 * */
static std::vector<std::string> list_entries(std::vector<std::string> paths) {
    std::vector<std::string> desktop_paths;
    for (std::string dir : paths) {
        struct stat st;
        char* c = const_cast<char*>(dir.c_str());
        // if directory exists
        if (stat(c, &st) == 0) {
            for (const auto & entry : fs::directory_iterator(dir)) {
                desktop_paths.push_back(entry.path());
            }
        }
    }
    return desktop_paths;
}

/*
 * Parses .desktop file to vector<string> {'name', 'exec', 'icon', 'comment'}
 * */
static std::vector<std::string> desktop_entry(std::string path, std::string lang) {
    std::vector<std::string> fields = {"", "", "", ""};

    std::ifstream file(path);
    std::string str;

    std::string name {""};          // Name=
    std::string name_ln {""};       // localized: Name[ln]=
    std::string loc_name = "Name[" + lang + "]=";

    std::string comment {""};       // Comment=
    std::string comment_ln {""};    // localized: Comment[ln]=
    std::string loc_comment = "Comment[" + lang + "]=";

    while (std::getline(file, str)) {
        bool read_me = true;
        if (str.find("[") == 0) {
            read_me = (str.find("[Desktop Entry") != std::string::npos);
            if (!read_me) {
            break;
        } else {
            continue;
            }
        }
        if (read_me) {
            if (str.find(loc_name) == 0) {
                if (str.find_first_of("=") != std::string::npos) {
                    int idx = str.find_first_of("=");
                    std::string val = str.substr(idx + 1);
                    name_ln = val;
                }
            }
            if (str.find("Name=") == 0) {
                if (str.find_first_of("=") != std::string::npos) {
                    int idx = str.find_first_of("=");
                    std::string val = str.substr(idx + 1);
                    name = val;
                }
            }
            if (str.find("Exec=") == 0) {
                if (str.find_first_of("=") != std::string::npos) {
                    int idx = str.find_first_of("=");
                    std::string val = str.substr(idx + 1);
                    // strip ' %' and following
                    if (val.find_first_of("%") != std::string::npos) {
                        int idx = val.find_first_of("%");
                        val = val.substr(0, idx - 1);
                    }
                    fields[1] = val;
                }
            }
            if (str.find("Icon=") == 0) {
                if (str.find_first_of("=") != std::string::npos) {
                    int idx = str.find_first_of("=");
                    std::string val = str.substr(idx + 1);
                    fields[2] = val;
                }
            }
            if (str.find("Comment=") == 0) {
                if (str.find_first_of("=") != std::string::npos) {
                    int idx = str.find_first_of("=");
                    std::string val = str.substr(idx + 1);
                    comment = val;
                }
            }
            if (str.find(loc_comment) == 0) {
                if (str.find_first_of("=") != std::string::npos) {
                    int idx = str.find_first_of("=");
                    std::string val = str.substr(idx + 1);
                    comment_ln = val;
                }
            }
        }
    }
    if (name_ln.empty()) {
        fields[0] = name;
    } else {
        fields[0] = name_ln;
    }

    if (comment_ln.empty()) {
        fields[3] = comment;
    } else {
        fields[3] = comment_ln;
    }
    return fields;
}

std::vector<DesktopEntry> create_desktop_entries(std::string lang) {
    /* get all applications dirs */
    std::vector<std::string> app_dirs = get_app_dirs();

    /* get a list of paths to all *.desktop entries */
    std::vector<std::string> entries = list_entries(app_dirs);
    std::cout << entries.size() << " .desktop entries found\n";

    /* create the vector of DesktopEntry structs */
    std::vector<DesktopEntry> desktop_entries {};
    for (std::string entry : entries) {
        // string path -> vector<string> {name, exec, icon, comment}
        std::vector<std::string> e = desktop_entry(entry, lang);
        struct DesktopEntry de = {e[0], e[1], e[2], e[3]};

        // only add if 'name' and 'exec' not empty
        if (!e[0].empty() && !e[1].empty()) {
            // avoid adding duplicates
            bool found = false;
            for (DesktopEntry entry : desktop_entries) {
                if (entry.name == de.name && entry.exec == de.exec) {
                    found = true;
                }
            }
            if (!found) {
                desktop_entries.push_back(de);
            }
        }
    }

    /* sort above by the 'name' field */
    std::sort(desktop_entries.begin(), desktop_entries.end(), [](const DesktopEntry& lhs, const DesktopEntry& rhs) {
        return lhs.name < rhs.name;
    });

    return desktop_entries;
}

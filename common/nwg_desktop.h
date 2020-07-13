/*
 * Desktop file wrangling for nwg-launchers
 * Copyright (c) 2020 Érico Nogueira
 * e-mail: ericonr@disroot.org
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#pragma once

#include <string>
#include <vector>

struct DesktopEntry {
    std::string name;
    std::string exec;
    std::string icon;
    std::string comment;
};

std::vector<DesktopEntry> create_desktop_entries(std::string);

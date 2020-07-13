/* GTK-based application grid
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include "nwg_tools.h"
#include "grid.h"

/*
 * Returns cache file path
 * */
std::string get_cache_path() {
    std::string s = "";
    char* val = getenv("XDG_CACHE_HOME");
    if (val) {
        s = val;
    } else {
        char* val = getenv("HOME");
        s = val;
        s += "/.cache";
    }
    fs::path dir (s);
    fs::path file ("nwg-fav-cache");
    fs::path full_path = dir / file;

    return full_path;
}

/*
 * Returns pinned cache file path
 * */
std::string get_pinned_path() {
    std::string s = "";
    char* val = getenv("XDG_CACHE_HOME");
    if (val) {
        s = val;
    } else {
        char* val = getenv("HOME");
        s = val;
        s += "/.cache";
    }
    fs::path dir (s);
    fs::path file ("nwg-pin-cache");
    fs::path full_path = dir / file;

    return full_path;
}

/*
 * Adds pinned entry and saves pinned cache file
 * */
void add_and_save_pinned(std::string command) {
    // Add if not yet pinned
    if (std::find(pinned.begin(), pinned.end(), command) == pinned.end()) {
        pinned.push_back(command);
        std::ofstream out_file(pinned_file);
        for (const auto &e : pinned) out_file << e << "\n";
    }
}

/*
 * Removes pinned entry and saves pinned cache file
 * */
void remove_and_save_pinned(std::string command) {
    // Find the item index
    bool found = false;
    int idx (-1);
    for (int i = 0; i < (int)pinned.size(); i++) {
        if (pinned[i] == command) {
            found = true;
            idx = i;
            break;
        }
    }

    if (found) {
        pinned.erase(pinned.begin() + idx);
        std::ofstream out_file(pinned_file);
        for (const auto &e : pinned) out_file << e << "\n";
    }
}

/*
 * Returns json object out of the cache file
 * */
ns::json get_cache(std::string cache_file) {
    std::string cache_string = read_file_to_string(cache_file);

    return string_to_json(cache_string);
}

/*
 * Returns vector of strings out of the pinned cache file content
 * */
std::vector<std::string> get_pinned(std::string pinned_file) {
    std::vector<std::string> lines;
    std::ifstream in(pinned_file.c_str());
    if(!in) {
        std::cerr << "Could not find " << pinned_file << ", creating!" << std::endl;
        save_string_to_file("", pinned_file);
        return lines;
    }
    std::string str;

    while (std::getline(in, str)) {
        // add non-empty lines to the vector
        if(str.size() > 0) {
            lines.push_back(str);
        }
    }
    in.close();
    return lines;
 }

/*
 * Returns n cache items sorted by clicks; n should be the number of grid columns
 * */
std::vector<CacheEntry> get_favourites(ns::json cache, int number) {
    // read from json object
    std::vector<CacheEntry> sorted_cache {}; // not yet sorted
    for (ns::json::iterator it = cache.begin(); it != cache.end(); ++it) {
        struct CacheEntry entry = {it.key(), it.value()};
        sorted_cache.push_back(entry);
    }
    // actually sort by the number of clicks
    sort(sorted_cache.begin(), sorted_cache.end(), [](const CacheEntry& lhs, const CacheEntry& rhs) {
        return lhs.clicks > rhs.clicks;
    });
    // Trim to the number of columns, as we need just 1 row of favourites
    std::vector<CacheEntry>::const_iterator first = sorted_cache.begin();
    std::vector<CacheEntry>::const_iterator last = sorted_cache.begin() + number;
    std::vector<CacheEntry> favourites(first, last);
    return favourites;
}

/*
 * Returns Gtk::Image out of the icon name of file path
 * */
Gtk::Image* app_image(std::string icon) {
    Glib::RefPtr<Gtk::IconTheme> icon_theme;
    Glib::RefPtr<Gdk::Pixbuf> pixbuf;

    icon_theme = Gtk::IconTheme::get_default();

    if (icon.find_first_of("/") != 0) {
        try {
            pixbuf = icon_theme->load_icon(icon, image_size, Gtk::ICON_LOOKUP_FORCE_SIZE);
        } catch (...) {
            pixbuf = Gdk::Pixbuf::create_from_file(DATA_DIR_STR "/nwggrid/icon-missing.svg", image_size, image_size, true);
        }
    } else {
        try {
            pixbuf = Gdk::Pixbuf::create_from_file(icon, image_size, image_size, true);
        } catch (...) {
            pixbuf = Gdk::Pixbuf::create_from_file(DATA_DIR_STR "/nwggrid/icon-missing.svg", image_size, image_size, true);
        }
    }
    auto image = Gtk::manage(new Gtk::Image(pixbuf));

    return image;
}

bool on_button_entered(GdkEventCrossing *event, Glib::ustring comment) {
    description -> set_text(comment);
    return true;
}

bool on_button_focused(GdkEventFocus *event, Glib::ustring comment) {
    description -> set_text(comment);
    return true;
}

void on_button_clicked(std::string cmd) {
    int clicks = 0;
    try {
        clicks = cache[cmd];
        clicks++;
    } catch (...) {
        clicks = 1;
    }
    cache[cmd] = clicks;
    save_json(cache, cache_file);

    cmd = cmd + " &";
    const char *command = cmd.c_str();
    std::system(command);

    Gtk::Main::quit();
}

bool on_grid_button_press(GdkEventButton *event, Glib::ustring exec) {
    if (pins && event -> button == 3) {
        add_and_save_pinned(exec);
        Gtk::Main::quit();
    }
    return true;
}

bool on_pinned_button_press(GdkEventButton *event, Glib::ustring exec) {
    if (pins && event -> button == 3) {
        remove_and_save_pinned(exec);
        Gtk::Main::quit();
    }
    return true;
}

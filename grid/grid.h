/* GTK-based application grid
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/file.h>
#include <fcntl.h>

#include <iostream>
#include <fstream>
#include <filesystem>

#include <gtkmm.h>
#include <glibmm/ustring.h>

#include <nlohmann/json.hpp>

#include "nwgconfig.h"
#include "nwg_classes.h"

namespace fs = std::filesystem;
namespace ns = nlohmann;

extern bool pins;
extern std::string wm;

extern int num_col;
extern Gtk::Label *description;

extern std::string pinned_file;
extern std::vector<std::string> pinned;
extern ns::json cache;
extern std::string cache_file;

class GridBox : public AppBox {
public:
    /* name, exec, comment, pinned */
    GridBox(Glib::ustring, Glib::ustring, Glib::ustring, bool);
    bool on_button_press_event(GdkEventButton*) override;
    bool on_focus_in_event(GdkEventFocus*) override;
    void on_enter() override;
    void on_activate() override;

    bool pinned;
};

class GridSearch : public Gtk::SearchEntry {
public:
    GridSearch();
    void prepare_to_insertion();
};

class MainWindow : public CommonWindow {
    public:
        MainWindow();

        GridSearch searchbox;                   // Search apps
        Gtk::Label label_desc;                  // To display .desktop entry Comment field at the bottom
        Gtk::Grid apps_grid;                    // All application buttons grid
        Gtk::Grid favs_grid;                    // Favourites grid above
        Gtk::Grid pinned_grid;                  // Pinned entries grid above
        Gtk::Separator separator;               // between favs and all apps
        Gtk::Separator separator1;              // below pinned
        std::list<GridBox> all_boxes {};        // attached to apps_grid unfiltered view
        std::list<GridBox*> filtered_boxes {};  // attached to apps_grid filtered view
        std::list<GridBox> fav_boxes {};        // attached to favs_grid
        std::list<GridBox> pinned_boxes {};     // attached to pinned_grid

    private:
        //Override default signal handler:
        bool on_key_press_event(GdkEventKey* event) override;
        bool on_button_press_event(GdkEventButton* event) override;
        void filter_view();
        void rebuild_grid(bool filtered);
};

struct CacheEntry {
    std::string exec;
    int clicks;
    CacheEntry(std::string, int);
};

/*
 * Function declarations
 * */
std::string get_cache_path(void);
std::string get_pinned_path(void);
void add_and_save_pinned(const std::string&);
void remove_and_save_pinned(const std::string&);
std::vector<std::string> get_app_dirs(void);
std::vector<std::string> list_entries(const std::vector<std::string>&);
DesktopEntry desktop_entry(std::string&&, const std::string&);
ns::json get_cache(const std::string&);
std::vector<std::string> get_pinned(const std::string&);
std::vector<CacheEntry> get_favourites(ns::json&&, int);

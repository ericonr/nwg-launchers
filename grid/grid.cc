/*
 * GTK-based application grid
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <sys/time.h>
#include <unistd.h>

#include <charconv>

#include "nwg_tools.h"
#include "nwg_classes.h"
#include "on_event.h"
#include "grid.h"

bool pins = false;              // whether to display pinned
RGBA background = {0.0, 0.0, 0.0, 0.9};
std::string wm {""};            // detected or forced window manager name

int num_col = 6;                // number of grid columns
Gtk::Label *description;

std::string pinned_file {};
std::vector<std::string> pinned;    // list of commands of pinned icons
ns::json cache;
std::string cache_file {};

const char* const HELP_MESSAGE =
"GTK application grid: nwggrid " VERSION_STR " (c) Piotr Miller 2020 & Contributors \n\n\
\
Options:\n\
-h               show this help message and exit\n\
-f               display favourites (most used entries)\n\
-p               display pinned entries \n\
-o <opacity>     default (black) background opacity (0.0 - 1.0, default 0.9)\n\
-b <background>  background colour in RRGGBB or RRGGBBAA format (RRGGBBAA alpha overrides <opacity>)\n\
-n <col>         number of grid columns (default: 6)\n\
-s <size>        button image size (default: 72)\n\
-c <name>        css file name (default: style.css)\n\
-l <ln>          force use of <ln> language\n\
-wm <wmname>     window manager name (if can not be detected)\n";

int main(int argc, char *argv[]) {
    bool favs (false);              // whether to display favourites
    std::string custom_css_file {"style.css"};

    struct timeval tp;
    gettimeofday(&tp, NULL);
    long int start_ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    create_pid_file_or_kill_pid("nwggrid");

    std::string lang ("");

    InputParser input(argc, argv);
    if (input.cmdOptionExists("-h")){
        std::cout << HELP_MESSAGE;
        std::exit(0);
    }
    if (input.cmdOptionExists("-f")){
        favs = true;
    }
    if (input.cmdOptionExists("-p")){
        pins = true;
    }
    auto forced_lang = input.getCmdOption("-l");
    if (!forced_lang.empty()){
        lang = forced_lang;
    }
    auto cols = input.getCmdOption("-n");
    if (!cols.empty()) {
        int n_c;
        auto [p, ec] = std::from_chars(cols.data(), cols.data() + cols.size(), n_c);
        if (ec == std::errc()) {
            if (n_c > 0 && n_c < 100) {
                num_col = n_c;
            } else {
                std::cerr << "\nERROR: Columns must be in range 1 - 99\n\n";
            }
        } else {
            std::cerr << "\nERROR: Invalid number of columns\n\n";
        }
    }

    auto css_name = input.getCmdOption("-c");
    if (!css_name.empty()){
        custom_css_file = css_name;
    }

    auto wm_name = input.getCmdOption("-wm");
    if (!wm_name.empty()){
        wm = wm_name;
    }

    auto opa = input.getCmdOption("-o");
    if (!opa.empty()) {
        try {
            auto o = std::stod(std::string{opa});
            if (o >= 0.0 && o <= 1.0) {
                background.alpha = o;
            } else {
                std::cerr << "\nERROR: Opacity must be in range 0.0 to 1.0\n\n";
            }
        } catch(...) {
            std::cerr << "\nERROR: Invalid opacity value\n\n";
        }
    }

    std::string_view bcg = input.getCmdOption("-b");
    if (!bcg.empty()) {
        set_background(bcg);
    }

    auto i_size = input.getCmdOption("-s");
    if (!i_size.empty()){
        int i_s;
        auto [p, ec] = std::from_chars(i_size.data(), i_size.data() + i_size.size(), i_s);
        if (ec == std::errc()) {
            if (i_s >= 16 && i_s <= 256) {
                image_size = i_s;
            } else {
                std::cerr << "\nERROR: Size must be in range 16 - 256\n\n";
            }
        } else {
            std::cerr << "\nERROR: Invalid image size\n\n";
        }
    }

    if (favs) {
        cache_file = get_cache_path();
        try {
            cache = get_cache(cache_file);
        }  catch (...) {
            std::cout << "Cache file not found, creating...\n";
            save_json(cache, cache_file);
        }
        if (cache.size() > 0) {
            std::cout << cache.size() << " cache entries loaded\n";
        } else {
            std::cout << "No cached favourites found\n";
            favs = false;   // ignore -f argument from now on
        }
    }

    if (pins) {
        pinned_file = get_pinned_path();
        pinned = get_pinned(pinned_file);
        if (pinned.size() > 0) {
          std::cout << pinned.size() << " pinned entries loaded\n";
        } else {
          std::cout << "No pinned entries found\n";
        }
    }

    std::string config_dir = get_config_dir("nwggrid");
    if (!fs::is_directory(config_dir)) {
        std::cout << "Config dir not found, creating...\n";
        fs::create_directories(config_dir);
    }

    // default and custom style sheet
    std::string default_css_file = config_dir + "/style.css";
    // css file to be used
    std::string css_file = config_dir + "/" + custom_css_file;
    // copy default file if not found
    if (!fs::exists(default_css_file)) {
        try {
            fs::copy_file(DATA_DIR_STR "/nwggrid/style.css", default_css_file, fs::copy_options::overwrite_existing);
        } catch (...) {
            std::cerr << "Failed copying default style.css\n";
        }
    }

    // This will be read-only, to find n most clicked items (n = number of grid columns)
    std::vector<CacheEntry> favourites {};
    if (cache.size() > 0) {
        auto n = cache.size() >= static_cast<std::size_t>(num_col) ? num_col : cache.size();
        favourites = get_favourites(std::move(cache), n);
    }

    /* get current WM name if not forced */
    if (wm.empty()) {
        wm = detect_wm();
    }
    std::cout << "WM: " << wm << "\n";

    /* get lang if not yet forced */
    if (lang.empty()) {
        lang = get_locale();
    }
    std::cout << "Locale: " << lang << "\n";

    /* get all applications dirs */
    std::vector<std::string> app_dirs = get_app_dirs();

    /* get a list of paths to all *.desktop entries */
    std::vector<std::string> entries = list_entries(app_dirs);
    std::cout << entries.size() << " .desktop entries found, ";

    /* create the vector of DesktopEntry structs */
    std::vector<DesktopEntry> desktop_entries {};
    int hidden {0};
    for (auto& entry_ : entries) {
        // string path -> DesktopEntry
        auto entry = desktop_entry(std::move(entry_), lang);
        if (entry.no_display) {
            hidden++;
        }

        // only add if 'name' and 'exec' not empty
        if (!entry.name.empty() && !entry.exec.empty()) {
            // avoid adding duplicates
            bool found = false;
            for (auto& e: desktop_entries) {
                // Checking the mime_type field should resolve #89
                if (entry.name == e.name && entry.exec == e.exec && entry.mime_type == e.mime_type) {
                    found = true;
                }
            }
            if (!found) {
                desktop_entries.emplace_back(std::move(entry));
            }
        }
    }
    std::cout << desktop_entries.size() << " unique, " << hidden << " hidden by NoDisplay=true\n";

    /* sort above by the 'name' field */
    std::sort(desktop_entries.begin(), desktop_entries.end(), [](auto& a, auto& b) { return a.name < b.name; });

    auto app = Gtk::Application::create();

    auto provider = Gtk::CssProvider::create();
    auto display = Gdk::Display::get_default();
    auto screen = display->get_default_screen();
    if (!provider || !display || !screen) {
        std::cerr << "ERROR: Failed to initialize GTK\n";
        return EXIT_FAILURE;
    }
    Gtk::StyleContext::add_provider_for_screen(screen, provider, GTK_STYLE_PROVIDER_PRIORITY_USER);
    auto icon_theme = Gtk::IconTheme::get_for_screen(screen);
    if (!icon_theme) {
        std::cerr << "ERROR: Failed to load icon theme\n";
    }
    auto& icon_theme_ref = *icon_theme.get();

    if (std::filesystem::is_regular_file(css_file)) {
        provider->load_from_path(css_file);
        std::cout << "Using " << css_file << '\n';
    } else {
        provider->load_from_path(default_css_file);
        std::cout << "Using " << default_css_file << '\n';
    }

    MainWindow window;

    window.show();

    /* Detect focused display geometry: {x, y, width, height} */
    auto geometry = display_geometry(wm, display, window.get_window());
    std::cout << "Focused display: " << geometry.x << ", " << geometry.y << ", " << geometry.width << ", "
    << geometry.height << '\n';

    int x = geometry.x;
    int y = geometry.y;
    int w = geometry.width;
    int h = geometry.height;

    /* turn off borders, enable floating on sway */
    if (wm == "sway") {
        auto* cmd = "swaymsg for_window [title=\"~nwggrid*\"] floating enable";
        std::system(cmd);
        cmd = "swaymsg for_window [title=\"~nwggrid*\"] border none";
        std::system(cmd);
    }

    if (wm == "sway" || wm == "i3" || wm == "openbox") {
        window.resize(w, h);
        window.move(x, y);
    }

    Gtk::Box outer_box(Gtk::ORIENTATION_VERTICAL);
    outer_box.set_spacing(15);

    // hbox for searchbox
    Gtk::HBox hbox_header;

    hbox_header.pack_start(window.searchbox, Gtk::PACK_EXPAND_PADDING);

    outer_box.pack_start(hbox_header, Gtk::PACK_SHRINK, Gtk::PACK_EXPAND_PADDING);

    Gtk::ScrolledWindow scrolled_window;
    scrolled_window.set_propagate_natural_height(true);
    scrolled_window.set_propagate_natural_width(true);
    scrolled_window.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_ALWAYS);

    /* Create buttons for all desktop entries */
    /* @Siborgium: We can not std::move them here, it breaks favourites: (de.exec == entry.exec) is always false */
    for (auto& entry : desktop_entries) {
        // Ignore .desktop entries with NoDisplay=true
        if (!entry.no_display) {
            if (std::find(pinned.begin(), pinned.end(), entry.exec) == pinned.end()) {
                 auto& ab = window.all_boxes.emplace_back(entry.name,
                                                          entry.exec,
                                                          entry.comment,
                                                          false);
                 Gtk::Image* image = app_image(icon_theme_ref, entry.icon);
                 ab.set_image_position(Gtk::POS_TOP);
                 ab.set_image(*image);
            }
        }
    }
    window.label_desc.set_text(std::to_string(window.all_boxes.size()));

    /* Create buttons for favourites */
    if (favs && favourites.size() > 0) {
        for (auto& entry : favourites) {
            for (auto& de : desktop_entries) {
                if (de.exec == entry.exec && !de.no_display) {

                    // avoid adding twice the same exec w/ another name
                    bool already_added {false};
                    for (auto& app_box : window.fav_boxes) {
                        if (app_box.exec == de.exec) {
                            already_added = true;
                            break;
                        }
                    }
                    if (already_added) {
                        continue;
                    }

                    auto& ab = window.fav_boxes.emplace_back(std::move(de.name),
                                                             std::move(de.exec),
                                                             std::move(de.comment),
                                                             false);

                    Gtk::Image* image = app_image(icon_theme_ref, de.icon);
                    ab.set_image_position(Gtk::POS_TOP);
                    ab.set_image(*image);
                }
            }
        }
    }

    /* Create buttons for pinned entries */
    if (pins && pinned.size() > 0) {
        for(auto& entry : desktop_entries) {
            if (!entry.no_display) {
                if (std::find(pinned.begin(), pinned.end(), entry.exec) != pinned.end()) {
                    auto& ab = window.pinned_boxes.emplace_back(std::move(entry.name),
                                                                std::move(entry.exec),
                                                                std::move(entry.comment),
                                                                true);
                    Gtk::Image* image = app_image(icon_theme_ref, entry.icon);
                    ab.set_image_position(Gtk::POS_TOP);
                    ab.set_image(*image);
                }
            }
        }
    }

    int column = 0;
    int row = 0;
    if (pins && pinned.size() > 0) {
        window.pinned_grid.freeze_child_notify();
        for (auto& box : window.pinned_boxes) {
            window.pinned_grid.attach(box, column, row, 1, 1);
            if (column < num_col - 1) {
                column++;
            } else {
                column = 0;
                row++;
            }
        }
        window.pinned_grid.thaw_child_notify();
    }

    column = 0;
    row = 0;
    if (favs && favourites.size() > 0) {
        window.favs_grid.freeze_child_notify();
        for (auto& box : window.fav_boxes) {
            window.favs_grid.attach(box, column, row, 1, 1);
            if (column < num_col - 1) {
                column++;
            } else {
                column = 0;
                row++;
            }
        }
        window.favs_grid.thaw_child_notify();
    }

    column = 0;
    row = 0;
    for (auto& box : window.all_boxes) {
        window.apps_grid.freeze_child_notify();
        window.apps_grid.attach(box, column, row, 1, 1);
        if (column < num_col - 1) {
            column++;
        } else {
            column = 0;
            row++;
        }
        window.apps_grid.thaw_child_notify();
    }

    Gtk::VBox inner_vbox;

    Gtk::HBox pinned_hbox;
    pinned_hbox.pack_start(window.pinned_grid, true, false, 0);
    inner_vbox.pack_start(pinned_hbox, false, false, 5);
    if (pins && pinned.size() > 0) {
        inner_vbox.pack_start(window.separator1, false, true, 0);
    }

    Gtk::HBox favs_hbox;
    favs_hbox.pack_start(window.favs_grid, true, false, 0);
    inner_vbox.pack_start(favs_hbox, false, false, 5);
    if (favs && favourites.size() > 0) {
        inner_vbox.pack_start(window.separator, false, true, 0);
    }

    Gtk::HBox apps_hbox;
    apps_hbox.pack_start(window.apps_grid, Gtk::PACK_EXPAND_PADDING);
    inner_vbox.pack_start(apps_hbox, true, true, 0);

    scrolled_window.add(inner_vbox);

    outer_box.pack_start(scrolled_window, Gtk::PACK_EXPAND_WIDGET);
    scrolled_window.show_all_children();

    outer_box.pack_start(window.label_desc, Gtk::PACK_SHRINK);

    window.add(outer_box);
    window.show_all_children();

    // Set keyboard focus to the first visible button
    if (favs && favourites.size() > 0) {
        auto* first = window.favs_grid.get_child_at(0, 0);
        if (first) {
            first -> grab_focus();
        }
    } else if (pins && pinned.size() > 0) {
        auto* first = window.pinned_grid.get_child_at(0, 0);
        if (first) {
            first -> grab_focus();
        }
    } else {
        auto* first = window.apps_grid.get_child_at(0, 0);
        if (first) {
            first -> grab_focus();
        }
    }

    gettimeofday(&tp, NULL);
    long int end_ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    std::cout << "Time: " << end_ms - start_ms << "ms\n";

    app->run(window);

    return 0;
}

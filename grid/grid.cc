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
        favourites = get_favourites(std::move(cache), n); // TODO: compact favs on exit
    }

    /* get all applications dirs */
    auto dirs = get_app_dirs();

//    /* get a list of paths to all *.desktop entries */
//    auto entries = list_entries(app_dirs);
//    std::cout << entries.size() << " .desktop entries found, ";

    gettimeofday(&tp, NULL);
    long int commons_ms  = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    /* TODO: Provide meaningful explanation */
    std::unordered_map<std::string, std::size_t> desktop_ids;
    std::vector<std::optional<DesktopEntry>>     desktop_entries;
    std::vector<Stats>                           stats;
    std::vector<Gtk::Image*>                     images;

    auto desktop_id = [](auto& path) {
        return path.string(); // actual desktop_id requires '/' to be replaced with '-'
    };

    for (auto& dir : dirs) {
        std::error_code ec;
        auto dir_iter = fs::directory_iterator(dir, ec);
        for (auto& entry : dir_iter) {
            if (ec) {
                std::cerr << ec.message() << '\n';
                ec.clear();
                continue;
            }
            if (!entry.is_regular_file()) {
                continue;
            }
            auto& path = entry.path();
            auto&& rel_path = path.lexically_relative(dir);
            auto&& id = desktop_id(rel_path);
            if (auto result = desktop_ids.find(id); result == desktop_ids.end()) {
                auto pos = desktop_ids.size();
                desktop_ids.emplace_hint(result, id, pos);
                desktop_entries.emplace_back(desktop_entry(path, lang));
                stats.emplace_back(0, Stats::Common, Stats::Unpinned);
                images.emplace_back(nullptr);
            }
        }
    }

    for (auto& pin : pinned) {
        if (auto result = desktop_ids.find(pin); result != desktop_ids.end()) {
            stats[result->second].pinned = Stats::Pinned;
        }
    }
    for (auto& [fav, clicks] : favourites) {
        if (auto result = desktop_ids.find(fav); result != desktop_ids.end()) {
            stats[result->second].clicks   = clicks;
            stats[result->second].favorite = Stats::Favorite;
        }
    }

    gettimeofday(&tp, NULL);
    long int bs_ms  = tp.tv_sec * 1000 + tp.tv_usec / 1000;

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
    icon_theme_ref.add_resource_path(DATA_DIR_STR "/icon-missing.svg");

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
    if (wm == "sway") { // TODO: Use sway-ipc
        auto* cmd = "swaymsg for_window [title=\"~nwggrid*\"] floating enable";
        std::system(cmd);
        cmd = "swaymsg for_window [title=\"~nwggrid*\"] floating enable";
    }

    if (wm == "sway" || wm == "i3" || wm == "openbox") {
        window.resize(w, h);
        window.move(x, y);
    }

    gettimeofday(&tp, NULL);
    long int images_ms  = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    // The most expensive part
    for (std::size_t i = 0; i < desktop_entries.size(); i++) {
        if (desktop_entries[i]) {
            images[i] = app_image(icon_theme_ref, desktop_entries[i]->icon);
        }
    }

    gettimeofday(&tp, NULL);
    long int boxes_ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    for (auto& [desktop_id, pos] : desktop_ids) {
        auto& entry_ = desktop_entries[pos];
        if (entry_) {
            auto& entry = *entry_;
            auto& ab = window.emplace_box(std::move(entry.name),
                                          std::move(entry.exec),
                                          std::move(entry.comment),
                                          stats[pos]);
            ab.set_image_position(Gtk::POS_TOP);
            ab.set_image(*images[pos]);
        }
    }
    
    gettimeofday(&tp, NULL);
    long int grids_ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    window.build_grids();

    gettimeofday(&tp, NULL);
    long int end_ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    auto format = [&cout=std::cout](auto&& title, auto from, auto to) {
        cout << title << to - from << "ms\n";
    };
    format("Total: ", start_ms, end_ms);
    format("\tgrids:   ", grids_ms, end_ms);
    format("\tboxes:   ", boxes_ms, grids_ms);
    format("\timages:  ", images_ms, boxes_ms);
    format("\tbs:      ", bs_ms, images_ms);
    format("\tcommons: ", commons_ms, bs_ms);

    app->run(window);

    return 0;
}

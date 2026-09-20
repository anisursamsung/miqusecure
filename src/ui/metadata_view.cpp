#include "metadata_view.hpp"
#include "ui_components.hpp"
#include <filesystem>
#include <thread>

namespace fs = std::filesystem;
using namespace miqu;

namespace miqusecure {

CleanerView::CleanerView(const CleanerInfo& info, std::function<void()> on_changed)
    : m_info(info), m_on_changed(std::move(on_changed)) {
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));

    m_layout = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_layout->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_layout->set_padding(18, 14);

    // =========================================================================
    // 1. MASTER CLEANER HERO CARD (Windows Security / iOS Style)
    // =========================================================================
    auto auto_cfg = Config::get();
    auto hero_card = std::make_shared<CardView>();
    hero_card->set_padding(14, 12);
    hero_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    hero_card->set_margin(0, 0, 0, 14);

    auto hero_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    hero_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    auto hero_badge = ui::make_icon_badge("🧹", auto_cfg->colors.primary_container, 32, 8, 12);
    hero_row->add_view(hero_badge);

    auto hero_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    hero_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    m_status_lbl = TextViewBuilder::create()
        ->text("System & Metadata Cleaner")
        ->h2()
        ->bold(true)
        ->build();

    std::string badge_text = m_info.mat2_installed ?
        ("mat2 Anonymization Engine Ready" + (m_info.version.empty() ? "" : (" (" + m_info.version + ")"))) :
        "⚠️ mat2 Engine Not Installed";

    m_status_badge = TextViewBuilder::create()
        ->text(badge_text)
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    m_status_badge->set_margin(0, 4, 0, 0);

    hero_col->add_view(m_status_lbl);
    hero_col->add_view(m_status_badge);
    hero_row->add_view(hero_col);

    auto btn_clean_all = ButtonBuilder::create()
        ->text("⚡ Clean All Caches")
        ->flat(true)
        ->bold(true)
        ->padding(12, 6)
        ->onClick([this]() {
            if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);
            std::thread([this]() {
                std::string log;
                SecurityBackend::clean_all_caches(log);
                if (auto engine = AppEngine::instance()) {
                    engine->post([this, log]() {
                        if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Invisible);
                        if (m_result_lbl) m_result_lbl->set_text("✔ " + log);
                        if (m_on_changed) m_on_changed();
                    });
                }
            }).detach();
        })
        ->build();
    hero_row->add_view(btn_clean_all);
    hero_card->add_view(hero_row);
    m_layout->add_view(hero_card);

    // Docked 3px linear progress bar
    m_progress_bar = ProgressBarBuilder::create()
        ->style(ProgressBarStyle::Linear)
        ->indeterminate(true)
        ->trackHeight(3)
        ->build();
    m_progress_bar->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        3
    ));
    m_progress_bar->set_margin(0, 0, 0, 12);
    m_progress_bar->set_visibility(Visibility::Invisible);
    m_layout->add_view(m_progress_bar);

    // =========================================================================
    // 2. METADATA CLEANER CARD (Unified Form Card)
    // =========================================================================
    m_layout->add_view(ui::make_section_header("DOCUMENT & MEDIA SANITIZER"));

    auto meta_card = std::make_shared<CardView>();
    meta_card->set_padding(14, 10);
    meta_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    meta_card->set_margin(0, 0, 0, 14);

    auto meta_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    meta_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto m_top = std::make_shared<LinearLayout>(Orientation::Horizontal);
    m_top->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    m_top->set_margin(4, 4, 4, 8);

    auto m_badge = ui::make_icon_badge("🧼", auto_cfg->colors.surface_variant, 32, 8, 12);
    m_top->add_view(m_badge);

    auto m_text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_text_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));
    auto m_name = TextViewBuilder::create()->text("Local Metadata Anonymizer (mat2)")->bold(true)->build();
    auto m_desc = TextViewBuilder::create()
        ->text("Strips hidden GPS coordinates, camera serials, usernames, and edit histories without altering the original file.")
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    m_desc->set_margin(0, 2, 0, 0);
    m_text_col->add_view(m_name);
    m_text_col->add_view(m_desc);
    m_top->add_view(m_text_col);
    meta_col->add_view(m_top);

    // Form row: File Path input + Pick File Browse Button + Clean Button
    auto form_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    form_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    form_row->set_margin(4, 6, 4, 6);

    m_path_input = EditTextBuilder::create()
        ->hint("Path to file or click Browse...")
        ->padding(12, 8)
        ->build();
    m_path_input->set_layout_params(LayoutParams(
        0,
        static_cast<int>(LayoutDimension::WrapContent),
        1.0f
    ));
    m_path_input->set_margin(0, 0, 10, 0);
    form_row->add_view(m_path_input);

    auto btn_browse = ButtonBuilder::create()
        ->text("📂 Browse...")
        ->flat(true)
        ->padding(10, 8)
        ->onClick([this]() {
            std::string cmd = "zenity --file-selection --title=\"Select File to Clean Metadata\" 2>/dev/null";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (pipe) {
                char buffer[512];
                std::string result = "";
                while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                    result += buffer;
                }
                pclose(pipe);
                size_t last = result.find_last_not_of(" \t\n\r");
                if (last != std::string::npos) {
                    result = result.substr(0, last + 1);
                }
                if (!result.empty() && m_path_input) {
                    m_path_input->set_text(result);
                }
            }
        })
        ->build();
    btn_browse->set_margin(0, 0, 10, 0);
    form_row->add_view(btn_browse);

    auto btn_clean = ButtonBuilder::create()
        ->text("🧼 Strip Metadata")
        ->primary(true)
        ->padding(14, 8)
        ->onClick([this]() {
            if (!m_path_input) return;
            std::string file_path = m_path_input->get_text();
            if (file_path.empty() || !fs::exists(file_path)) {
                if (m_result_lbl) m_result_lbl->set_text("⚠️ Please select a valid file path first.");
                return;
            }

            if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);
            std::thread([this, file_path]() {
                std::string log;
                bool ok = SecurityBackend::clean_metadata(file_path, log);
                if (auto engine = AppEngine::instance()) {
                    engine->post([this, ok, log]() {
                        if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Invisible);
                        if (m_result_lbl) m_result_lbl->set_text(ok ? ("✔ " + log) : ("Error: " + log));
                        if (m_on_changed) m_on_changed();
                    });
                }
            }).detach();
        })
        ->build();
    form_row->add_view(btn_clean);
    meta_col->add_view(form_row);

    // Results feedback label
    m_result_lbl = TextViewBuilder::create()
        ->text("")
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    m_result_lbl->set_margin(4, 4, 4, 0);
    meta_col->add_view(m_result_lbl);

    meta_card->add_view(meta_col);
    m_layout->add_view(meta_card);

    // =========================================================================
    // 3. PRIVACY & CACHE CLEANERS (Grouped Inset Card)
    // =========================================================================
    m_layout->add_view(ui::make_section_header("SYSTEM & APP CACHE PURGE"));

    auto cache_card = std::make_shared<CardView>();
    cache_card->set_padding(14, 10);
    cache_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    cache_card->set_margin(0, 0, 0, 14);

    auto cache_list = std::make_shared<LinearLayout>(Orientation::Vertical);
    cache_list->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto make_cache_row = [this](const std::string& icon,
                                 const std::string& title,
                                 const std::string& desc,
                                 const std::string& initial_size,
                                 std::shared_ptr<TextView>& out_size_lbl,
                                 std::function<bool(std::string&)> clean_action) {
        auto cfg = Config::get();
        auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        row->set_margin(4, 6, 4, 6);

        auto badge = ui::make_icon_badge(icon, cfg->colors.surface_variant, 32, 8, 12);
        row->add_view(badge);

        auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
        col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

        auto title_r = std::make_shared<LinearLayout>(Orientation::Horizontal);
        title_r->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));

        auto title_tv = TextViewBuilder::create()->text(title)->bold(true)->build();
        title_tv->set_margin(0, 0, 8, 0);
        title_r->add_view(title_tv);

        auto pill = ui::make_status_pill(initial_size, out_size_lbl);
        title_r->add_view(pill);
        col->add_view(title_r);

        auto desc_tv = TextViewBuilder::create()
            ->text(desc)
            ->caption()
            ->muted()
            ->multiline(true)
            ->ellipsize(false)
            ->build();
        desc_tv->set_margin(0, 2, 0, 0);
        col->add_view(desc_tv);
        row->add_view(col);

        auto clean_btn = ButtonBuilder::create()
            ->text("Clean Now")
            ->flat(true)
            ->padding(10, 6)
            ->onClick([this, clean_action]() {
                if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);
                std::thread([this, clean_action]() {
                    std::string log;
                    bool ok = clean_action(log);
                    if (auto engine = AppEngine::instance()) {
                        engine->post([this, ok, log]() {
                            if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Invisible);
                            if (m_result_lbl) m_result_lbl->set_text(ok ? ("✔ " + log) : ("Error: " + log));
                            if (m_on_changed) m_on_changed();
                        });
                    }
                }).detach();
            })
            ->build();
        clean_btn->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::WrapContent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));
        clean_btn->set_margin(10, 0, 0, 0);
        row->add_view(clean_btn);

        return row;
    };

    // 1. Thumbnails
    cache_list->add_view(make_cache_row("🖼️", "Thumbnail Cache",
        "Purge image and video thumbnail preview files.",
        m_info.thumbnails_size, m_thumb_size_lbl,
        [](std::string& log) { return SecurityBackend::clean_thumbnails(log); }));

    // 2. Browser Caches
    cache_list->add_view(make_cache_row("🌐", "Browser Caches",
        "Purge Chrome, Firefox & Chromium temporary offline caches.",
        m_info.browser_cache_size, m_browser_size_lbl,
        [](std::string& log) { return SecurityBackend::clean_browser_caches(log); }));

    // 3. Trash & Temp
    cache_list->add_view(make_cache_row("🗑️", "Trash Bin & Temp",
        "Empty deleted trash files and shader cache directories.",
        m_info.trash_size, m_trash_size_lbl,
        [](std::string& log) { return SecurityBackend::clean_trash_and_temp(log); }));

    // 4. Shell History
    cache_list->add_view(make_cache_row("📜", "Shell History",
        "Clear bash, zsh & terminal command prompt histories.",
        m_info.bash_history_size, m_history_size_lbl,
        [](std::string& log) { return SecurityBackend::clean_shell_history(log); }));

    cache_card->add_view(cache_list);
    m_layout->add_view(cache_card);

    // =========================================================================
    // 4. EDUCATIONAL "WHAT IS METADATA & CACHE?" (Soft Tinted Callout)
    // =========================================================================
    auto guide_card = std::make_shared<FrameLayout>();
    guide_card->set_background_color(auto_cfg->colors.surface_variant);
    guide_card->set_corner_radius(10);
    guide_card->set_padding(14, 10);
    guide_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    guide_card->set_margin(0, 4, 0, 10);

    auto guide_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    auto g_title = TextViewBuilder::create()->text("💡 Why Clean Metadata & System Caches?")->bold(true)->build();
    auto g_desc = TextViewBuilder::create()
        ->text("When photos are taken with phones or documents are exported, hidden metadata is automatically embedded (exact GPS coordinates, camera serials, usernames, and edit timestamps). Scrubbing with mat2 creates clean files safe for public sharing.\n\nSimultaneously, browsers and desktops maintain persistent local cache files that retain browsing trails, sensitive image previews, and command logs. Periodic cleaning protects against digital forensic inspection and reclaims disk storage.")
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    g_desc->set_margin(0, 4, 0, 0);
    guide_col->add_view(g_title);
    guide_col->add_view(g_desc);
    guide_card->add_view(guide_col);
    m_layout->add_view(guide_card);

    set_content_view(m_layout);
}

void CleanerView::clean_file(const std::string& path) {
    if (!m_info.mat2_installed) {
        if (m_result_lbl) m_result_lbl->set_text("Error: mat2 is not installed. Run: sudo pacman -S mat2");
        return;
    }

    if (path.empty()) {
        if (m_result_lbl) m_result_lbl->set_text("Please enter or browse to a valid file path.");
        return;
    }

    if (!fs::exists(path)) {
        if (m_result_lbl) m_result_lbl->set_text("Error: File does not exist: " + path);
        return;
    }

    if (m_path_input) {
        m_path_input->set_text(path);
    }
    if (m_btn_clean_file) {
        m_btn_clean_file->set_text("Scrubbing...");
    }
    if (m_progress_bar) {
        m_progress_bar->set_visibility(Visibility::Visible);
    }
    if (m_result_lbl) {
        m_result_lbl->set_text("Scrubbing metadata with mat2...");
    }

    std::thread([this, path]() {
        std::string log;
        bool ok = SecurityBackend::clean_metadata(path, log);
        if (auto engine = AppEngine::instance()) {
            engine->post([this, ok, log]() {
                if (m_btn_clean_file) {
                    m_btn_clean_file->set_text("🧼 Clean Metadata");
                }
                if (m_progress_bar) {
                    m_progress_bar->set_visibility(Visibility::Invisible);
                }
                if (m_result_lbl) {
                    if (ok) {
                        m_result_lbl->set_text("✔ " + (log.empty() ? "Successfully scrubbed metadata! Created '.cleaned' copy in same folder." : log));
                    } else {
                        m_result_lbl->set_text("Result: " + log);
                    }
                }
            });
        }
    }).detach();
}

void CleanerView::update_info(const CleanerInfo& info) {
    m_info = info;

    if (m_status_badge) {
        std::string badge_text = m_info.mat2_installed ?
            ("mat2 Engine Ready" + (m_info.version.empty() ? "" : (" (" + m_info.version + ")"))) :
            "⚠️ mat2 Engine Not Installed";
        m_status_badge->set_text(badge_text);
    }

    if (m_thumb_size_lbl) m_thumb_size_lbl->set_text(m_info.thumbnails_size);
    if (m_browser_size_lbl) m_browser_size_lbl->set_text(m_info.browser_cache_size);
    if (m_trash_size_lbl) m_trash_size_lbl->set_text(m_info.trash_size);
    if (m_history_size_lbl) m_history_size_lbl->set_text(m_info.bash_history_size);
}

} // namespace miqusecure

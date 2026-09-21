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
    m_layout->set_padding(22, 18);

    // =========================================================================
    // 1. MASTER CLEANER HERO (Direct Placement)
    // =========================================================================
    auto auto_cfg = Config::get();
    auto hero_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    hero_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    hero_row->set_margin(0, 4, 0, 20);

    auto hero_badge = ui::make_icon_badge("edit-clear", auto_cfg->colors.primary_container, 34, 8, 14);
    hero_row->add_view(hero_badge);

    auto hero_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    hero_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    m_status_lbl = TextViewBuilder::create()
        ->text("System & Metadata Cleaner")
        ->h2()
        ->bold(true)
        ->build();
    m_status_lbl->set_margin(0, 0, 0, 4);

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
    m_status_badge->set_margin(0, 2, 0, 0);

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
    m_layout->add_view(hero_row);

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
    m_progress_bar->set_margin(0, 0, 0, 16);
    m_progress_bar->set_visibility(Visibility::Invisible);
    m_layout->add_view(m_progress_bar);

    // =========================================================================
    // 2. METADATA CLEANER (Direct Placement)
    // =========================================================================
    auto sec_meta_hdr = ui::make_section_header("DOCUMENT & MEDIA SANITIZER");
    sec_meta_hdr->set_margin(0, 18, 0, 12);
    m_layout->add_view(sec_meta_hdr);

    auto meta_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    meta_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    meta_col->set_margin(0, 4, 0, 20);

    auto m_top = std::make_shared<LinearLayout>(Orientation::Horizontal);
    m_top->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    m_top->set_margin(0, 2, 0, 12);

    auto m_badge = ui::make_icon_badge("edit-clear", auto_cfg->colors.surface_variant, 34, 8, 14);
    m_top->add_view(m_badge);

    auto m_text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_text_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));
    auto m_title = TextViewBuilder::create()->text("Sanitize File Metadata (mat2)")->bold(true)->build();
    auto m_desc = TextViewBuilder::create()
        ->text("Strips hidden GPS coordinates, camera serial numbers, author tags, and edit history from images, PDFs, and Office documents.")
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    m_desc->set_margin(0, 3, 0, 0);
    m_text_col->add_view(m_title);
    m_text_col->add_view(m_desc);
    m_top->add_view(m_text_col);
    meta_col->add_view(m_top);

    // File path picker/input row
    auto file_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    file_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    file_row->set_margin(0, 4, 0, 8);

    m_path_input = EditTextBuilder::create()
        ->hint("Path to file (e.g. /home/user/photo.jpg)")
        ->padding(12, 8)
        ->build();
    m_path_input->set_layout_params(LayoutParams(
        0,
        static_cast<int>(LayoutDimension::WrapContent),
        1.0f
    ));
    m_path_input->set_margin(0, 0, 12, 0);
    file_row->add_view(m_path_input);

    auto btn_clean_file = ButtonBuilder::create()
        ->text("🧹 Strip Metadata")
        ->primary(true)
        ->padding(14, 8)
        ->onClick([this]() {
            std::string path = m_path_input->get_text();
            if (!path.empty()) {
                clean_file(path);
            }
        })
        ->build();
    btn_clean_file->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    file_row->add_view(btn_clean_file);
    meta_col->add_view(file_row);

    m_result_lbl = TextViewBuilder::create()
        ->text("")
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    m_result_lbl->set_margin(0, 4, 0, 0);
    meta_col->add_view(m_result_lbl);

    m_layout->add_view(meta_col);

    // =========================================================================
    // 3. PRIVACY & CACHE CLEANERS (Direct Placement)
    // =========================================================================
    auto sec_cache_hdr = ui::make_section_header("SYSTEM & APP CACHE PURGE");
    sec_cache_hdr->set_margin(0, 18, 0, 12);
    m_layout->add_view(sec_cache_hdr);

    auto cache_list = std::make_shared<LinearLayout>(Orientation::Vertical);
    cache_list->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    cache_list->set_margin(0, 4, 0, 20);

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
        row->set_margin(0, 10, 0, 10);

        auto badge = ui::make_icon_badge(icon, cfg->colors.surface_variant, 34, 8, 14);
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
        desc_tv->set_margin(0, 3, 0, 0);
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
    cache_list->add_view(make_cache_row("folder-pictures", "Thumbnail Cache",
        "Purge image and video thumbnail preview files.",
        m_info.thumbnails_size, m_thumb_size_lbl,
        [](std::string& log) { return SecurityBackend::clean_thumbnails(log); }));

    // 2. Browser Caches
    cache_list->add_view(make_cache_row("network-workgroup", "Browser Caches",
        "Purge Chrome, Firefox & Chromium temporary offline caches.",
        m_info.browser_cache_size, m_browser_size_lbl,
        [](std::string& log) { return SecurityBackend::clean_browser_caches(log); }));

    // 3. Trash & Temp
    cache_list->add_view(make_cache_row("user-trash", "Trash Bin & Temp",
        "Empty deleted trash files and shader cache directories.",
        m_info.trash_size, m_trash_size_lbl,
        [](std::string& log) { return SecurityBackend::clean_trash_and_temp(log); }));

    // 4. Shell History
    cache_list->add_view(make_cache_row("utilities-terminal", "Shell History",
        "Clear bash, zsh & terminal command prompt histories.",
        m_info.bash_history_size, m_history_size_lbl,
        [](std::string& log) { return SecurityBackend::clean_shell_history(log); }));

    m_layout->add_view(cache_list);

    // =========================================================================
    // 4. EDUCATIONAL "WHAT IS METADATA & CACHE?" (Direct Placement Callout)
    // =========================================================================
    auto guide_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    guide_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    guide_col->set_margin(0, 18, 0, 24);

    auto g_title = TextViewBuilder::create()->text("💡 Why Clean Metadata?")->bold(true)->build();
    auto g_desc = TextViewBuilder::create()
        ->text("Photos and documents embed hidden GPS locations, device serials, and edit history. Scrubbing files and system caches removes digital trails and reclaims storage.")
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    g_desc->set_margin(0, 4, 0, 0);
    guide_col->add_view(g_title);
    guide_col->add_view(g_desc);
    m_layout->add_view(guide_col);

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

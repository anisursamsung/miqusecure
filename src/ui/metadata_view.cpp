#include "metadata_view.hpp"
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
    m_layout->set_padding(4, 2);

    // =========================================================================
    // 0. CLEANER STATUS PILL (Centered WrapContent Hero Capsule)
    // =========================================================================
    auto status_pill = std::make_shared<CardView>();
    status_pill->set_radius(26);
    status_pill->set_padding(24, 12);
    status_pill->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterHorizontal
    ));
    status_pill->set_margin(0, 4, 0, 16);

    auto status_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    status_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    auto status_icon = TextViewBuilder::create()
        ->text("🧹")
        ->h2()
        ->build();
    status_icon->set_margin(0, 0, 12, 0);
    status_row->add_view(status_icon);

    auto status_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_status_lbl = TextViewBuilder::create()
        ->text("System & Metadata Cleaner")
        ->h3()
        ->bold(true)
        ->build();
    status_col->add_view(m_status_lbl);

    std::string badge_text = m_info.mat2_installed ?
        ("mat2 Engine Ready" + (m_info.version.empty() ? "" : (" (" + m_info.version + ")"))) :
        "⚠️ mat2 Engine Not Installed";

    m_status_badge = TextViewBuilder::create()
        ->text(badge_text)
        ->caption()
        ->muted()
        ->build();
    m_status_badge->set_margin(0, 2, 0, 0);
    status_col->add_view(m_status_badge);

    status_row->add_view(status_col);
    status_pill->add_view(status_row);
    m_layout->add_view(status_pill);

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
    // 1. METADATA CLEANER CARD (Path Typer + Browse File + Clean)
    // =========================================================================
    auto meta_card = std::make_shared<CardView>();
    meta_card->set_padding(16, 14);
    meta_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    meta_card->set_margin(0, 0, 0, 16);

    auto meta_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    meta_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto meta_title = TextViewBuilder::create()
        ->text("🧼 Document & Media Metadata Anonymizer")
        ->bold(true)
        ->build();
    meta_title->set_margin(0, 0, 0, 10);
    meta_col->add_view(meta_title);

    // Form row: File Path input + Pick File Browse Button + Clean Button
    auto form_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    form_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    form_row->set_margin(0, 0, 0, 8);

    m_path_input = EditTextBuilder::create()
        ->hint("Path to file or click Browse...")
        ->padding(8, 6)
        ->build();
    m_path_input->set_layout_params(LayoutParams(
        0,
        static_cast<int>(LayoutDimension::WrapContent),
        1.0f
    ));
    m_path_input->set_margin(0, 0, 8, 0);
    form_row->add_view(m_path_input);

    m_btn_browse = ButtonBuilder::create()
        ->text("📁 Browse...")
        ->flat(true)
        ->padding(10, 6)
        ->onClick([this]() {
            if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);
            std::thread([this]() {
                FILE* pipe = popen("zenity --file-selection --title=\"Select File to Clean Metadata\" 2>/dev/null", "r");
                std::string chosen_path;
                if (pipe) {
                    char buffer[1024];
                    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                        chosen_path += buffer;
                    }
                    pclose(pipe);
                }
                while (!chosen_path.empty() && (chosen_path.back() == '\n' || chosen_path.back() == '\r')) {
                    chosen_path.pop_back();
                }

                if (auto engine = AppEngine::instance()) {
                    engine->post([this, chosen_path]() {
                        if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Invisible);
                        if (!chosen_path.empty() && m_path_input) {
                            m_path_input->set_text(chosen_path);
                            if (m_result_lbl) m_result_lbl->set_text("Selected: " + chosen_path);
                        }
                    });
                }
            }).detach();
        })
        ->build();
    m_btn_browse->set_margin(0, 0, 8, 0);
    form_row->add_view(m_btn_browse);

    m_btn_clean_file = ButtonBuilder::create()
        ->text("🧼 Clean Metadata")
        ->primary(true)
        ->padding(12, 6)
        ->onClick([this]() {
            if (m_path_input) {
                clean_file(m_path_input->get_text());
            }
        })
        ->build();
    form_row->add_view(m_btn_clean_file);
    meta_col->add_view(form_row);

    m_result_lbl = TextViewBuilder::create()
        ->text("")
        ->caption()
        ->multiline(true)
        ->build();
    m_result_lbl->set_margin(0, 0, 0, 4);
    meta_col->add_view(m_result_lbl);

    auto meta_note = TextViewBuilder::create()
        ->text("Strips hidden GPS coordinates, camera serials, usernames, and edit histories using mat2 without altering the original (creates a safe '.cleaned' file).")
        ->caption()
        ->muted()
        ->multiline(true)
        ->build();
    meta_col->add_view(meta_note);

    meta_card->add_view(meta_col);
    m_layout->add_view(meta_card);

    // =========================================================================
    // 2. BLEACHBIT-STYLE QUICK CACHE & PRIVACY CLEANERS (2x2 Grid)
    // =========================================================================
    auto bleach_hdr_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    bleach_hdr_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    bleach_hdr_row->set_margin(0, 0, 0, 8);

    auto bleach_title = TextViewBuilder::create()
        ->text("🧽 Quick Privacy & Cache Cleaner")
        ->h3()
        ->bold(true)
        ->build();
    bleach_title->set_layout_params(LayoutParams(
        0,
        static_cast<int>(LayoutDimension::WrapContent),
        1.0f
    ));
    bleach_hdr_row->add_view(bleach_title);

    auto btn_clean_all = ButtonBuilder::create()
        ->text("⚡ Clean All Caches")
        ->flat(true)
        ->bold(true)
        ->padding(10, 4)
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
    bleach_hdr_row->add_view(btn_clean_all);
    m_layout->add_view(bleach_hdr_row);

    auto make_cache_card = [this](const std::string& icon, const std::string& title,
                                  const std::string& desc,
                                  const std::string& initial_size,
                                  std::shared_ptr<TextView>& out_size_lbl,
                                  std::function<bool(std::string&)> clean_action,
                                  int margin_left, int margin_right,
                                  LayoutDimension height_dim = LayoutDimension::WrapContent) {
        auto card = std::make_shared<CardView>();
        card->set_padding(16, 14);
        card->set_layout_params(LayoutParams(
            0,
            static_cast<int>(height_dim),
            1.0f
        ));
        card->set_margin(margin_left, 0, margin_right, 0);

        auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
        col->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::MatchParent)
        ));

        // Top row: Title + Size badge
        auto top_r = std::make_shared<LinearLayout>(Orientation::Horizontal);
        top_r->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        top_r->set_margin(0, 0, 0, 6);

        auto t_tv = TextViewBuilder::create()
            ->text(icon + " " + title)
            ->bold(true)
            ->ellipsize(true)
            ->build();
        t_tv->set_layout_params(LayoutParams(
            0,
            static_cast<int>(LayoutDimension::WrapContent),
            1.0f
        ));
        t_tv->set_margin(0, 0, 6, 0);
        top_r->add_view(t_tv);

        out_size_lbl = TextViewBuilder::create()
            ->text(initial_size)
            ->caption()
            ->bold(true)
            ->build();
        top_r->add_view(out_size_lbl);
        col->add_view(top_r);

        // Description
        auto d_tv = TextViewBuilder::create()
            ->text(desc)
            ->caption()
            ->muted()
            ->multiline(true)
            ->maxLines(2)
            ->ellipsize(true)
            ->build();
        d_tv->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            0,
            1.0f
        ));
        d_tv->set_margin(0, 0, 0, 8);
        col->add_view(d_tv);

        // Clean action button
        auto clean_btn = ButtonBuilder::create()
            ->text("Clean Now")
            ->flat(true)
            ->padding(8, 4)
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
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));
        col->add_view(clean_btn);

        card->add_view(col);
        return card;
    };

    // Row 1: Thumbnails (Left) & Browser Caches (Right)
    auto cache_row1 = std::make_shared<LinearLayout>(Orientation::Horizontal);
    cache_row1->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    cache_row1->set_margin(0, 0, 0, 10);

    auto card_thumb = make_cache_card("🖼️", "Thumbnail Cache",
        "Purge image and video thumbnail preview files.",
        m_info.thumbnails_size, m_thumb_size_lbl,
        [](std::string& log) { return SecurityBackend::clean_thumbnails(log); },
        0, 6, LayoutDimension::WrapContent);
    cache_row1->add_view(card_thumb);

    auto card_browser = make_cache_card("🌐", "Browser Caches",
        "Purge Chrome, Firefox & Chromium temporary offline caches.",
        m_info.browser_cache_size, m_browser_size_lbl,
        [](std::string& log) { return SecurityBackend::clean_browser_caches(log); },
        6, 0, LayoutDimension::MatchParent);
    cache_row1->add_view(card_browser);

    m_layout->add_view(cache_row1);

    // Row 2: Trash & Temp (Left) & Shell History (Right)
    auto cache_row2 = std::make_shared<LinearLayout>(Orientation::Horizontal);
    cache_row2->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    cache_row2->set_margin(0, 0, 0, 16);

    auto card_trash = make_cache_card("🗑️", "Trash Bin & Temp",
        "Empty deleted trash files and shader cache directories.",
        m_info.trash_size, m_trash_size_lbl,
        [](std::string& log) { return SecurityBackend::clean_trash_and_temp(log); },
        0, 6, LayoutDimension::WrapContent);
    cache_row2->add_view(card_trash);

    auto card_hist = make_cache_card("📜", "Shell History",
        "Clear bash, zsh & terminal command prompt histories.",
        m_info.bash_history_size, m_history_size_lbl,
        [](std::string& log) { return SecurityBackend::clean_shell_history(log); },
        6, 0, LayoutDimension::MatchParent);
    cache_row2->add_view(card_hist);

    m_layout->add_view(cache_row2);

    // =========================================================================
    // 3. EDUCATIONAL "WHAT IS METADATA & CACHE?" (Soft Tinted Callout)
    // =========================================================================
    m_layout->add_view(DividerViewBuilder::create()->margin(0, 4, 0, 10)->build());

    auto auto_cfg = Config::get();
    auto guide_card = std::make_shared<FrameLayout>();
    guide_card->set_background_color(auto_cfg->colors.surface_variant);
    guide_card->set_corner_radius(10);
    guide_card->set_padding(14, 10);
    guide_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto guide_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    auto g_title = TextViewBuilder::create()->text("💡 Why Clean Metadata & System Caches?")->bold(true)->build();
    auto g_desc = TextViewBuilder::create()
        ->text("When photos are taken with phones or documents are exported, hidden metadata is automatically embedded (exact GPS coordinates, camera serials, usernames, and edit timestamps). Scrubbing with mat2 creates clean files safe for public sharing.\n\nSimultaneously, browsers and desktops maintain persistent local cache files that retain browsing trails, sensitive image previews, and command logs. Periodic cleaning protects against digital forensic inspection and reclaims disk storage.")
        ->caption()
        ->muted()
        ->multiline(true)
        ->build();
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

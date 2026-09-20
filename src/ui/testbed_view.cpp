#include "testbed_view.hpp"
#include "ui_components.hpp"
#include <thread>

using namespace miqu;

namespace miqusecure {

TestbedView::TestbedView(const DistroboxInfo& info, std::function<void()> on_refresh)
    : m_info(info), m_on_refresh(std::move(on_refresh)) {
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
    // 1. TOP HEADER & STATUS HERO CARD
    // =========================================================================
    auto auto_cfg = Config::get();
    auto status_card = std::make_shared<CardView>();
    status_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    status_card->set_padding(14, 12);
    status_card->set_margin(0, 0, 0, 14);

    auto status_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    status_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    auto hero_badge = ui::make_icon_badge("package-x-generic", auto_cfg->colors.primary_container, 32, 8, 12);
    status_row->add_view(hero_badge);

    auto info_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    info_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    auto title_tv = TextViewBuilder::create()
        ->text("Disposable Testing Environments")
        ->h2()
        ->bold(true)
        ->build();

    std::string st_str = (m_info.distrobox_installed && m_info.podman_installed) ?
        "Podman & Distrobox ready • Run GUI and CLI apps in isolated containers" :
        "Container engine needed • Install Podman and Distrobox to enable";
    m_status_lbl = TextViewBuilder::create()
        ->text(st_str)
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    m_status_lbl->set_margin(0, 4, 0, 0);

    info_col->add_view(title_tv);
    info_col->add_view(m_status_lbl);
    status_row->add_view(info_col);

    auto card_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    card_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    if (m_info.distrobox_installed && m_info.podman_installed) {
        m_btn_create = ButtonBuilder::create()
            ->text("+ Create Arch Testbed")
            ->primary(true)
            ->bold(true)
            ->padding(14, 8)
            ->onClick([this]() {
                if (m_btn_create) {
                    m_btn_create->set_text("⏳ Creating testbed...");
                }
                if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);
                std::thread([this]() {
                    SecurityBackend::create_testbed("testbox", "archlinux:latest");
                    if (auto engine = AppEngine::instance()) {
                        engine->post([this]() {
                            if (m_btn_create) {
                                m_btn_create->set_text("+ Create Arch Testbed");
                            }
                            if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Invisible);
                            if (m_on_refresh) m_on_refresh();
                        });
                    }
                }).detach();
            })
            ->build();
        status_row->add_view(m_btn_create);
    }

    card_col->add_view(status_row);

    // Docked linear progress bar (3px, zero layout shift)
    m_progress_bar = ProgressBarBuilder::create()
        ->style(ProgressBarStyle::Linear)
        ->indeterminate(true)
        ->trackHeight(3)
        ->build();
    m_progress_bar->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        3
    ));
    m_progress_bar->set_visibility(Visibility::Invisible);
    m_progress_bar->set_margin(0, 8, 0, 0);
    card_col->add_view(m_progress_bar);

    status_card->add_view(card_col);
    m_layout->add_view(status_card);

    // =========================================================================
    // 2. ACTIVE TESTBEDS SECTION
    // =========================================================================
    m_layout->add_view(ui::make_section_header("CONFIGURED TESTBEDS"));

    m_boxes_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_boxes_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_boxes_container->set_margin(0, 0, 0, 14);
    m_layout->add_view(m_boxes_container);

    // =========================================================================
    // 3. EDUCATIONAL CALLOUT
    // =========================================================================
    m_layout->add_view(ui::make_section_header("TESTBED USAGE GUIDE"));

    auto guide_card = std::make_shared<FrameLayout>();
    guide_card->set_background_color(auto_cfg->colors.surface_variant);
    guide_card->set_corner_radius(10);
    guide_card->set_padding(14, 10);
    guide_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    guide_card->set_margin(0, 4, 0, 10);

    auto guide_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    guide_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::Top
    ));

    auto g_badge = ui::make_icon_badge("dialog-information", auto_cfg->colors.surface_variant, 32, 8, 12);
    guide_row->add_view(g_badge);

    auto guide_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    guide_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    auto g_title = TextViewBuilder::create()->text("How to Test Apps in a Disposable Container")->bold(true)->build();
    auto g_desc = TextViewBuilder::create()
        ->text("1. Launch Terminal: Click '💻 Terminal' on an active testbed.\n2. Install Packages: Run 'sudo pacman -S <package>' inside the container.\n3. Test the App: Launch GUI applications — windows open directly on the Wayland desktop with hardware acceleration.\n4. Wipe Clean: When finished testing, click '🗑️ Destroy' to delete all container packages and files, keeping the host system completely pristine.")
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    g_desc->set_margin(0, 4, 0, 0);

    guide_col->add_view(g_title);
    guide_col->add_view(g_desc);
    guide_row->add_view(guide_col);
    guide_card->add_view(guide_row);
    m_layout->add_view(guide_card);

    set_content_view(m_layout);
    rebuild_boxes_list();
}

void TestbedView::update_info(const DistroboxInfo& info) {
    m_info = info;

    if (m_status_lbl) {
        std::string st_str = (m_info.distrobox_installed && m_info.podman_installed) ?
            "Podman & Distrobox ready • Run GUI and CLI apps in isolated containers" :
            "Container engine needed • Install Podman and Distrobox to enable";
        m_status_lbl->set_text(st_str);
    }

    rebuild_boxes_list();
}

void TestbedView::rebuild_boxes_list() {
    if (!m_boxes_container) return;
    m_boxes_container->clear_views();

    if (!m_info.distrobox_installed || !m_info.podman_installed) {
        auto auto_cfg = Config::get();
        auto inst_card = std::make_shared<FrameLayout>();
        inst_card->set_background_color(auto_cfg->colors.surface_variant);
        inst_card->set_corner_radius(10);
        inst_card->set_padding(14, 10);
        inst_card->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));
        inst_card->set_margin(0, 0, 0, 14);

        auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::Top
        ));

        auto badge = ui::make_icon_badge("package-x-generic", auto_cfg->colors.surface_variant, 32, 8, 12);
        row->add_view(badge);

        auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
        col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

        auto t = TextViewBuilder::create()->text("Install Distrobox & Podman")->bold(true)->build();
        auto d = TextViewBuilder::create()
            ->text("Distrobox allows spinning up disposable Arch Linux, Ubuntu, or Fedora environments in seconds. All GUI applications run seamlessly on the desktop while isolating packages from the host system.\n\nTo install, run in a terminal:\n  sudo pacman -S distrobox podman")
            ->caption()
            ->muted()
            ->multiline(true)
            ->ellipsize(false)
            ->build();
        d->set_margin(0, 4, 0, 0);

        col->add_view(t);
        col->add_view(d);
        row->add_view(col);
        inst_card->add_view(row);
        m_boxes_container->add_view(inst_card);
        return;
    }

    if (m_info.boxes.empty()) {
        auto auto_cfg = Config::get();
        auto empty_card = std::make_shared<CardView>();
        empty_card->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));
        empty_card->set_padding(14, 12);
        empty_card->set_margin(0, 0, 0, 14);

        auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));

        auto badge = ui::make_icon_badge("package-x-generic", auto_cfg->colors.surface_variant, 32, 8, 12);
        row->add_view(badge);

        auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
        col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

        auto t = TextViewBuilder::create()->text("No Active Testbeds")->bold(true)->build();
        auto msg = TextViewBuilder::create()
            ->text("Click '+ Create Arch Testbed' above to spin up a clean Arch Linux container for safe app testing.")
            ->caption()
            ->muted()
            ->multiline(true)
            ->ellipsize(false)
            ->build();
        msg->set_margin(0, 4, 0, 0);

        col->add_view(t);
        col->add_view(msg);
        row->add_view(col);
        empty_card->add_view(row);
        m_boxes_container->add_view(empty_card);
        return;
    }

    for (size_t i = 0; i < m_info.boxes.size(); ++i) {
        const auto& box = m_info.boxes[i];
        std::string b_name = box.name;

        auto box_card = std::make_shared<CardView>();
        box_card->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));
        box_card->set_padding(14, 10);
        box_card->set_margin(0, 0, 0, 14);

        auto box_col = std::make_shared<LinearLayout>(Orientation::Vertical);
        box_col->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));

        // ---------------------------------------------------------------------
        // Row 1: Box Identity & Actions (Terminal, Destroy)
        // ---------------------------------------------------------------------
        auto row_top = std::make_shared<LinearLayout>(Orientation::Horizontal);
        row_top->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        row_top->set_margin(4, 4, 4, 8);

        auto auto_cfg = Config::get();
        auto box_badge = ui::make_icon_badge("package-x-generic", auto_cfg->colors.surface_variant, 32, 8, 12);
        row_top->add_view(box_badge);

        auto name_col = std::make_shared<LinearLayout>(Orientation::Vertical);
        name_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

        auto name_tv = TextViewBuilder::create()->text(box.name)->h3()->bold(true)->build();
        std::string desc = box.image + " • " + (box.status.empty() ? "Ready" : box.status);
        auto desc_tv = TextViewBuilder::create()
            ->text(desc)
            ->caption()
            ->muted()
            ->multiline(true)
            ->ellipsize(false)
            ->build();
        desc_tv->set_margin(0, 2, 0, 0);

        name_col->add_view(name_tv);
        name_col->add_view(desc_tv);
        row_top->add_view(name_col);

        auto btn_term = ButtonBuilder::create()
            ->text("💻 Terminal")
            ->flat(true)
            ->bold(true)
            ->padding(10, 6)
            ->onClick([b_name]() {
                SecurityBackend::launch_testbed_terminal(b_name);
            })
            ->build();
        btn_term->set_margin(0, 0, 8, 0);
        row_top->add_view(btn_term);

        auto btn_destroy = ButtonBuilder::create()
            ->text("🗑️ Destroy")
            ->flat(true)
            ->bold(true)
            ->padding(10, 6)
            ->build();
        std::weak_ptr<Button> weak_destroy = btn_destroy;
        btn_destroy->set_on_click_listener([this, b_name, weak_destroy]() {
            if (auto b = weak_destroy.lock()) {
                b->set_text("⏳ Destroying...");
            }
            std::thread([this, b_name]() {
                SecurityBackend::destroy_testbed(b_name);
                if (auto engine = AppEngine::instance()) {
                    engine->post([this]() {
                        if (m_on_refresh) m_on_refresh();
                    });
                }
            }).detach();
        });
        row_top->add_view(btn_destroy);

        box_col->add_view(row_top);

        // ---------------------------------------------------------------------
        // Row 2: In-App Package Installer & Runner
        // ---------------------------------------------------------------------
        auto inst_hdr = TextViewBuilder::create()
            ->text("📥 Install & Test an Application")
            ->bold(true)
            ->build();
        inst_hdr->set_margin(0, 4, 0, 4);
        box_col->add_view(inst_hdr);

        auto inst_sub = TextViewBuilder::create()
            ->text("Type any Arch Linux package name to install and run without modifying the host pacman database.")
            ->caption()
            ->muted()
            ->multiline(true)
            ->ellipsize(false)
            ->build();
        inst_sub->set_margin(0, 0, 0, 6);
        box_col->add_view(inst_sub);

        auto form_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        form_row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        form_row->set_margin(0, 2, 0, 6);

        auto input_pkg = EditTextBuilder::create()
            ->hint("Package name (e.g. gimp, mpv, vlc, wireshark, htop)...")
            ->padding(12, 8)
            ->build();
        input_pkg->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));
        input_pkg->set_margin(0, 0, 10, 0);
        form_row->add_view(input_pkg);

        // Docked linear progress bar for this box (zero layout shift)
        auto box_progress = ProgressBarBuilder::create()
            ->style(ProgressBarStyle::Linear)
            ->indeterminate(true)
            ->trackHeight(3)
            ->build();
        box_progress->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            3
        ));
        box_progress->set_visibility(Visibility::Invisible);
        box_progress->set_margin(0, 6, 0, 6);

        auto btn_install = ButtonBuilder::create()
            ->text("📥 Install")
            ->primary(true)
            ->bold(true)
            ->padding(12, 8)
            ->build();
        btn_install->set_margin(0, 0, 8, 0);

        std::weak_ptr<Button> weak_install = btn_install;
        std::weak_ptr<EditText> weak_input = input_pkg;
        std::weak_ptr<ProgressBar> weak_prog = box_progress;

        btn_install->set_on_click_listener([this, b_name, weak_install, weak_input, weak_prog]() {
            auto inp = weak_input.lock();
            auto btn = weak_install.lock();
            auto prg = weak_prog.lock();
            if (!inp || !btn) return;

            std::string pkg = inp->get_text();
            size_t first = pkg.find_first_not_of(" \t\n\r");
            if (first == std::string::npos) return;
            size_t last = pkg.find_last_not_of(" \t\n\r");
            pkg = pkg.substr(first, (last - first + 1));
            if (pkg.empty()) return;

            btn->set_text("⏳ Installing...");
            if (prg) prg->set_visibility(Visibility::Visible);

            std::thread([this, b_name, pkg, weak_install, weak_input, weak_prog]() {
                std::string err;
                bool ok = SecurityBackend::install_testbed_package(b_name, pkg, err);
                if (auto engine = AppEngine::instance()) {
                    engine->post([this, ok, weak_install, weak_input, weak_prog]() {
                        if (auto p = weak_prog.lock()) {
                            p->set_visibility(Visibility::Invisible);
                        }
                        if (auto i = weak_input.lock()) {
                            if (ok) i->set_text("");
                        }
                        if (auto b = weak_install.lock()) {
                            b->set_text(ok ? "✔ Installed" : "✖ Failed");
                        }
                        if (ok && m_on_refresh) {
                            m_on_refresh();
                        }
                    });
                }
            }).detach();
        });
        form_row->add_view(btn_install);

        auto btn_run = ButtonBuilder::create()
            ->text("▶ Run")
            ->flat(true)
            ->bold(true)
            ->padding(10, 8)
            ->onClick([b_name, input_pkg]() {
                std::string app = input_pkg->get_text();
                size_t first = app.find_first_not_of(" \t\n\r");
                if (first == std::string::npos) return;
                size_t last = app.find_last_not_of(" \t\n\r");
                app = app.substr(first, (last - first + 1));
                if (!app.empty()) {
                    SecurityBackend::launch_testbed_app(b_name, app);
                }
            })
            ->build();
        form_row->add_view(btn_run);

        box_col->add_view(form_row);
        box_col->add_view(box_progress);

        // ---------------------------------------------------------------------
        // Row 3: Installed Sandbox Apps List
        // ---------------------------------------------------------------------
        auto apps_hdr = TextViewBuilder::create()
            ->text("Installed Sandbox Apps (" + std::to_string(box.installed_packages.size()) + ")")
            ->bold(true)
            ->build();
        apps_hdr->set_margin(0, 6, 0, 6);
        box_col->add_view(apps_hdr);

        if (box.installed_packages.empty()) {
            auto no_apps = TextViewBuilder::create()
                ->text("No test apps installed yet. Type a package name above to install.")
                ->caption()
                ->muted()
                ->multiline(true)
                ->ellipsize(false)
                ->build();
            no_apps->set_margin(0, 2, 0, 4);
            box_col->add_view(no_apps);
        } else {
            auto apps_list = std::make_shared<LinearLayout>(Orientation::Vertical);
            apps_list->set_layout_params(LayoutParams(
                static_cast<int>(LayoutDimension::MatchParent),
                static_cast<int>(LayoutDimension::WrapContent)
            ));

            for (size_t j = 0; j < box.installed_packages.size(); ++j) {
                const auto& pkg = box.installed_packages[j];
                std::string p_name = pkg.name;

                auto app_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
                app_row->set_layout_params(LayoutParams(
                    static_cast<int>(LayoutDimension::MatchParent),
                    static_cast<int>(LayoutDimension::WrapContent),
                    Gravity::CenterVertical
                ));
                app_row->set_margin(0, 4, 0, 4);

                auto auto_cfg = Config::get();
                auto a_badge = ui::make_icon_badge("utilities-terminal", auto_cfg->colors.surface_variant, 24, 6, 8);
                app_row->add_view(a_badge);

                auto a_col = std::make_shared<LinearLayout>(Orientation::Vertical);
                a_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

                auto a_name = TextViewBuilder::create()->text(pkg.name)->bold(true)->build();
                auto a_ver = TextViewBuilder::create()
                    ->text("Version: " + pkg.version)
                    ->caption()
                    ->muted()
                    ->multiline(true)
                    ->ellipsize(false)
                    ->build();
                a_col->add_view(a_name);
                a_col->add_view(a_ver);
                app_row->add_view(a_col);

                auto btn_launch = ButtonBuilder::create()
                    ->text("▶ Launch GUI")
                    ->flat(true)
                    ->bold(true)
                    ->padding(10, 6)
                    ->onClick([b_name, p_name]() {
                        SecurityBackend::launch_testbed_app(b_name, p_name);
                    })
                    ->build();
                btn_launch->set_margin(0, 0, 6, 0);
                app_row->add_view(btn_launch);

                auto btn_uninstall = ButtonBuilder::create()
                    ->text("✕ Remove")
                    ->flat(true)
                    ->padding(10, 6)
                    ->build();
                std::weak_ptr<Button> weak_uninst = btn_uninstall;
                btn_uninstall->set_on_click_listener([this, b_name, p_name, weak_uninst]() {
                    if (auto b = weak_uninst.lock()) {
                        b->set_text("⏳ Removing...");
                    }
                    std::thread([this, b_name, p_name]() {
                        SecurityBackend::remove_testbed_package(b_name, p_name);
                        if (auto engine = AppEngine::instance()) {
                            engine->post([this]() {
                                if (m_on_refresh) m_on_refresh();
                            });
                        }
                    }).detach();
                });
                app_row->add_view(btn_uninstall);

                apps_list->add_view(app_row);
            }
            box_col->add_view(apps_list);
        }

        box_card->add_view(box_col);
        m_boxes_container->add_view(box_card);
    }
}

} // namespace miqusecure


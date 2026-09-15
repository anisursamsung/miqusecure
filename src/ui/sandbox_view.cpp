#include "sandbox_view.hpp"

using namespace miqu;

namespace miqusecure {

SandboxView::SandboxView(const LsmInfo& info) : m_info(info) {
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
    // 1. KERNEL POSTURE HEADER (Clean Borderless Hero)
    // =========================================================================
    auto hero_row = std::make_shared<LinearLayout>(Orientation::Vertical);
    hero_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    hero_row->set_margin(0, 4, 0, 16);

    auto title = TextViewBuilder::create()->text("🛡️ Application Sandboxing & Defense")->h2()->bold(true)->build();
    title->set_margin(0, 0, 0, 2);

    std::string lsm_str = "";
    for (size_t i = 0; i < m_info.active_lsms.size(); ++i) {
        if (i > 0) lsm_str += ", ";
        lsm_str += m_info.active_lsms[i];
    }
    if (lsm_str.empty()) lsm_str = "None detected";

    m_stack_lbl = TextViewBuilder::create()
        ->text("Active Kernel Modules: " + lsm_str)
        ->caption()
        ->muted()
        ->build();

    hero_row->add_view(title);
    hero_row->add_view(m_stack_lbl);
    m_layout->add_view(hero_row);

    // =========================================================================
    // 2. FLATPAK APPS CONTAINER (OR INSTALL GUIDE IF MISSING)
    // =========================================================================
    m_flatpak_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_flatpak_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    rebuild_flatpak_section();
    m_layout->add_view(m_flatpak_container);

    // =========================================================================
    // 3. KERNEL PROTECTION SUBSYSTEMS (Unified Grouped Card with Dividers)
    // =========================================================================
    auto sec_title = TextViewBuilder::create()
        ->text("Active Kernel Defenses")
        ->h3()
        ->bold(true)
        ->build();
    sec_title->set_margin(0, 4, 0, 8);
    m_layout->add_view(sec_title);

    auto unified_card = std::make_shared<CardView>();
    unified_card->set_padding(14, 6);
    unified_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    unified_card->set_margin(0, 0, 0, 16);

    auto list_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    list_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto make_module_row = [](const std::string& icon, const std::string& name,
                              const std::string& desc, const std::string& status_txt) {
        auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        row->set_margin(4, 6, 4, 6);

        auto icon_tv = TextViewBuilder::create()->text(icon)->h2()->build();
        icon_tv->set_margin(0, 0, 14, 0);
        row->add_view(icon_tv);

        auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
        col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

        auto name_tv = TextViewBuilder::create()->text(name)->bold(true)->build();
        auto desc_tv = TextViewBuilder::create()->text(desc)->caption()->muted()->multiline(true)->maxLines(2)->ellipsize(true)->build();

        col->add_view(name_tv);
        col->add_view(desc_tv);
        row->add_view(col);

        auto st_tv = TextViewBuilder::create()
            ->text(status_txt)
            ->caption()
            ->bold(true)
            ->build();
        row->add_view(st_tv);

        return row;
    };

    // Landlock
    std::string landlock_status = m_info.landlock_active ? "✔ Active" : "○ Inactive";
    list_col->add_view(make_module_row("🏰", "Landlock LSM (Process Isolation)",
        "Allows apps to voluntarily restrict their own filesystem and internet access without requiring root passwords.",
        landlock_status));
    list_col->add_view(DividerViewBuilder::create()->margin(0, 4)->build());

    // YAMA
    std::string yama_status = m_info.yama_active ? "✔ Protected" : "○ Inactive";
    list_col->add_view(make_module_row("🔒", "YAMA Memory Guard (Anti-Spyware)",
        "Stops background malware or untrusted programs from reading browser passwords or debugging other running apps.",
        yama_status));
    list_col->add_view(DividerViewBuilder::create()->margin(0, 4)->build());

    // AppArmor
    std::string aa_status = "○ Inactive";
    if (m_info.apparmor_active) {
        aa_status = "✔ Active (" + std::to_string(m_info.enforcing_profiles) + " enforcing)";
    }
    list_col->add_view(make_module_row("🛡️", "AppArmor Profiles",
        "Mandatory system confinement profiles restricting application directories and network capabilities.",
        aa_status));

    unified_card->add_view(list_col);
    m_layout->add_view(unified_card);

    // =========================================================================
    // 4. "WHAT IS SANDBOXING?" (Soft Tinted, Borderless Callout)
    // =========================================================================
    m_layout->add_view(DividerViewBuilder::create()->margin(0, 10)->build());

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
    auto g_title = TextViewBuilder::create()->text("💡 What is App Sandboxing?")->bold(true)->build();
    auto g_desc = TextViewBuilder::create()
        ->text("Application sandboxing confines processes inside an isolated security boundary. Even if a program contains vulnerabilities or malicious code, operations are restricted to that boundary.\n\nIsolated applications cannot read personal documents, browser databases, or access audio and video hardware without explicit permission grants.")
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

void SandboxView::rebuild_flatpak_section() {
    if (!m_flatpak_container) return;
    m_flatpak_container->clear_views();

    if (!m_info.flatpak_installed) {
        auto auto_cfg = Config::get();
        auto inst_box = std::make_shared<FrameLayout>();
        inst_box->set_background_color(auto_cfg->colors.surface_variant);
        inst_box->set_corner_radius(10);
        inst_box->set_padding(14, 10);
        inst_box->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));
        inst_box->set_margin(0, 0, 0, 16);

        auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
        auto t = TextViewBuilder::create()->text("📦 Want Easy App Sandboxing?")->bold(true)->build();
        auto d = TextViewBuilder::create()
            ->text("Flatpak runs desktop applications (such as web browsers, messaging clients, and productivity suites) inside isolated Bubblewrap containers so they cannot access host files without permission.\n\nTo install Flatpak on this system, run:\n  sudo pacman -S flatpak")
            ->caption()
            ->muted()
            ->multiline(true)
            ->build();
        d->set_margin(0, 4, 0, 0);

        col->add_view(t);
        col->add_view(d);
        inst_box->add_view(col);
        m_flatpak_container->add_view(inst_box);
    } else {
        auto fp_hdr = TextViewBuilder::create()
            ->text("Sandboxed Applications (" + std::to_string(m_info.flatpak_apps.size()) + ")")
            ->h3()
            ->bold(true)
            ->build();
        fp_hdr->set_margin(0, 0, 0, 8);
        m_flatpak_container->add_view(fp_hdr);

        if (m_info.flatpak_apps.empty()) {
            auto empty_tv = TextViewBuilder::create()
                ->text("Flatpak is installed, but no sandboxed apps have been installed yet.")
                ->caption()
                ->muted()
                ->build();
            m_flatpak_container->add_view(empty_tv);
        } else {
            auto fp_card = std::make_shared<CardView>();
            fp_card->set_padding(14, 6);
            fp_card->set_layout_params(LayoutParams(
                static_cast<int>(LayoutDimension::MatchParent),
                static_cast<int>(LayoutDimension::WrapContent)
            ));
            fp_card->set_margin(0, 0, 0, 16);

            auto list_col = std::make_shared<LinearLayout>(Orientation::Vertical);
            list_col->set_layout_params(LayoutParams(
                static_cast<int>(LayoutDimension::MatchParent),
                static_cast<int>(LayoutDimension::WrapContent)
            ));

            for (size_t i = 0; i < m_info.flatpak_apps.size(); ++i) {
                const auto& app = m_info.flatpak_apps[i];
                auto a_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
                a_row->set_layout_params(LayoutParams(
                    static_cast<int>(LayoutDimension::MatchParent),
                    static_cast<int>(LayoutDimension::WrapContent),
                    Gravity::CenterVertical
                ));
                a_row->set_margin(4, 6, 4, 6);

                auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
                col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

                auto n_tv = TextViewBuilder::create()->text(app.name)->bold(true)->build();
                auto id_tv = TextViewBuilder::create()->text(app.id)->caption()->muted()->ellipsize(true)->build();
                col->add_view(n_tv);
                col->add_view(id_tv);
                a_row->add_view(col);

                auto badge = TextViewBuilder::create()->text("🛡️ Sandboxed")->caption()->bold(true)->build();
                a_row->add_view(badge);

                list_col->add_view(a_row);
                if (i + 1 < m_info.flatpak_apps.size()) {
                    list_col->add_view(DividerViewBuilder::create()->margin(0, 4)->build());
                }
            }

            fp_card->add_view(list_col);
            m_flatpak_container->add_view(fp_card);
        }
    }
}

void SandboxView::update_info(const LsmInfo& info) {
    m_info = info;

    std::string lsm_str = "";
    for (size_t i = 0; i < m_info.active_lsms.size(); ++i) {
        if (i > 0) lsm_str += ", ";
        lsm_str += m_info.active_lsms[i];
    }
    if (lsm_str.empty()) lsm_str = "None detected";

    if (m_stack_lbl) m_stack_lbl->set_text("Active Kernel Modules: " + lsm_str);

    rebuild_flatpak_section();
}

} // namespace miqusecure

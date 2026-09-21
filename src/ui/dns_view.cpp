#include "dns_view.hpp"
#include "ui_components.hpp"
#include <thread>

using namespace miqu;

namespace miqusecure {

DnsView::DnsView(const DnsInfo& info, std::function<void()> on_changed)
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
    // 1. MASTER DNS PROTECTION HERO (Direct Placement)
    // =========================================================================
    auto auto_cfg = Config::get();
    auto hero_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    hero_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    hero_row->set_margin(0, 4, 0, 20);

    auto hero_badge = ui::make_icon_badge(
        m_info.encrypted ? "network-vpn" : "dialog-warning",
        m_info.encrypted ? auto_cfg->colors.primary_container : auto_cfg->colors.surface_variant,
        34, 8, 14
    );
    hero_row->add_view(hero_badge);

    auto hero_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    hero_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    std::string title_text = m_info.encrypted ?
        (m_info.active_provider + " Protection Active") :
        "Standard Router DNS (Unencrypted)";

    m_status_lbl = TextViewBuilder::create()
        ->text(title_text)
        ->h2()
        ->bold(true)
        ->build();
    m_status_lbl->set_margin(0, 0, 0, 4);

    std::string desc_text = m_info.encrypted ?
        "Encrypted DNS lookups shield web navigation from ISP surveillance and network hijacking." :
        "DNS queries are routed unencrypted through default ISP/router nameservers.";

    m_status_desc = TextViewBuilder::create()
        ->text(desc_text)
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    m_status_desc->set_margin(0, 2, 0, 0);

    hero_col->add_view(m_status_lbl);
    hero_col->add_view(m_status_desc);
    hero_row->add_view(hero_col);

    // Master Switch on right
    m_switch_master = SwitchBuilder::create()
        ->checked(m_info.encrypted)
        ->build();
    std::weak_ptr<Switch> weak_master = m_switch_master;
    m_switch_master->set_on_checked_changed_listener([this, weak_master](bool checked) {
        if (m_updating_ui) return;
        if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);

        std::thread([this, checked, weak_master]() {
            bool ok = false;
            if (checked) {
                ok = SecurityBackend::apply_dns("9.9.9.9", "149.112.112.112");
            } else {
                ok = SecurityBackend::restore_default_dns();
            }

            if (auto engine = AppEngine::instance()) {
                engine->post([this, checked, ok, weak_master]() {
                    if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Invisible);
                    if (ok) {
                        if (m_on_changed) m_on_changed();
                    } else {
                        m_updating_ui = true;
                        if (auto s = weak_master.lock()) {
                            s->set_checked(!checked);
                        }
                        m_updating_ui = false;
                    }
                });
            }
        }).detach();
    });
    hero_row->add_view(m_switch_master);
    m_layout->add_view(hero_row);

    // Docked 3px progress bar
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
    // 2. 1-CLICK TRUSTED PRIVACY RESOLVERS (Direct Placement)
    // =========================================================================
    auto sec_res_hdr = ui::make_section_header("PRIVACY & SECURITY RESOLVERS");
    sec_res_hdr->set_margin(0, 18, 0, 12);
    m_layout->add_view(sec_res_hdr);

    auto res_list = std::make_shared<LinearLayout>(Orientation::Vertical);
    res_list->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    res_list->set_margin(0, 4, 0, 20);

    auto make_resolver_row = [this](const std::string& icon,
                                    const std::string& title,
                                    const std::string& desc,
                                    const std::string& primary_ip, const std::string& secondary_ip,
                                    bool is_active, std::shared_ptr<Switch>& out_switch) {
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

        auto title_tv = TextViewBuilder::create()->text(title)->bold(true)->build();
        auto desc_tv = TextViewBuilder::create()->text(desc)->caption()->muted()->multiline(true)->ellipsize(false)->build();
        desc_tv->set_margin(0, 2, 0, 0);

        col->add_view(title_tv);
        col->add_view(desc_tv);
        row->add_view(col);

        auto sw = SwitchBuilder::create()
            ->checked(is_active)
            ->build();
        sw->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::WrapContent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        std::weak_ptr<Switch> weak_sw = sw;
        sw->set_on_checked_changed_listener([this, primary_ip, secondary_ip, weak_sw](bool checked) {
            if (m_updating_ui) return;
            if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);

            std::thread([this, checked, primary_ip, secondary_ip, weak_sw]() {
                bool ok = false;
                if (checked) {
                    ok = SecurityBackend::apply_dns(primary_ip, secondary_ip);
                } else {
                    ok = SecurityBackend::restore_default_dns();
                }

                if (auto engine = AppEngine::instance()) {
                    engine->post([this, checked, ok, weak_sw]() {
                        if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Invisible);
                        if (ok) {
                            if (m_on_changed) m_on_changed();
                        } else {
                            m_updating_ui = true;
                            if (auto s = weak_sw.lock()) {
                                s->set_checked(!checked);
                            }
                            m_updating_ui = false;
                        }
                    });
                }
            }).detach();
        });
        out_switch = sw;
        row->add_view(out_switch);
        return row;
    };

    bool is_quad9 = m_info.active_provider.find("Quad9") != std::string::npos;
    bool is_adguard = m_info.active_provider.find("AdGuard") != std::string::npos;
    bool is_mullvad = m_info.active_provider.find("Mullvad") != std::string::npos;
    bool is_cloudflare = m_info.active_provider.find("Cloudflare") != std::string::npos;

    // 1. Quad9
    res_list->add_view(make_resolver_row("security-high", "Quad9 (Malware Block)",
        "Encrypted DNS blocking malicious phishing and malware domains (9.9.9.9).",
        "9.9.9.9", "149.112.112.112",
        is_quad9, m_switch_quad9));

    // 2. AdGuard
    res_list->add_view(make_resolver_row("dialog-cancel", "AdGuard DNS (Ad Block)",
        "System-wide sinkhole blocking advertisements and tracking telemetry (94.140.14.14).",
        "94.140.14.14", "94.140.15.15",
        is_adguard, m_switch_adguard));

    // 3. Mullvad
    res_list->add_view(make_resolver_row("network-vpn", "Mullvad DNS (No-Log)",
        "Privacy-first encrypted resolver with strict zero logging (194.242.2.3).",
        "194.242.2.3", "194.242.2.4",
        is_mullvad, m_switch_mullvad));

    // 4. Cloudflare
    res_list->add_view(make_resolver_row("network-wireless", "Cloudflare (High Speed)",
        "Ultra-low latency public resolver with privacy auditing (1.1.1.1).",
        "1.1.1.1", "1.0.0.1",
        is_cloudflare, m_switch_cloudflare));

    m_layout->add_view(res_list);

    // =========================================================================
    // 3. CUSTOM DNS SERVER (Direct Placement)
    // =========================================================================
    auto sec_custom_hdr = ui::make_section_header("CUSTOM NAMESERVERS");
    sec_custom_hdr->set_margin(0, 18, 0, 12);
    m_layout->add_view(sec_custom_hdr);

    auto custom_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    custom_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    custom_col->set_margin(0, 4, 0, 20);

    auto c_top = std::make_shared<LinearLayout>(Orientation::Horizontal);
    c_top->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    c_top->set_margin(0, 2, 0, 12);

    auto c_badge = ui::make_icon_badge("preferences-system", auto_cfg->colors.surface_variant, 34, 8, 14);
    c_top->add_view(c_badge);

    auto c_text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    c_text_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));
    auto c_name = TextViewBuilder::create()->text("Manual IPv4 Nameservers")->bold(true)->build();
    auto c_desc = TextViewBuilder::create()
        ->text("Specify custom upstream nameserver addresses to override DHCP for the active network interface.")
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    c_desc->set_margin(0, 3, 0, 0);
    c_text_col->add_view(c_name);
    c_text_col->add_view(c_desc);
    c_top->add_view(c_text_col);
    custom_col->add_view(c_top);

    // Form row: Primary & Secondary IP Inputs & Apply Button
    auto form_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    form_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    form_row->set_margin(0, 4, 0, 8);

    m_input_primary = EditTextBuilder::create()
        ->hint("Primary IP (e.g. 1.1.1.1)")
        ->padding(12, 8)
        ->build();
    m_input_primary->set_layout_params(LayoutParams(
        0,
        static_cast<int>(LayoutDimension::WrapContent),
        1.0f
    ));
    m_input_primary->set_margin(0, 0, 12, 0);
    form_row->add_view(m_input_primary);

    m_input_secondary = EditTextBuilder::create()
        ->hint("Secondary IP (optional)")
        ->padding(12, 8)
        ->build();
    m_input_secondary->set_layout_params(LayoutParams(
        0,
        static_cast<int>(LayoutDimension::WrapContent),
        1.0f
    ));
    m_input_secondary->set_margin(0, 0, 12, 0);
    form_row->add_view(m_input_secondary);

    auto btn_apply_custom = ButtonBuilder::create()
        ->text("+ Apply DNS")
        ->primary(true)
        ->padding(14, 8)
        ->onClick([this]() {
            std::string primary = m_input_primary->get_text();
            std::string secondary = m_input_secondary->get_text();
            if (!primary.empty()) {
                m_input_primary->set_text("");
                m_input_secondary->set_text("");
                if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);

                std::thread([this, primary, secondary]() {
                    bool ok = SecurityBackend::apply_dns(primary, secondary);
                    if (auto engine = AppEngine::instance()) {
                        engine->post([this, ok]() {
                            if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Invisible);
                            if (ok && m_on_changed) m_on_changed();
                        });
                    }
                }).detach();
            }
        })
        ->build();
    btn_apply_custom->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    form_row->add_view(btn_apply_custom);
    custom_col->add_view(form_row);

    auto custom_note = TextViewBuilder::create()
        ->text("Overrides default DHCP resolver for active connection " + (m_info.active_connection_name.empty() ? "" : ("(" + m_info.active_connection_name + ")")))
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    custom_note->set_margin(0, 4, 0, 0);
    custom_col->add_view(custom_note);

    m_layout->add_view(custom_col);

    // =========================================================================
    // 4. EDUCATIONAL "WHAT IS DNS?" (Direct Placement Callout)
    // =========================================================================
    auto guide_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    guide_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    guide_col->set_margin(0, 18, 0, 24);

    auto g_title = TextViewBuilder::create()->text("💡 What is DNS?")->bold(true)->build();
    auto g_desc = TextViewBuilder::create()
        ->text("Translates website names into IP addresses. Encrypted resolvers shield your queries from ISP tracking and block malicious sites.")
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

void DnsView::update_info(const DnsInfo& info) {
    m_info = info;

    if (m_status_lbl) {
        std::string title_text = m_info.encrypted ?
            ("🌐 " + m_info.active_provider + " Protection Active") :
            "🌐 Standard Router DNS (Unencrypted)";
        m_status_lbl->set_text(title_text);
    }

    if (m_status_desc) {
        std::string desc_text = m_info.encrypted ?
            "Encrypted DNS lookups shield web navigation from ISP surveillance and network hijacking." :
            "DNS queries are routed unencrypted through default ISP/router nameservers.";
        m_status_desc->set_text(desc_text);
    }

    m_updating_ui = true;

    if (m_switch_master) {
        m_switch_master->set_checked(m_info.encrypted);
    }

    bool is_quad9 = m_info.active_provider.find("Quad9") != std::string::npos;
    bool is_adguard = m_info.active_provider.find("AdGuard") != std::string::npos;
    bool is_mullvad = m_info.active_provider.find("Mullvad") != std::string::npos;
    bool is_cloudflare = m_info.active_provider.find("Cloudflare") != std::string::npos;

    if (m_switch_quad9) m_switch_quad9->set_checked(is_quad9);
    if (m_switch_adguard) m_switch_adguard->set_checked(is_adguard);
    if (m_switch_mullvad) m_switch_mullvad->set_checked(is_mullvad);
    if (m_switch_cloudflare) m_switch_cloudflare->set_checked(is_cloudflare);

    m_updating_ui = false;
}

} // namespace miqusecure

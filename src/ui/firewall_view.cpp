#include "firewall_view.hpp"
#include "ui_components.hpp"
#include <thread>

using namespace miqu;

namespace miqusecure {

FirewallView::FirewallView(const FirewallInfo& info, std::function<void()> on_rules_changed)
    : m_info(info), m_on_changed(std::move(on_rules_changed)) {
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
    // 1. MASTER FIREWALL STATUS HERO (Direct Placement with Rich Spacing)
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
        "security-high",
        m_info.active ? auto_cfg->colors.primary_container : auto_cfg->colors.surface_variant,
        42, 10, 16
    );
    hero_row->add_view(hero_badge);

    auto hero_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    hero_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    m_status_lbl = TextViewBuilder::create()
        ->text(m_info.active ? "Firewall Protection is Active" : "Firewall Protection is Disabled")
        ->h2()
        ->bold(true)
        ->build();
    m_status_lbl->set_margin(0, 0, 0, 4);

    std::string net_str = m_info.active_network_name.empty() ?
        "No active network connection detected." :
        ("Active on: " + m_info.active_network_name + (m_info.metered ? " • 📶 Metered Network" : ""));

    m_network_lbl = TextViewBuilder::create()
        ->text(net_str)
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    m_network_lbl->set_margin(0, 2, 0, 0);

    hero_col->add_view(m_status_lbl);
    hero_col->add_view(m_network_lbl);
    hero_row->add_view(hero_col);

    m_switch_master = SwitchBuilder::create()
        ->checked(m_info.active)
        ->build();
    std::weak_ptr<Switch> weak_master = m_switch_master;
    m_switch_master->set_on_checked_changed_listener([this, weak_master](bool checked) {
        if (m_updating_ui) return;
        if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);

        std::thread([this, checked, weak_master]() {
            bool ok = SecurityBackend::set_firewall_enabled(checked);
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

    // Docked Progress Bar
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
    // 2. NETWORK PROTECTION CONFIGURATION (Direct Placement)
    // =========================================================================
    auto sec_policy_hdr = ui::make_section_header("FIREWALL PRESET POLICY");
    sec_policy_hdr->set_margin(0, 18, 0, 12);
    m_layout->add_view(sec_policy_hdr);

    auto mode_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    mode_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    mode_row->set_margin(0, 4, 0, 20);

    auto mode_badge = ui::make_icon_badge("security-high", auto_cfg->colors.surface_variant, 34, 8, 14);
    mode_row->add_view(mode_badge);

    auto mode_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    mode_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    auto mode_title = TextViewBuilder::create()->text("Protection Preset")->bold(true)->build();
    m_mode_desc = TextViewBuilder::create()
        ->text("")
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    m_mode_desc->set_margin(0, 3, 0, 0);

    mode_col->add_view(mode_title);
    mode_col->add_view(m_mode_desc);
    mode_row->add_view(mode_col);

    std::vector<std::string> mode_items = {
        "🏠 Home Wi-Fi (Trusted)",
        "📱 Mobile Hotspot (Saver)",
        "☕ Public Wi-Fi (Stealth)",
        "🔒 Extreme Lockdown"
    };

    m_spinner_mode = SpinnerBuilder::create()
        ->items(mode_items)
        ->selectedIndex(mode_to_index(m_info.current_mode))
        ->padding(10, 6)
        ->onItemSelected([this](int idx, const std::string&) {
            if (m_updating_ui) return;
            NetworkMode target_mode = index_to_mode(idx);
            update_mode_descriptions(target_mode);

            if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);

            std::thread([this, target_mode]() {
                bool ok = SecurityBackend::set_network_mode(target_mode);
                if (auto engine = AppEngine::instance()) {
                    engine->post([this, ok]() {
                        if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Invisible);
                        if (ok) {
                            if (m_on_changed) m_on_changed();
                        } else {
                            m_updating_ui = true;
                            if (m_spinner_mode) {
                                m_spinner_mode->set_selected_index(mode_to_index(m_info.current_mode));
                            }
                            update_mode_descriptions(m_info.current_mode);
                            m_updating_ui = false;
                        }
                    });
                }
            }).detach();
        })
        ->build();
    m_spinner_mode->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    mode_row->add_view(m_spinner_mode);

    m_layout->add_view(mode_row);
    update_mode_descriptions(m_info.current_mode);

    // =========================================================================
    // 3. OPEN CUSTOM INBOUND PORT (Direct Placement)
    // =========================================================================
    auto sec_port_hdr = ui::make_section_header("OPEN INBOUND PORT");
    sec_port_hdr->set_margin(0, 18, 0, 12);
    m_layout->add_view(sec_port_hdr);

    auto custom_port_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    custom_port_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    custom_port_col->set_margin(0, 4, 0, 20);

    auto port_top = std::make_shared<LinearLayout>(Orientation::Horizontal);
    port_top->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    port_top->set_margin(0, 2, 0, 12);

    auto port_badge = ui::make_icon_badge("security-low", auto_cfg->colors.surface_variant, 34, 8, 14);
    port_top->add_view(port_badge);

    auto port_text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    port_text_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));
    auto port_title = TextViewBuilder::create()->text("Open Inbound Port")->bold(true)->build();
    auto port_desc = TextViewBuilder::create()
        ->text("Allow incoming connections on a specific port for testing or servers.")
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    port_desc->set_margin(0, 3, 0, 0);
    port_text_col->add_view(port_title);
    port_text_col->add_view(port_desc);
    port_top->add_view(port_text_col);
    custom_port_col->add_view(port_top);

    // Form row: Port Number input + Protocol dropdown + Allow Button
    auto form_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    form_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    form_row->set_margin(0, 4, 0, 8);

    m_input_port = EditTextBuilder::create()
        ->hint("Port number (e.g. 3000, 8080, 25565)")
        ->padding(12, 8)
        ->build();
    m_input_port->set_layout_params(LayoutParams(
        0,
        static_cast<int>(LayoutDimension::WrapContent),
        1.0f
    ));
    m_input_port->set_margin(0, 0, 12, 0);
    form_row->add_view(m_input_port);

    std::vector<std::string> proto_items = {"TCP", "UDP", "TCP & UDP"};
    m_spinner_proto = SpinnerBuilder::create()
        ->items(proto_items)
        ->selectedIndex(0)
        ->padding(10, 8)
        ->onItemSelected([this](int idx, const std::string&) {
            if (idx == 0) m_selected_proto = "tcp";
            else if (idx == 1) m_selected_proto = "udp";
            else m_selected_proto = "any";
        })
        ->build();
    m_spinner_proto->set_layout_params(LayoutParams(
        130,
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_spinner_proto->set_margin(0, 0, 12, 0);
    form_row->add_view(m_spinner_proto);

    auto btn_add = ButtonBuilder::create()
        ->text("+ Open Port")
        ->primary(true)
        ->padding(14, 8)
        ->onClick([this]() {
            std::string port = m_input_port->get_text();
            std::string proto = m_selected_proto;
            if (!port.empty()) {
                m_input_port->set_text("");
                if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);
                std::thread([this, port, proto]() {
                    bool ok = SecurityBackend::add_firewall_rule(port, proto, "ALLOW");
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
    btn_add->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    form_row->add_view(btn_add);
    custom_port_col->add_view(form_row);

    auto port_note = TextViewBuilder::create()
        ->text("Adds an active inbound rule to ufw/nftables. Active rules appear in the table below.")
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    port_note->set_margin(0, 4, 0, 0);
    custom_port_col->add_view(port_note);

    m_layout->add_view(custom_port_col);

    // =========================================================================
    // 4. LOCAL NETWORK SERVICES (Direct Placement with Rich Spacing)
    // =========================================================================
    auto sec_serv_hdr = ui::make_section_header("LOCAL NETWORK SERVICES");
    sec_serv_hdr->set_margin(0, 18, 0, 12);
    m_layout->add_view(sec_serv_hdr);

    auto serv_list = std::make_shared<LinearLayout>(Orientation::Vertical);
    serv_list->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    serv_list->set_margin(0, 4, 0, 20);

    auto make_service_row = [this](const std::string& icon,
                                   const std::string& title,
                                   const std::string& desc,
                                   bool is_allowed, const std::string& port,
                                   std::shared_ptr<Switch>& out_switch) {
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

        auto name_tv = TextViewBuilder::create()->text(title)->bold(true)->build();
        auto desc_tv = TextViewBuilder::create()->text(desc)->caption()->muted()->multiline(true)->ellipsize(false)->build();
        desc_tv->set_margin(0, 2, 0, 0);

        col->add_view(name_tv);
        col->add_view(desc_tv);
        row->add_view(col);

        auto sw = SwitchBuilder::create()
            ->checked(is_allowed)
            ->build();
        sw->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::WrapContent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        std::weak_ptr<Switch> weak_sw = sw;
        sw->set_on_checked_changed_listener([this, port, weak_sw](bool checked) {
            if (m_updating_ui) return;
            if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);
            std::thread([this, port, checked, weak_sw]() {
                bool ok = SecurityBackend::toggle_service(port, checked);
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

    // SSH Row
    serv_list->add_view(make_service_row("utilities-terminal", "Remote Terminal (SSH)",
        "Secure command-line login over local network (Port 22).",
        m_info.ssh_allowed, "22", m_switch_ssh));

    // Local Web Dev Row
    serv_list->add_view(make_service_row("network-workgroup", "Local Web Server",
        "Incoming connections for local web development (Port 8080).",
        m_info.web_dev_allowed, "8080", m_switch_web));

    // Syncthing Row
    serv_list->add_view(make_service_row("view-refresh", "Syncthing Sync",
        "Peer-to-peer folder and document synchronization (Port 22000).",
        m_info.syncthing_allowed, "22000", m_switch_syncthing));

    // Samba Row
    serv_list->add_view(make_service_row("folder", "File Sharing (Samba)",
        "Local network folder and file sharing (Port 445).",
        m_info.samba_allowed, "445", m_switch_samba));

    m_layout->add_view(serv_list);

    // =========================================================================
    // 5. CONFIGURED RULES LIST (Direct Placement)
    // =========================================================================
    m_rules_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_rules_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_rules_container->set_margin(0, 0, 0, 8);

    rebuild_rules_list();
    m_layout->add_view(m_rules_container);

    // =========================================================================
    // 6. "WHAT IS A FIREWALL?" (Direct Placement Callout)
    // =========================================================================
    auto guide_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    guide_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    guide_col->set_margin(0, 18, 0, 24);

    auto g_title = TextViewBuilder::create()->text("💡 What is a Firewall?")->bold(true)->build();
    auto g_desc = TextViewBuilder::create()
        ->text("Monitors incoming and outgoing network traffic to block unauthorized connections, port scans, and remote exploits.")
        ->caption()
        ->muted()
        ->multiline(true)
        ->build();
    g_desc->set_margin(0, 4, 0, 0);

    guide_col->add_view(g_title);
    guide_col->add_view(g_desc);
    m_layout->add_view(guide_col);

    set_content_view(m_layout);
}

void FirewallView::update_info(const FirewallInfo& info) {
    m_info = info;

    if (m_status_lbl) {
        m_status_lbl->set_text(m_info.active ? "🛡️ Firewall Protection is Active" : "🛡️ Firewall Protection is Disabled");
    }

    if (m_network_lbl) {
        std::string net_str = m_info.active_network_name.empty() ?
            "No active network connection detected." :
            ("Active on: " + m_info.active_network_name + (m_info.metered ? " • 📶 Metered Network" : ""));
        m_network_lbl->set_text(net_str);
    }

    m_updating_ui = true;

    if (m_switch_master) m_switch_master->set_checked(m_info.active);
    if (m_switch_ssh) m_switch_ssh->set_checked(m_info.ssh_allowed);
    if (m_switch_web) m_switch_web->set_checked(m_info.web_dev_allowed);
    if (m_switch_syncthing) m_switch_syncthing->set_checked(m_info.syncthing_allowed);
    if (m_switch_samba) m_switch_samba->set_checked(m_info.samba_allowed);

    if (m_spinner_mode) {
        m_spinner_mode->set_selected_index(mode_to_index(m_info.current_mode));
    }
    update_mode_descriptions(m_info.current_mode);

    m_updating_ui = false;

    rebuild_rules_list();
}

void FirewallView::rebuild_rules_list() {
    if (!m_rules_container) return;
    m_rules_container->clear_views();

    auto list_hdr = ui::make_section_header("ACTIVE INBOUND PORT RULES (" + std::to_string(m_info.rules.size()) + ")");
    list_hdr->set_margin(0, 18, 0, 12);
    m_rules_container->add_view(list_hdr);

    if (m_info.rules.empty()) {
        auto empty_col = std::make_shared<LinearLayout>(Orientation::Vertical);
        empty_col->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));
        empty_col->set_margin(0, 4, 0, 16);

        auto empty_tv = TextViewBuilder::create()
            ->text("🛡️ No custom port rules added. Default security policy guards all inbound ports.")
            ->caption()
            ->muted()
            ->multiline(true)
            ->maxLines(3)
            ->ellipsize(true)
            ->build();
        empty_tv->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));
        empty_col->add_view(empty_tv);
        m_rules_container->add_view(empty_col);
        return;
    }

    auto list_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    list_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    list_col->set_margin(0, 4, 0, 16);

    auto cfg = Config::get();
    for (size_t i = 0; i < m_info.rules.size(); ++i) {
        const auto& rule = m_info.rules[i];
        auto r_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        r_row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        r_row->set_margin(0, 10, 0, 10);

        bool is_allow = (rule.action == "ALLOW");
        auto r_badge = ui::make_icon_badge(
            is_allow ? "emblem-default" : "dialog-error",
            is_allow ? cfg->colors.primary_container : cfg->colors.surface_variant,
            34, 8, 14
        );
        r_row->add_view(r_badge);

        auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
        col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

        auto port_tv = TextViewBuilder::create()
            ->text(rule.port_or_service + " / " + rule.protocol)
            ->bold(true)
            ->ellipsize(true)
            ->build();
        port_tv->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));

        auto src_tv = TextViewBuilder::create()
            ->text("Direction: " + rule.direction + " • Source: " + rule.source)
            ->caption()
            ->muted()
            ->multiline(true)
            ->maxLines(3)
            ->ellipsize(true)
            ->build();
        src_tv->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));
        src_tv->set_margin(0, 3, 0, 0);

        col->add_view(port_tv);
        col->add_view(src_tv);
        r_row->add_view(col);

        FirewallRule current_rule = rule;
        auto del_btn = ButtonBuilder::create()
            ->text("Delete")
            ->flat(true)
            ->padding(10, 6)
            ->onClick([this, current_rule]() {
                if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);
                std::thread([this, current_rule]() {
                    bool ok = SecurityBackend::delete_firewall_rule(current_rule);
                    if (auto engine = AppEngine::instance()) {
                        engine->post([this, ok]() {
                            if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Invisible);
                            if (ok && m_on_changed) m_on_changed();
                        });
                    }
                }).detach();
            })
            ->build();
        r_row->add_view(del_btn);

        list_col->add_view(r_row);
    }

    m_rules_container->add_view(list_col);
}

void FirewallView::update_mode_descriptions(NetworkMode mode) {
    if (!m_mode_desc) return;

    switch (mode) {
        case NetworkMode::Home:
            m_mode_desc->set_text("Blocks inbound attacks while keeping LAN devices accessible.");
            break;
        case NetworkMode::MobileHotspot:
            m_mode_desc->set_text("Drops inbound probes and pauses background updates for metered data.");
            break;
        case NetworkMode::PublicWifi:
            m_mode_desc->set_text("Stealth mode. Drops network scans and ignores ping probes.");
            break;
        case NetworkMode::Lockdown:
            m_mode_desc->set_text("Strict isolation. Shuts down all inbound ports and listening shares.");
            break;
        default:
            m_mode_desc->set_text("Custom user-configured firewall policy.");
            break;
    }
}

int FirewallView::mode_to_index(NetworkMode mode) {
    switch (mode) {
        case NetworkMode::Home: return 0;
        case NetworkMode::MobileHotspot: return 1;
        case NetworkMode::PublicWifi: return 2;
        case NetworkMode::Lockdown: return 3;
        default: return 0;
    }
}

NetworkMode FirewallView::index_to_mode(int index) {
    switch (index) {
        case 0: return NetworkMode::Home;
        case 1: return NetworkMode::MobileHotspot;
        case 2: return NetworkMode::PublicWifi;
        case 3: return NetworkMode::Lockdown;
        default: return NetworkMode::Home;
    }
}

} // namespace miqusecure

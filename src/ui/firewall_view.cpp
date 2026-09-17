#include "firewall_view.hpp"
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
    m_layout->set_padding(4, 2);

    // =========================================================================
    // 1. MASTER FIREWALL SWITCH (Clean Borderless Hero)
    // =========================================================================
    auto master_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    master_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    master_row->set_margin(0, 4, 0, 16);

    auto status_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    status_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    m_status_lbl = TextViewBuilder::create()
        ->text(m_info.active ? "🛡️ Firewall Protection is ON" : "⚠️ Firewall Protection is OFF")
        ->h2()
        ->bold(true)
        ->build();

    std::string desc_str = m_info.active ?
        "Unknown incoming connections from Wi-Fi and internet are blocked." :
        "The computer is exposed to unsolicited inbound traffic on this network.";

    m_status_desc = TextViewBuilder::create()
        ->text(desc_str)
        ->caption()
        ->muted()
        ->build();
    m_status_desc->set_margin(0, 2, 0, 0);

    status_col->add_view(m_status_lbl);
    status_col->add_view(m_status_desc);
    master_row->add_view(status_col);

    // Native Master Switch
    m_switch_master = SwitchBuilder::create()
        ->checked(m_info.active)
        ->build();
    std::weak_ptr<Switch> weak_master = m_switch_master;
    m_switch_master->set_on_checked_changed_listener([this, weak_master](bool checked) {
        if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);
        if (m_status_desc) m_status_desc->set_text("Applying firewall policy changes...");

        std::thread([this, checked, weak_master]() {
            bool ok = SecurityBackend::set_firewall_enabled(checked);
            if (auto engine = AppEngine::instance()) {
                engine->post([this, checked, ok, weak_master]() {
                    if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Invisible);
                    if (ok) {
                        if (m_on_changed) m_on_changed();
                    } else {
                        if (auto s = weak_master.lock()) {
                            s->set_checked(!checked);
                        }
                    }
                });
            }
        }).detach();
    });
    master_row->add_view(m_switch_master);

    m_layout->add_view(master_row);

    // Material Design 3 Docked Linear Progress Bar (zero vertical layout shifts)
    m_progress_bar = ProgressBarBuilder::create()
        ->style(ProgressBarStyle::Linear)
        ->indeterminate(true)
        ->trackHeight(3)
        ->build();
    m_progress_bar->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        3
    ));
    m_progress_bar->set_margin(0, 0, 0, 10);
    m_progress_bar->set_visibility(Visibility::Invisible);
    m_layout->add_view(m_progress_bar);

    // =========================================================================
    // 2. NETWORK PROTECTION MODES (Switch-Driven Grouped Card)
    // =========================================================================
    auto def_title = TextViewBuilder::create()
        ->text("Network Protection Modes")
        ->h3()
        ->bold(true)
        ->build();
    def_title->set_margin(0, 0, 0, 2);
    m_layout->add_view(def_title);

    std::string net_str = m_info.active_network_name.empty() ?
        "Current Network: None detected" :
        ("Current Network: " + m_info.active_network_name + (m_info.metered ? " • 📶 Mobile Metered Data-Saver ON" : ""));
    m_network_lbl = TextViewBuilder::create()
        ->text(net_str)
        ->caption()
        ->muted()
        ->build();
    m_network_lbl->set_margin(0, 0, 0, 8);
    m_layout->add_view(m_network_lbl);

    m_modes_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_modes_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_modes_container->set_margin(0, 0, 0, 16);
    rebuild_modes_list();
    m_layout->add_view(m_modes_container);

    // =========================================================================
    // 3. LOCAL SERVICES SHARING (Switch-Driven Grouped Card)
    // =========================================================================
    auto serv_title = TextViewBuilder::create()
        ->text("Local Services Sharing")
        ->h3()
        ->bold(true)
        ->build();
    serv_title->set_margin(0, 0, 0, 8);
    m_layout->add_view(serv_title);

    auto serv_card = std::make_shared<CardView>();
    serv_card->set_padding(14, 6);
    serv_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    serv_card->set_margin(0, 0, 0, 16);

    auto serv_list = std::make_shared<LinearLayout>(Orientation::Vertical);
    serv_list->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto make_service_row = [this](const std::string& icon, const std::string& title,
                                   const std::string& desc, bool is_allowed,
                                   const std::string& port,
                                   std::shared_ptr<Switch>& out_switch) {
        auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        row->set_margin(4, 6, 4, 6);

        auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
        col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

        auto t_tv = TextViewBuilder::create()->text(icon + " " + title)->bold(true)->build();
        auto d_tv = TextViewBuilder::create()->text(desc)->caption()->muted()->multiline(true)->maxLines(2)->ellipsize(true)->build();
        col->add_view(t_tv);
        col->add_view(d_tv);
        row->add_view(col);

        auto sw = SwitchBuilder::create()
            ->checked(is_allowed)
            ->build();
        std::weak_ptr<Switch> weak_sw = sw;
        sw->set_on_checked_changed_listener([this, port, weak_sw](bool checked) {
            std::thread([this, port, checked, weak_sw]() {
                bool ok = SecurityBackend::toggle_service(port, checked);
                if (auto engine = AppEngine::instance()) {
                    engine->post([this, checked, ok, weak_sw]() {
                        if (ok) {
                            if (m_on_changed) m_on_changed();
                        } else {
                            if (auto s = weak_sw.lock()) {
                                s->set_checked(!checked);
                            }
                        }
                    });
                }
            }).detach();
        });
        out_switch = sw;
        row->add_view(out_switch);

        return row;
    };

    // SSH
    serv_list->add_view(make_service_row("💻", "Remote Terminal (SSH - Port 22)",
        "Allow secure command-line login from trusted machines across the network.",
        m_info.ssh_allowed, "22", m_switch_ssh));
    serv_list->add_view(DividerViewBuilder::create()->margin(0, 4)->build());

    // Local Web Dev
    serv_list->add_view(make_service_row("🌐", "Local Web Development (Port 8080)",
        "Allow testing local web servers on this machine from other devices.",
        m_info.web_dev_allowed, "8080", m_switch_web));
    serv_list->add_view(DividerViewBuilder::create()->margin(0, 4)->build());

    // Syncthing
    serv_list->add_view(make_service_row("🔄", "Syncthing File Sync (Port 22000)",
        "Allow continuous peer-to-peer folder and document synchronization.",
        m_info.syncthing_allowed, "22000", m_switch_syncthing));

    serv_card->add_view(serv_list);
    m_layout->add_view(serv_card);

    // =========================================================================
    // 4. CUSTOM PORT RULE (Clean, Flat Inset)
    // =========================================================================
    auto form_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    form_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    form_row->set_margin(0, 0, 0, 16);

    m_input_port = EditTextBuilder::create()
        ->hint("Custom port (e.g. 5000)")
        ->padding(10, 6)
        ->build();
    m_input_port->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));
    m_input_port->set_margin(0, 0, 8, 0);
    form_row->add_view(m_input_port);

    m_btn_proto_tcp = ButtonBuilder::create()
        ->text("TCP")
        ->flat(true)
        ->bold(true)
        ->padding(10, 6)
        ->onClick([this]() {
            m_selected_proto = "tcp";
            m_btn_proto_tcp->set_selected(true);
            m_btn_proto_udp->set_selected(false);
        })
        ->build();
    m_btn_proto_tcp->set_selected(true);
    m_btn_proto_tcp->set_margin(0, 0, 4, 0);
    form_row->add_view(m_btn_proto_tcp);

    m_btn_proto_udp = ButtonBuilder::create()
        ->text("UDP")
        ->flat(true)
        ->padding(10, 6)
        ->onClick([this]() {
            m_selected_proto = "udp";
            m_btn_proto_tcp->set_selected(false);
            m_btn_proto_udp->set_selected(true);
        })
        ->build();
    m_btn_proto_udp->set_margin(0, 0, 8, 0);
    form_row->add_view(m_btn_proto_udp);

    auto btn_add = ButtonBuilder::create()
        ->text("+ Allow")
        ->primary(true)
        ->padding(14, 6)
        ->onClick([this]() {
            std::string port = m_input_port->get_text();
            std::string proto = m_selected_proto;
            if (!port.empty()) {
                m_input_port->set_text("");
                std::thread([this, port, proto]() {
                    bool ok = SecurityBackend::add_firewall_rule(port, proto, "ALLOW");
                    if (auto engine = AppEngine::instance()) {
                        engine->post([this, ok]() {
                            if (ok && m_on_changed) m_on_changed();
                        });
                    }
                }).detach();
            }
        })
        ->build();
    form_row->add_view(btn_add);

    m_layout->add_view(form_row);

    // =========================================================================
    // 5. CONFIGURED RULES LIST (Unified Card Container)
    // =========================================================================
    auto rules_hdr = TextViewBuilder::create()
        ->text("Active Rules (" + std::to_string(m_info.rules.size()) + ")")
        ->h3()
        ->bold(true)
        ->build();
    rules_hdr->set_margin(0, 0, 0, 8);
    m_layout->add_view(rules_hdr);

    m_rules_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_rules_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    rebuild_rules_list();
    m_layout->add_view(m_rules_container);

    // =========================================================================
    // 6. "WHAT IS A FIREWALL?" (Soft Tinted, Borderless Callout)
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
    auto g_title = TextViewBuilder::create()->text("💡 What is a Firewall?")->bold(true)->build();
    auto g_desc = TextViewBuilder::create()
        ->text("A firewall inspects incoming and outgoing network traffic. Outbound traffic to the internet (such as web browsing or media streaming) is permitted, while unsolicited inbound connections from the local Wi-Fi or external networks are blocked to protect services, cameras, and files on the computer.")
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

void FirewallView::rebuild_rules_list() {
    if (!m_rules_container) return;
    m_rules_container->clear_views();

    if (m_info.rules.empty()) {
        auto empty_tv = TextViewBuilder::create()
            ->text("No custom port rules added. Default security policy is active.")
            ->caption()
            ->muted()
            ->build();
        empty_tv->set_margin(0, 4, 0, 8);
        m_rules_container->add_view(empty_tv);
        return;
    }

    auto rules_card = std::make_shared<CardView>();
    rules_card->set_padding(14, 6);
    rules_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    rules_card->set_margin(0, 0, 0, 8);

    auto list_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    list_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    for (size_t i = 0; i < m_info.rules.size(); ++i) {
        const auto& rule = m_info.rules[i];
        auto r_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        r_row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        r_row->set_margin(4, 6, 4, 6);

        auto act_lbl = TextViewBuilder::create()
            ->text(rule.action == "ALLOW" ? "✔ ALLOW" : "✖ DENY")
            ->bold(true)
            ->build();
        act_lbl->set_margin(0, 0, 14, 0);
        r_row->add_view(act_lbl);

        auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
        col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

        auto port_tv = TextViewBuilder::create()
            ->text(rule.port_or_service + " / " + rule.protocol)
            ->bold(true)
            ->build();

        auto src_tv = TextViewBuilder::create()
            ->text("Direction: " + rule.direction + " • Source: " + rule.source)
            ->caption()
            ->muted()
            ->build();

        col->add_view(port_tv);
        col->add_view(src_tv);
        r_row->add_view(col);

        int rule_idx = rule.id;
        auto del_btn = ButtonBuilder::create()
            ->text("Delete")
            ->flat(true)
            ->padding(8, 4)
            ->onClick([this, rule_idx]() {
                std::thread([this, rule_idx]() {
                    bool ok = SecurityBackend::delete_firewall_rule(rule_idx);
                    if (auto engine = AppEngine::instance()) {
                        engine->post([this, ok]() {
                            if (ok && m_on_changed) m_on_changed();
                        });
                    }
                }).detach();
            })
            ->build();
        r_row->add_view(del_btn);

        list_col->add_view(r_row);
        if (i + 1 < m_info.rules.size()) {
            list_col->add_view(DividerViewBuilder::create()->margin(0, 4)->build());
        }
    }

    rules_card->add_view(list_col);
    m_rules_container->add_view(rules_card);
}

void FirewallView::rebuild_modes_list() {
    if (!m_modes_container) return;
    m_modes_container->clear_views();

    auto card = std::make_shared<CardView>();
    card->set_padding(14, 12);
    card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto card_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    card_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    // Top row: Label on left, Spinner on right
    auto header_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    header_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    auto title_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    title_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    auto t_tv = TextViewBuilder::create()
        ->text("Active Protection Profile")
        ->bold(true)
        ->build();
    auto s_tv = TextViewBuilder::create()
        ->text("Select network security policy preset")
        ->caption()
        ->muted()
        ->build();
    title_col->add_view(t_tv);
    title_col->add_view(s_tv);
    header_row->add_view(title_col);

    std::vector<std::string> mode_items = {
        "🏠 Home Wi-Fi (Trusted)",
        "📱 Mobile Hotspot (Data-Saver)",
        "☕ Public Wi-Fi (Stealth)",
        "🔒 Extreme Lockdown (Isolated)"
    };

    m_spinner_mode = SpinnerBuilder::create()
        ->items(mode_items)
        ->selectedIndex(mode_to_index(m_info.current_mode))
        ->padding(12, 8)
        ->onItemSelected([this](int idx, const std::string&) {
            if (m_updating_ui) return;
            NetworkMode target_mode = index_to_mode(idx);
            update_mode_descriptions(target_mode);

            if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);
            if (m_status_desc) m_status_desc->set_text("Applying network protection preset...");

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
        270,
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    header_row->add_view(m_spinner_mode);
    card_col->add_view(header_row);

    // Divider
    card_col->add_view(DividerViewBuilder::create()->margin(0, 10)->build());

    // Description & Technical details container
    m_mode_desc = TextViewBuilder::create()
        ->text("")
        ->caption()
        ->multiline(true)
        ->build();
    m_mode_desc->set_margin(0, 0, 0, 4);

    m_mode_tech_desc = TextViewBuilder::create()
        ->text("")
        ->caption()
        ->muted()
        ->ellipsize(true)
        ->build();

    card_col->add_view(m_mode_desc);
    card_col->add_view(m_mode_tech_desc);

    update_mode_descriptions(m_info.current_mode);

    card->add_view(card_col);
    m_modes_container->add_view(card);
}

void FirewallView::update_mode_descriptions(NetworkMode mode) {
    if (!m_mode_desc || !m_mode_tech_desc) return;

    switch (mode) {
        case NetworkMode::Home:
            m_mode_desc->set_text("Optimized for home or trusted private networks. Shields the computer from internet intrusions while keeping printers and local devices accessible.");
            m_mode_tech_desc->set_text("⚙ ufw: deny in, allow out | metered: no | LAN discovery & local services allowed");
            break;
        case NetworkMode::MobileHotspot:
            m_mode_desc->set_text("Optimized when tethered to mobile 4G/5G hotspots. Drops incoming scan probes and pauses background updates to conserve mobile data.");
            m_mode_tech_desc->set_text("⚙ ufw: reject in | nmcli: connection.metered yes | pauses system & flatpak background sync");
            break;
        case NetworkMode::PublicWifi:
            m_mode_desc->set_text("Optimized for coffee shops, airports, and hotels. Makes the computer invisible to network scanners, ignores ping probes, and rejects untrusted inbound scans.");
            m_mode_tech_desc->set_text("⚙ ufw: reject in | ICMP scan drops | mDNS/UPnP discovery suppressed");
            break;
        case NetworkMode::Lockdown:
            m_mode_desc->set_text("Maximum isolation for hostile or suspicious networks. Shuts down all local listening ports and blocks all incoming connections unconditionally.");
            m_mode_tech_desc->set_text("⚙ ufw: reject in | local ports (22, 8080, 22000) forced closed | zero inbound exposure");
            break;
        default:
            m_mode_desc->set_text("Custom user-configured firewall policy.");
            m_mode_tech_desc->set_text("⚙ manual port and routing rules applied");
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

void FirewallView::update_info(const FirewallInfo& info) {
    m_info = info;

    if (m_status_lbl) {
        m_status_lbl->set_text(m_info.active ? "🛡️ Firewall Protection is ON" : "⚠️ Firewall Protection is OFF");
    }

    if (m_status_desc) {
        std::string desc_str = m_info.active ?
            "Unknown incoming connections from Wi-Fi and internet are blocked." :
            "The computer is visible and exposed to other devices on this network.";
        m_status_desc->set_text(desc_str);
    }

    if (m_switch_master) {
        m_switch_master->set_checked(m_info.active);
    }

    if (m_network_lbl) {
        std::string net_str = m_info.active_network_name.empty() ?
            "Current Network: None detected" :
            ("Current Network: " + m_info.active_network_name + (m_info.metered ? " • 📶 Mobile Metered Data-Saver ON" : ""));
        m_network_lbl->set_text(net_str);
    }

    m_updating_ui = true;
    if (m_spinner_mode) {
        m_spinner_mode->set_selected_index(mode_to_index(m_info.current_mode));
    }
    update_mode_descriptions(m_info.current_mode);
    m_updating_ui = false;

    if (m_switch_ssh) {
        m_switch_ssh->set_checked(m_info.ssh_allowed);
    }

    if (m_switch_web) {
        m_switch_web->set_checked(m_info.web_dev_allowed);
    }

    if (m_switch_syncthing) {
        m_switch_syncthing->set_checked(m_info.syncthing_allowed);
    }

    rebuild_rules_list();
}

} // namespace miqusecure

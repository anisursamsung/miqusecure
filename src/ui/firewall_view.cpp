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
    m_layout->set_padding(18, 14);

    // =========================================================================
    // 1. MASTER FIREWALL STATUS HERO CARD (Windows Security / iOS Style)
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

    auto hero_badge = ui::make_icon_badge(
        "🛡️",
        m_info.active ? auto_cfg->colors.primary_container : auto_cfg->colors.surface_variant,
        40, 10, 14
    );
    hero_row->add_view(hero_badge);

    auto hero_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    hero_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    m_status_lbl = TextViewBuilder::create()
        ->text(m_info.active ? "Firewall Protection is Active" : "Firewall Protection is Disabled")
        ->h2()
        ->bold(true)
        ->build();

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
    hero_card->add_view(hero_row);
    m_layout->add_view(hero_card);

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
    m_progress_bar->set_margin(0, 0, 0, 10);
    m_progress_bar->set_visibility(Visibility::Invisible);
    m_layout->add_view(m_progress_bar);

    // =========================================================================
    // 2. NETWORK PROTECTION CONFIGURATION (Grouped Card)
    // =========================================================================
    m_layout->add_view(ui::make_section_header("FIREWALL PRESET POLICY"));

    auto pol_card = std::make_shared<CardView>();
    pol_card->set_padding(14, 10);
    pol_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    pol_card->set_margin(0, 0, 0, 14);

    auto mode_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    mode_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    mode_row->set_margin(4, 6, 4, 6);

    auto mode_badge = ui::make_icon_badge("🛡️", auto_cfg->colors.surface_variant, 32, 8, 12);
    mode_row->add_view(mode_badge);

    auto mode_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    mode_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    auto mode_title = TextViewBuilder::create()->text("Protection Preset Profile")->bold(true)->build();
    m_mode_desc = TextViewBuilder::create()
        ->text("")
        ->caption()
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    m_mode_desc->set_margin(0, 2, 0, 2);

    m_mode_tech_desc = TextViewBuilder::create()
        ->text("")
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();

    mode_col->add_view(mode_title);
    mode_col->add_view(m_mode_desc);
    mode_col->add_view(m_mode_tech_desc);
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

    pol_card->add_view(mode_row);
    m_layout->add_view(pol_card);
    update_mode_descriptions(m_info.current_mode);

    // =========================================================================
    // 3. OPEN CUSTOM INBOUND PORT CARD (Dedicated Form Card)
    // =========================================================================
    m_layout->add_view(ui::make_section_header("OPEN INBOUND PORT"));

    auto custom_port_card = std::make_shared<CardView>();
    custom_port_card->set_padding(14, 10);
    custom_port_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    custom_port_card->set_margin(0, 0, 0, 14);

    auto custom_port_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    custom_port_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto port_top = std::make_shared<LinearLayout>(Orientation::Horizontal);
    port_top->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    port_top->set_margin(4, 4, 4, 8);

    auto port_badge = ui::make_icon_badge("🔓", auto_cfg->colors.surface_variant, 32, 8, 12);
    port_top->add_view(port_badge);

    auto port_text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    port_text_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));
    auto port_title = TextViewBuilder::create()->text("Open Inbound Port Rule")->bold(true)->build();
    auto port_desc = TextViewBuilder::create()
        ->text("Allow external network connections on a specific port number for local testing or custom servers.")
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    port_desc->set_margin(0, 2, 0, 0);
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
    form_row->set_margin(4, 6, 4, 6);

    m_input_port = EditTextBuilder::create()
        ->hint("Port number (e.g. 3000, 8080, 25565)")
        ->padding(12, 8)
        ->build();
    m_input_port->set_layout_params(LayoutParams(
        0,
        static_cast<int>(LayoutDimension::WrapContent),
        1.0f
    ));
    m_input_port->set_margin(0, 0, 10, 0);
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
    m_spinner_proto->set_margin(0, 0, 10, 0);
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
    port_note->set_margin(4, 4, 4, 0);
    custom_port_col->add_view(port_note);

    custom_port_card->add_view(custom_port_col);
    m_layout->add_view(custom_port_card);

    // =========================================================================
    // 3. LOCAL SERVICES SHARING (Grouped Card Container)
    // =========================================================================
    m_layout->add_view(ui::make_section_header("LOCAL NETWORK SERVICES"));

    auto serv_card = std::make_shared<CardView>();
    serv_card->set_padding(14, 10);
    serv_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    serv_card->set_margin(0, 0, 0, 14);

    auto serv_list = std::make_shared<LinearLayout>(Orientation::Vertical);
    serv_list->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto make_service_row = [this](const std::string& icon,
                                   const std::string& title,
                                   const std::string& desc, const std::string& tech,
                                   bool is_allowed, const std::string& port,
                                   std::shared_ptr<Switch>& out_switch) {
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

        auto name_tv = TextViewBuilder::create()->text(title)->bold(true)->build();
        auto desc_tv = TextViewBuilder::create()->text(desc)->caption()->muted()->multiline(true)->ellipsize(false)->build();
        desc_tv->set_margin(0, 2, 0, 1);
        auto tech_tv = TextViewBuilder::create()->text(tech)->caption()->muted()->multiline(true)->ellipsize(false)->build();

        col->add_view(name_tv);
        col->add_view(desc_tv);
        col->add_view(tech_tv);
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

    // SSH Row
    serv_list->add_view(make_service_row("💻", "Remote Terminal (SSH - 22)",
        "Allow secure command-line login from trusted machines across the network.",
        "⚙ OpenSSH Daemon • Port 22/tcp",
        m_info.ssh_allowed, "22", m_switch_ssh));

    // Local Web Dev Row
    serv_list->add_view(make_service_row("🌐", "Local Web Dev (8080)",
        "Allow testing local web servers on this machine from other devices.",
        "⚙ HTTP Web Server • Port 8080/tcp",
        m_info.web_dev_allowed, "8080", m_switch_web));

    // Syncthing Row
    serv_list->add_view(make_service_row("🔄", "Syncthing File Sync (22000)",
        "Allow continuous peer-to-peer folder and document synchronization.",
        "⚙ Syncthing Protocol • Port 22000/tcp,udp",
        m_info.syncthing_allowed, "22000", m_switch_syncthing));

    // Samba Row
    serv_list->add_view(make_service_row("📁", "LAN File Share (Samba - 445)",
        "Allow Windows and Linux network folder and document sharing.",
        "⚙ SMB/CIFS Network Share • Port 445/tcp",
        m_info.samba_allowed, "445", m_switch_samba));

    serv_card->add_view(serv_list);
    m_layout->add_view(serv_card);

    // =========================================================================
    // 4. CONFIGURED RULES LIST (Unified Card Container)
    // =========================================================================
    m_rules_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_rules_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    rebuild_rules_list();
    m_layout->add_view(m_rules_container);

    // =========================================================================
    // 5. "WHAT IS A FIREWALL?" (Soft Tinted, Borderless Callout)
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
    auto g_title = TextViewBuilder::create()->text("💡 What is a Firewall & Why Configure It?")->bold(true)->build();
    auto g_desc = TextViewBuilder::create()
        ->text("A firewall monitors and controls all incoming and outgoing network traffic based on predefined security rules. It establishes a digital barrier between your trusted internal device and untrusted external networks (like public Wi-Fi).\n\nEnabling Miqu Firewall shields your machine from port scans, unauthorized remote connections, and network-level exploits while allowing your normal web browsing and trusted LAN shares.")
        ->caption()
        ->muted()
        ->multiline(true)
        ->build();
    g_desc->set_margin(0, 4, 0, 0);

    guide_col->add_view(g_title);
    guide_col->add_view(g_desc);
    guide_card->add_view(guide_col);
    m_layout->add_view(guide_card);

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
    m_rules_container->add_view(list_hdr);

    if (m_info.rules.empty()) {
        auto empty_card = std::make_shared<CardView>();
        empty_card->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));
        empty_card->set_padding(14, 12);
        empty_card->set_margin(0, 0, 0, 8);

        auto empty_col = std::make_shared<LinearLayout>(Orientation::Vertical);
        empty_col->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));

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
        empty_card->add_view(empty_col);
        m_rules_container->add_view(empty_card);
        return;
    }

    auto rules_card = std::make_shared<CardView>();
    rules_card->set_padding(14, 8);
    rules_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    rules_card->set_margin(0, 0, 0, 14);

    auto list_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    list_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto cfg = Config::get();
    for (size_t i = 0; i < m_info.rules.size(); ++i) {
        const auto& rule = m_info.rules[i];
        auto r_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        r_row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        r_row->set_margin(4, 6, 4, 6);

        bool is_allow = (rule.action == "ALLOW");
        auto r_badge = ui::make_icon_badge(
            is_allow ? "✔" : "✖",
            is_allow ? cfg->colors.primary_container : cfg->colors.surface_variant,
            32, 8, 12
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

        col->add_view(port_tv);
        col->add_view(src_tv);
        r_row->add_view(col);

        int rule_idx = rule.id;
        auto del_btn = ButtonBuilder::create()
            ->text("Delete")
            ->flat(true)
            ->padding(10, 6)
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
    }

    rules_card->add_view(list_col);
    m_rules_container->add_view(rules_card);
}

void FirewallView::update_mode_descriptions(NetworkMode mode) {
    if (!m_mode_desc || !m_mode_tech_desc) return;

    switch (mode) {
        case NetworkMode::Home:
            m_mode_tech_desc->set_text("⚙ ufw: deny in, allow out • metered: off");
            m_mode_desc->set_text("Optimized for trusted home networks. Blocks intrusions while keeping local devices accessible.");
            break;
        case NetworkMode::MobileHotspot:
            m_mode_tech_desc->set_text("⚙ ufw: reject in • metered: on (saver)");
            m_mode_desc->set_text("Optimized when tethered to mobile hotspots. Drops probes and pauses background updates.");
            break;
        case NetworkMode::PublicWifi:
            m_mode_tech_desc->set_text("⚙ ufw: reject in • stealth ICMP drop");
            m_mode_desc->set_text("Optimized for coffee shops and hotels. Invisible to network scanners and ignores ping probes.");
            break;
        case NetworkMode::Lockdown:
            m_mode_tech_desc->set_text("⚙ ufw: reject in • local ports locked");
            m_mode_desc->set_text("Maximum isolation for hostile networks. Shuts down local listening ports and blocks all inbound traffic.");
            break;
        default:
            m_mode_tech_desc->set_text("⚙ custom firewall rules applied");
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

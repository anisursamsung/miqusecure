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
    // 0. FIREWALL MASTER STATUS PILL (Centered WrapContent Hero Capsule)
    // =========================================================================
    auto status_pill = std::make_shared<CardView>();
    status_pill->set_radius(26);
    status_pill->set_padding(24, 12);
    status_pill->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterHorizontal
    ));
    status_pill->set_margin(0, 4, 0, 18);

    auto status_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    status_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    // Shield Icon
    m_status_icon = TextViewBuilder::create()
        ->text(m_info.active ? "🛡️" : "⚠️")
        ->h2()
        ->build();
    m_status_icon->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    m_status_icon->set_margin(0, 0, 12, 0);
    status_row->add_view(m_status_icon);

    // Title
    m_status_lbl = TextViewBuilder::create()
        ->text(m_info.active ? "Firewall Protection is Active" : "Firewall Protection is Inactive")
        ->h3()
        ->bold(true)
        ->build();
    m_status_lbl->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    m_status_lbl->set_margin(0, 0, 18, 0);
    status_row->add_view(m_status_lbl);

    // Master Switch on right
    m_switch_master = SwitchBuilder::create()
        ->checked(m_info.active)
        ->build();
    m_switch_master->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
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
    status_row->add_view(m_switch_master);

    status_pill->add_view(status_row);
    m_layout->add_view(status_pill);

    // =========================================================================
    // 1. TOP DUAL CARDS (Left: Firewall Mode, Right: Allow Custom Port)
    // =========================================================================
    auto top_cards_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    top_cards_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    top_cards_row->set_margin(0, 0, 0, 16);

    // -------------------------------------------------------------------------
    // LEFT CARD: Firewall Mode Preset & Network Details
    // -------------------------------------------------------------------------
    auto left_card = std::make_shared<CardView>();
    left_card->set_padding(16, 16);
    left_card->set_layout_params(LayoutParams(
        0,
        static_cast<int>(LayoutDimension::WrapContent),
        1.0f
    ));
    left_card->set_margin(0, 0, 6, 0);

    auto left_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    left_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    // Card Header: Firewall Mode
    auto mode_title = TextViewBuilder::create()
        ->text("Firewall Mode")
        ->h3()
        ->bold(true)
        ->textAlignment(TextAlignment::Center)
        ->build();
    mode_title->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterHorizontal
    ));
    mode_title->set_margin(0, 0, 0, 10);
    left_col->add_view(mode_title);

    // Line 2: Responsive Protection Preset Spinner
    std::vector<std::string> mode_items = {
        "🏠 Home Wi-Fi (Trusted)",
        "📱 Mobile Hotspot (Saver)",
        "☕ Public Wi-Fi (Stealth)",
        "🔒 Extreme Lockdown"
    };

    m_spinner_mode = SpinnerBuilder::create()
        ->items(mode_items)
        ->selectedIndex(mode_to_index(m_info.current_mode))
        ->padding(8, 6)
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
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterHorizontal
    ));
    m_spinner_mode->set_margin(0, 0, 0, 10);
    left_col->add_view(m_spinner_mode);

    // Progress bar
    m_progress_bar = ProgressBarBuilder::create()
        ->style(ProgressBarStyle::Linear)
        ->indeterminate(true)
        ->trackHeight(3)
        ->build();
    m_progress_bar->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        3
    ));
    m_progress_bar->set_margin(0, 0, 0, 8);
    m_progress_bar->set_visibility(Visibility::Invisible);
    left_col->add_view(m_progress_bar);

    // Divider
    left_col->add_view(DividerViewBuilder::create()->margin(0, 6)->build());

    // Details:
    // a) Current Network
    std::string net_str = m_info.active_network_name.empty() ?
        "Network: None detected" :
        ("Network: " + m_info.active_network_name + (m_info.metered ? " • 📶 Metered (Data-Saver)" : ""));
    m_network_lbl = TextViewBuilder::create()
        ->text(net_str)
        ->caption()
        ->muted()
        ->ellipsize(true)
        ->textAlignment(TextAlignment::Center)
        ->build();
    m_network_lbl->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterHorizontal
    ));
    m_network_lbl->set_margin(0, 6, 0, 3);
    left_col->add_view(m_network_lbl);

    // b) Current configuration summary
    m_mode_tech_desc = TextViewBuilder::create()
        ->text("")
        ->caption()
        ->muted()
        ->multiline(true)
        ->textAlignment(TextAlignment::Center)
        ->build();
    m_mode_tech_desc->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterHorizontal
    ));
    m_mode_tech_desc->set_margin(0, 0, 0, 3);
    left_col->add_view(m_mode_tech_desc);

    // c) Mode explanation
    m_mode_desc = TextViewBuilder::create()
        ->text("")
        ->caption()
        ->multiline(true)
        ->maxLines(3)
        ->ellipsize(true)
        ->textAlignment(TextAlignment::Center)
        ->build();
    m_mode_desc->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterHorizontal
    ));
    m_mode_desc->set_margin(0, 3, 0, 0);
    left_col->add_view(m_mode_desc);

    update_mode_descriptions(m_info.current_mode);

    left_card->add_view(left_col);
    top_cards_row->add_view(left_card);

    // -------------------------------------------------------------------------
    // RIGHT CARD: Allow Custom Port Rule (with TCP/UDP Dropdown)
    // -------------------------------------------------------------------------
    auto right_card = std::make_shared<CardView>();
    right_card->set_padding(16, 16);
    right_card->set_layout_params(LayoutParams(
        0,
        static_cast<int>(LayoutDimension::MatchParent),
        1.0f
    ));
    right_card->set_margin(6, 0, 0, 0);

    auto right_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    right_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));

    auto port_title = TextViewBuilder::create()
        ->text("Allow Custom Port")
        ->h3()
        ->bold(true)
        ->textAlignment(TextAlignment::Center)
        ->build();
    port_title->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterHorizontal
    ));
    port_title->set_margin(0, 0, 0, 10);
    right_col->add_view(port_title);

    // Form row: Port number + Protocol dropdown (Responsive flex)
    auto form_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    form_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterHorizontal
    ));
    form_row->set_margin(0, 0, 0, 10);

    m_input_port = EditTextBuilder::create()
        ->hint("Port (e.g. 5000)")
        ->padding(8, 6)
        ->build();
    m_input_port->set_layout_params(LayoutParams(
        0,
        static_cast<int>(LayoutDimension::WrapContent),
        1.0f
    ));
    m_input_port->set_margin(0, 0, 6, 0);
    form_row->add_view(m_input_port);

    std::vector<std::string> proto_items = {"TCP", "UDP", "TCP & UDP"};
    m_spinner_proto = SpinnerBuilder::create()
        ->items(proto_items)
        ->selectedIndex(0)
        ->padding(8, 6)
        ->onItemSelected([this](int idx, const std::string&) {
            if (idx == 0) m_selected_proto = "tcp";
            else if (idx == 1) m_selected_proto = "udp";
            else m_selected_proto = "any";
        })
        ->build();
    m_spinner_proto->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    form_row->add_view(m_spinner_proto);
    right_col->add_view(form_row);

    // Allow button (Matches card width responsively)
    auto btn_add = ButtonBuilder::create()
        ->text("+ Allow Port Rule")
        ->primary(true)
        ->padding(10, 6)
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
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterHorizontal
    ));
    btn_add->set_margin(0, 0, 0, 6);
    right_col->add_view(btn_add);

    auto port_note = TextViewBuilder::create()
        ->text("Opens the selected inbound port through the firewall.")
        ->caption()
        ->muted()
        ->textAlignment(TextAlignment::Center)
        ->build();
    port_note->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterHorizontal
    ));
    right_col->add_view(port_note);

    right_card->add_view(right_col);
    top_cards_row->add_view(right_card);

    m_layout->add_view(top_cards_row);

    // =========================================================================
    // 2. LOCAL SERVICES SHARING CARDS (2x2 Grid Matrix)
    // =========================================================================
    auto make_service_card = [this](const std::string& icon, const std::string& title,
                                    const std::string& desc, bool is_allowed,
                                    const std::string& port,
                                    std::shared_ptr<Switch>& out_switch,
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

        // Top row inside card: Title on left, Switch on right
        auto top_r = std::make_shared<LinearLayout>(Orientation::Horizontal);
        top_r->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        top_r->set_margin(0, 0, 0, 8);

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
        t_tv->set_margin(0, 0, 8, 0);
        top_r->add_view(t_tv);

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
        top_r->add_view(out_switch);
        col->add_view(top_r);

        // Description below
        auto d_tv = TextViewBuilder::create()
            ->text(desc)
            ->caption()
            ->muted()
            ->multiline(true)
            ->maxLines(3)
            ->ellipsize(true)
            ->build();
        d_tv->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));
        col->add_view(d_tv);

        card->add_view(col);
        return card;
    };

    // Row 1: SSH (Left) & Web Dev (Right)
    auto serv_row1 = std::make_shared<LinearLayout>(Orientation::Horizontal);
    serv_row1->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    serv_row1->set_margin(0, 0, 0, 10);

    auto card_ssh = make_service_card("💻", "Remote Terminal (SSH - 22)",
        "Allow secure command-line login from trusted machines across the network.",
        m_info.ssh_allowed, "22", m_switch_ssh, 0, 6, LayoutDimension::WrapContent);
    serv_row1->add_view(card_ssh);

    auto card_web = make_service_card("🌐", "Local Web Dev (8080)",
        "Allow testing local web servers on this machine from other devices.",
        m_info.web_dev_allowed, "8080", m_switch_web, 6, 0, LayoutDimension::MatchParent);
    serv_row1->add_view(card_web);

    m_layout->add_view(serv_row1);

    // Row 2: Syncthing (Left) + Samba File Sharing (Right)
    auto serv_row2 = std::make_shared<LinearLayout>(Orientation::Horizontal);
    serv_row2->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    serv_row2->set_margin(0, 0, 0, 16);

    auto card_syncthing = make_service_card("🔄", "Syncthing File Sync (22000)",
        "Allow continuous peer-to-peer folder and document synchronization.",
        m_info.syncthing_allowed, "22000", m_switch_syncthing, 0, 6, LayoutDimension::WrapContent);
    serv_row2->add_view(card_syncthing);

    auto card_samba = make_service_card("📁", "LAN File Share (Samba - 445)",
        "Allow Windows and Linux network folder and document sharing.",
        m_info.samba_allowed, "445", m_switch_samba, 6, 0, LayoutDimension::MatchParent);
    serv_row2->add_view(card_samba);

    m_layout->add_view(serv_row2);

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
    guide_card->set_padding(20, 14);
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
        auto empty_card = std::make_shared<CardView>();
        empty_card->set_padding(16, 14);
        empty_card->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));
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
    rules_card->set_padding(16, 12);
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
        r_row->set_margin(2, 6, 2, 6);

        auto act_lbl = TextViewBuilder::create()
            ->text(rule.action == "ALLOW" ? "✔ ALLOW" : "✖ DENY")
            ->bold(true)
            ->build();
        act_lbl->set_margin(0, 0, 12, 0);
        r_row->add_view(act_lbl);

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

void FirewallView::update_info(const FirewallInfo& info) {
    m_info = info;

    if (m_status_icon) {
        m_status_icon->set_text(m_info.active ? "🛡️" : "⚠️");
    }

    if (m_status_lbl) {
        m_status_lbl->set_text(m_info.active ? "Firewall Protection is Active" : "Firewall Protection is Inactive");
    }

    if (m_switch_master) {
        m_switch_master->set_checked(m_info.active);
    }

    if (m_network_lbl) {
        std::string net_str = m_info.active_network_name.empty() ?
            "Network: None detected" :
            ("Network: " + m_info.active_network_name + (m_info.metered ? " • 📶 Metered (Data-Saver)" : ""));
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

    if (m_switch_samba) {
        m_switch_samba->set_checked(m_info.samba_allowed);
    }

    rebuild_rules_list();
}

} // namespace miqusecure

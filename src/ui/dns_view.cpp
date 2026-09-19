#include "dns_view.hpp"
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
    m_layout->set_padding(4, 2);

    // =========================================================================
    // 0. DNS MASTER STATUS PILL (Centered WrapContent Hero Capsule)
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

    // Status Icon
    m_status_icon = TextViewBuilder::create()
        ->text(m_info.encrypted ? "🌐" : "⚠️")
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
    std::string title_text = m_info.encrypted ?
        (m_info.active_provider + " Protection Active") :
        "Standard Router DNS (Unencrypted)";
    m_status_lbl = TextViewBuilder::create()
        ->text(title_text)
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
        ->checked(m_info.encrypted)
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
            bool ok = false;
            if (checked) {
                // Default to Quad9 if master enabled from unencrypted state
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
    status_row->add_view(m_switch_master);

    status_pill->add_view(status_row);
    m_layout->add_view(status_pill);

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
    m_progress_bar->set_margin(0, 0, 0, 12);
    m_progress_bar->set_visibility(Visibility::Invisible);
    m_layout->add_view(m_progress_bar);

    // =========================================================================
    // 1. 1-CLICK TRUSTED PRIVACY RESOLVERS (2x2 Grid Matrix)
    // =========================================================================
    auto rec_title = TextViewBuilder::create()
        ->text("1-Click Privacy & Security Resolvers")
        ->h3()
        ->bold(true)
        ->build();
    rec_title->set_margin(0, 0, 0, 8);
    m_layout->add_view(rec_title);

    auto make_dns_card = [this](const std::string& icon, const std::string& title,
                                const std::string& desc,
                                const std::string& primary_ip, const std::string& secondary_ip,
                                bool is_active,
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

            std::thread([this, primary_ip, secondary_ip, checked, weak_sw]() {
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

    bool is_quad9 = m_info.active_provider.find("Quad9") != std::string::npos;
    bool is_adguard = m_info.active_provider.find("AdGuard") != std::string::npos;
    bool is_mullvad = m_info.active_provider.find("Mullvad") != std::string::npos;
    bool is_cloudflare = m_info.active_provider.find("Cloudflare") != std::string::npos;

    // Row 1: Quad9 (Left) & AdGuard (Right)
    auto dns_row1 = std::make_shared<LinearLayout>(Orientation::Horizontal);
    dns_row1->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    dns_row1->set_margin(0, 0, 0, 10);

    auto card_quad9 = make_dns_card("🛡️", "Quad9 Threat Shield",
        "Automated threat, phishing & ransomware protection via Swiss non-profit.",
        "9.9.9.9", "149.112.112.112",
        is_quad9, m_switch_quad9, 0, 6, LayoutDimension::WrapContent);
    dns_row1->add_view(card_quad9);

    auto card_adguard = make_dns_card("🚫", "AdGuard Ad-Block",
        "Network-level blocking of advertising servers, tracking scripts, and telemetry domains.",
        "94.140.14.14", "94.140.15.15",
        is_adguard, m_switch_adguard, 6, 0, LayoutDimension::MatchParent);
    dns_row1->add_view(card_adguard);

    m_layout->add_view(dns_row1);

    // Row 2: Mullvad (Left) & Cloudflare (Right)
    auto dns_row2 = std::make_shared<LinearLayout>(Orientation::Horizontal);
    dns_row2->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    dns_row2->set_margin(0, 0, 0, 16);

    auto card_mullvad = make_dns_card("🔒", "Mullvad Privacy",
        "Strict Swedish privacy jurisdiction with zero logs, DNSSEC, and QNAME minimization.",
        "194.242.2.4", "194.242.2.5",
        is_mullvad, m_switch_mullvad, 0, 6, LayoutDimension::WrapContent);
    dns_row2->add_view(card_mullvad);

    auto card_cloudflare = make_dns_card("⚡", "Cloudflare 1.1.1.1",
        "Fastest worldwide response times with regular independent public privacy audits.",
        "1.1.1.1", "1.0.0.1",
        is_cloudflare, m_switch_cloudflare, 6, 0, LayoutDimension::MatchParent);
    dns_row2->add_view(card_cloudflare);

    m_layout->add_view(dns_row2);

    // =========================================================================
    // 2. CUSTOM DNS SERVER CARD (Full-Width Responsive Form)
    // =========================================================================
    auto custom_card = std::make_shared<CardView>();
    custom_card->set_padding(16, 14);
    custom_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    custom_card->set_margin(0, 0, 0, 16);

    auto custom_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    custom_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto custom_title = TextViewBuilder::create()
        ->text("⚙ Custom DNS Server")
        ->bold(true)
        ->build();
    custom_title->set_margin(0, 0, 0, 10);
    custom_col->add_view(custom_title);

    // Form row: Primary & Secondary IP Inputs & Apply Button
    auto form_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    form_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    form_row->set_margin(0, 0, 0, 8);

    m_input_primary = EditTextBuilder::create()
        ->hint("Primary IP (e.g. 1.1.1.1)")
        ->padding(8, 6)
        ->build();
    m_input_primary->set_layout_params(LayoutParams(
        0,
        static_cast<int>(LayoutDimension::WrapContent),
        1.0f
    ));
    m_input_primary->set_margin(0, 0, 8, 0);
    form_row->add_view(m_input_primary);

    m_input_secondary = EditTextBuilder::create()
        ->hint("Secondary IP (optional)")
        ->padding(8, 6)
        ->build();
    m_input_secondary->set_layout_params(LayoutParams(
        0,
        static_cast<int>(LayoutDimension::WrapContent),
        1.0f
    ));
    m_input_secondary->set_margin(0, 0, 8, 0);
    form_row->add_view(m_input_secondary);

    auto btn_apply_custom = ButtonBuilder::create()
        ->text("+ Apply DNS")
        ->primary(true)
        ->padding(12, 6)
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
        ->build();
    custom_col->add_view(custom_note);

    custom_card->add_view(custom_col);
    m_layout->add_view(custom_card);

    // =========================================================================
    // 3. EDUCATIONAL "WHAT IS DNS?" (Soft Tinted, Borderless Callout)
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
    auto g_title = TextViewBuilder::create()->text("💡 What is DNS & Why Does It Matter?")->bold(true)->build();
    auto g_desc = TextViewBuilder::create()
        ->text("DNS is the directory of the internet. When querying an address like wikipedia.org, DNS looks up the exact server IP to connect to.\n\nBy default, the Internet Service Provider (ISP) resolves these lookups, which allows recording and logging every website visited—even in private browsing mode. Using a trusted encrypted resolver like Quad9 or Cloudflare shields network queries from ISP inspection and automatically blocks malicious fraud and phishing domains.")
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

void DnsView::update_info(const DnsInfo& info) {
    m_info = info;

    if (m_status_icon) {
        m_status_icon->set_text(m_info.encrypted ? "🌐" : "⚠️");
    }

    if (m_status_lbl) {
        std::string title_text = m_info.encrypted ?
            (m_info.active_provider + " Protection Active") :
            "Standard Router DNS (Unencrypted)";
        m_status_lbl->set_text(title_text);
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

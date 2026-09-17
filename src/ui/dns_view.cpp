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
    // 1. ACTIVE DNS STATUS & REVERT (Clean Borderless Hero)
    // =========================================================================
    auto status_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    status_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    status_row->set_margin(0, 4, 0, 16);

    auto status_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    status_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    m_mode_lbl = TextViewBuilder::create()
        ->text(m_info.encrypted ? ("🌐 " + m_info.active_provider + " (Active)") : "⚠️ Standard Router DNS (Unencrypted)")
        ->h2()
        ->bold(true)
        ->build();

    std::string desc_str = m_info.encrypted ?
        "DNS queries are filtered and protected against ISP surveillance." :
        "DNS queries are unencrypted and visible to the Internet Provider.";

    m_status_desc = TextViewBuilder::create()
        ->text(desc_str)
        ->caption()
        ->muted()
        ->build();
    m_status_desc->set_margin(0, 2, 0, 0);

    std::string conn_str = m_info.active_connection_name.empty() ?
        "Active Network: None detected" :
        ("Active Network: " + m_info.active_connection_name);

    m_conn_lbl = TextViewBuilder::create()
        ->text(conn_str)
        ->caption()
        ->muted()
        ->build();
    m_conn_lbl->set_margin(0, 2, 0, 0);

    status_col->add_view(m_mode_lbl);
    status_col->add_view(m_status_desc);
    status_col->add_view(m_conn_lbl);
    status_row->add_view(status_col);

    auto btn_revert = ButtonBuilder::create()
        ->text("↺ Use Router DNS")
        ->flat(true)
        ->padding(10, 6)
        ->onClick([this]() {
            if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);
            if (m_status_desc) m_status_desc->set_text("Reverting to default router DNS...");
            std::thread([this]() {
                bool ok = SecurityBackend::restore_default_dns();
                if (auto engine = AppEngine::instance()) {
                    engine->post([this, ok]() {
                        if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Invisible);
                        if (ok && m_on_changed) m_on_changed();
                    });
                }
            }).detach();
        })
        ->build();
    status_row->add_view(btn_revert);

    m_layout->add_view(status_row);

    // Material 3 docked linear progress indicator (3px, zero layout shift)
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
    m_progress_bar->set_margin(0, 0, 0, 8);
    m_layout->add_view(m_progress_bar);

    // =========================================================================
    // 2. 1-CLICK TRUSTED PRIVACY RESOLVERS (ONE Grouped Card with Dividers)
    // =========================================================================
    auto rec_title = TextViewBuilder::create()
        ->text("1-Click Encrypted & Privacy Resolvers")
        ->h3()
        ->bold(true)
        ->build();
    rec_title->set_margin(0, 0, 0, 8);
    m_layout->add_view(rec_title);

    m_profiles_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_profiles_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    rebuild_profiles_list();
    m_layout->add_view(m_profiles_container);

    // =========================================================================
    // 3. EDUCATIONAL "WHAT IS DNS?" (Soft Tinted, Borderless Callout)
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

void DnsView::rebuild_profiles_list() {
    if (!m_profiles_container) return;
    m_profiles_container->clear_views();

    auto unified_card = std::make_shared<CardView>();
    unified_card->set_padding(14, 12);
    unified_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    unified_card->set_margin(0, 0, 0, 16);

    auto card_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    card_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    // Top row: Title on left, Spinner on right
    auto header_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    header_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    auto title_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    title_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    auto t_tv = TextViewBuilder::create()
        ->text("Active DNS Resolver")
        ->bold(true)
        ->build();
    auto s_tv = TextViewBuilder::create()
        ->text("Select secure resolver or network default")
        ->caption()
        ->muted()
        ->build();
    title_col->add_view(t_tv);
    title_col->add_view(s_tv);
    header_row->add_view(title_col);

    std::vector<std::string> dns_items = {
        "🛡️ Quad9 (Threat Block)",
        "⚡ Cloudflare 1.1.1.1",
        "🔒 Mullvad Privacy",
        "🚫 AdGuard Ad-Block",
        "📡 Router Default (ISP)"
    };

    m_spinner_dns = SpinnerBuilder::create()
        ->items(dns_items)
        ->selectedIndex(get_profile_index(m_info))
        ->padding(12, 8)
        ->onItemSelected([this](int idx, const std::string&) {
            if (m_updating_ui) return;
            update_profile_descriptions(idx);

            if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);
            if (m_status_desc) m_status_desc->set_text("Configuring DNS resolvers and applying changes...");

            std::thread([this, idx]() {
                bool ok = false;
                if (idx == 0) ok = SecurityBackend::apply_dns("9.9.9.9", "149.112.112.112");
                else if (idx == 1) ok = SecurityBackend::apply_dns("1.1.1.1", "1.0.0.1");
                else if (idx == 2) ok = SecurityBackend::apply_dns("194.242.2.4", "194.242.2.5");
                else if (idx == 3) ok = SecurityBackend::apply_dns("94.140.14.14", "94.140.15.15");
                else ok = SecurityBackend::restore_default_dns();

                if (auto engine = AppEngine::instance()) {
                    engine->post([this, ok]() {
                        if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Invisible);
                        if (ok) {
                            if (m_on_changed) m_on_changed();
                        } else {
                            m_updating_ui = true;
                            if (m_spinner_dns) {
                                m_spinner_dns->set_selected_index(get_profile_index(m_info));
                            }
                            update_profile_descriptions(get_profile_index(m_info));
                            m_updating_ui = false;
                        }
                    });
                }
            }).detach();
        })
        ->build();

    m_spinner_dns->set_layout_params(LayoutParams(
        270,
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    header_row->add_view(m_spinner_dns);
    card_col->add_view(header_row);

    // Divider
    card_col->add_view(DividerViewBuilder::create()->margin(0, 10)->build());

    // Description & Technical details container
    m_profile_desc = TextViewBuilder::create()
        ->text("")
        ->caption()
        ->multiline(true)
        ->build();
    m_profile_desc->set_margin(0, 0, 0, 4);

    m_profile_tech_desc = TextViewBuilder::create()
        ->text("")
        ->caption()
        ->muted()
        ->ellipsize(true)
        ->build();

    card_col->add_view(m_profile_desc);
    card_col->add_view(m_profile_tech_desc);

    update_profile_descriptions(get_profile_index(m_info));

    unified_card->add_view(card_col);
    m_profiles_container->add_view(unified_card);
}

void DnsView::update_profile_descriptions(int idx) {
    if (!m_profile_desc || !m_profile_tech_desc) return;

    switch (idx) {
        case 0:
            m_profile_desc->set_text("Automated threat & phishing protection. Swiss non-profit with zero logging policy.");
            m_profile_tech_desc->set_text("⚙ Primary: 9.9.9.9 • Secondary: 149.112.112.112 • DNSSEC validated");
            break;
        case 1:
            m_profile_desc->set_text("Fastest worldwide response times with regular independent public privacy audits.");
            m_profile_tech_desc->set_text("⚙ Primary: 1.1.1.1 • Secondary: 1.0.0.1 • Anycast network");
            break;
        case 2:
            m_profile_desc->set_text("Strict Swedish privacy jurisdiction with zero logs, DNSSEC, and QNAME minimization.");
            m_profile_tech_desc->set_text("⚙ Primary: 194.242.2.4 • Secondary: 194.242.2.5 • No telemetry");
            break;
        case 3:
            m_profile_desc->set_text("Built-in network-level blocking of ad servers, tracking scripts, and telemetry domains.");
            m_profile_tech_desc->set_text("⚙ Primary: 94.140.14.14 • Secondary: 94.140.15.15 • Filtered");
            break;
        case 4:
        default:
            m_profile_desc->set_text("Standard unencrypted DNS provided by your local Wi-Fi router or ISP DHCP server.");
            m_profile_tech_desc->set_text("⚙ ISP Default • Queries may be logged by network operator");
            break;
    }
}

int DnsView::get_profile_index(const DnsInfo& info) {
    if (info.active_provider.find("Quad9") != std::string::npos) return 0;
    if (info.active_provider.find("Cloudflare") != std::string::npos) return 1;
    if (info.active_provider.find("Mullvad") != std::string::npos) return 2;
    if (info.active_provider.find("AdGuard") != std::string::npos) return 3;
    return 4; // Router Default
}

void DnsView::update_info(const DnsInfo& info) {
    m_info = info;

    if (m_mode_lbl) {
        m_mode_lbl->set_text(m_info.encrypted ? ("🌐 " + m_info.active_provider + " (Active)") : "⚠️ Standard Router DNS (Unencrypted)");
    }

    if (m_status_desc) {
        std::string desc_str = m_info.encrypted ?
            "DNS queries are filtered and protected against ISP surveillance." :
            "DNS queries are unencrypted and visible to the Internet Provider.";
        m_status_desc->set_text(desc_str);
    }

    if (m_conn_lbl) {
        std::string conn_str = m_info.active_connection_name.empty() ?
            "Active Network: None detected" :
            ("Active Network: " + m_info.active_connection_name);
        m_conn_lbl->set_text(conn_str);
    }

    m_updating_ui = true;
    if (m_spinner_dns) {
        m_spinner_dns->set_selected_index(get_profile_index(m_info));
    }
    update_profile_descriptions(get_profile_index(m_info));
    m_updating_ui = false;
}

} // namespace miqusecure

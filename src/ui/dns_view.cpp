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

    struct ProfileDef {
        std::string name;
        std::string key_id;
        std::string desc;
        std::string p_ip;
        std::string s_ip;
    };

    std::vector<ProfileDef> profiles = {
        {"Quad9 (Recommended)", "Quad9", "Automated threat & phishing protection. Swiss non-profit with zero logging.", "9.9.9.9", "149.112.112.112"},
        {"Cloudflare 1.1.1.1", "Cloudflare", "Fastest worldwide response times. Regular independent privacy audits.", "1.1.1.1", "1.0.0.1"},
        {"Mullvad DNS", "Mullvad", "Strict Swedish privacy jurisdiction. Zero logs, DNSSEC and QNAME minimization.", "194.242.2.4", "194.242.2.5"},
        {"AdGuard DNS", "AdGuard", "Built-in ad, tracking script, and telemetry domain blocker.", "94.140.14.14", "94.140.15.15"}
    };

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

    for (size_t i = 0; i < profiles.size(); ++i) {
        const auto& prof = profiles[i];
        auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        row->set_margin(4, 6, 4, 6);

        auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
        col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

        auto name_tv = TextViewBuilder::create()->text(prof.name)->bold(true)->build();
        auto desc_tv = TextViewBuilder::create()->text(prof.desc + " (" + prof.p_ip + ")")->caption()->muted()->multiline(true)->maxLines(2)->ellipsize(true)->build();

        col->add_view(name_tv);
        col->add_view(desc_tv);
        row->add_view(col);

        bool is_active = (m_info.active_provider.find(prof.key_id) != std::string::npos);
        std::string p1 = prof.p_ip;
        std::string p2 = prof.s_ip;

        auto sw = SwitchBuilder::create()
            ->checked(is_active)
            ->build();
        std::weak_ptr<Switch> weak_sw = sw;
        sw->set_on_checked_changed_listener([this, p1, p2, weak_sw](bool checked) {
            if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);
            if (m_status_desc) m_status_desc->set_text("Configuring DNS resolvers and applying changes...");
            std::thread([this, p1, p2, checked, weak_sw]() {
                bool ok = checked ? SecurityBackend::apply_dns(p1, p2) : SecurityBackend::restore_default_dns();
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
        row->add_view(sw);

        list_col->add_view(row);
        if (i + 1 < profiles.size()) {
            list_col->add_view(DividerViewBuilder::create()->margin(0, 4)->build());
        }
    }

    unified_card->add_view(list_col);
    m_profiles_container->add_view(unified_card);
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

    rebuild_profiles_list();
}

} // namespace miqusecure

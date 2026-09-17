#include "traffic_view.hpp"
#include <iomanip>
#include <sstream>

using namespace miqu;

namespace miqusecure {

static std::string get_proc_icon(const std::string& name) {
    std::string lower = name;
    for (char& c : lower) c = std::tolower(c);

    if (lower.find("firefox") != std::string::npos || lower.find("browser") != std::string::npos) return "🦊";
    if (lower.find("music") != std::string::npos || lower.find("mpd") != std::string::npos || lower.find("spotify") != std::string::npos) return "🎵";
    if (lower.find("antigravity") != std::string::npos || lower.find("code") != std::string::npos) return "⚡";
    if (lower.find("server") != std::string::npos || lower.find("daemon") != std::string::npos) return "⚙️";
    if (lower.find("waybar") != std::string::npos || lower.find("miqu") != std::string::npos) return "💻";
    if (lower.find("curl") != std::string::npos || lower.find("wget") != std::string::npos) return "📦";
    if (lower.find("systemd") != std::string::npos) return "🛡️";
    return "🌐";
}

TrafficView::TrafficView(const TrafficReport& traffic)
    : m_traffic(traffic) {
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
    // 1. TOP METRIC CARDS STRIP
    // =========================================================================
    auto metrics_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    metrics_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    metrics_row->set_margin(0, 0, 0, 14);

    auto make_metric_card = [](const std::string& icon, const std::string& label, std::shared_ptr<TextView>& out_val) {
        auto card = std::make_shared<CardView>();
        card->set_layout_params(LayoutParams(
            0,
            static_cast<int>(LayoutDimension::WrapContent),
            1.0f
        ));
        card->set_padding(12, 10);
        card->set_margin(4, 0, 4, 0);

        auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
        auto hdr_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        hdr_row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));

        auto icon_tv = TextViewBuilder::create()->text(icon)->caption()->build();
        icon_tv->set_margin(0, 0, 6, 0);
        auto lbl_tv = TextViewBuilder::create()->text(label)->caption()->muted()->ellipsize(true)->build();

        hdr_row->add_view(icon_tv);
        hdr_row->add_view(lbl_tv);
        col->add_view(hdr_row);

        out_val = TextViewBuilder::create()->text("--")->h2()->bold(true)->ellipsize(true)->build();
        out_val->set_margin(0, 4, 0, 0);
        col->add_view(out_val);

        card->add_view(col);
        return card;
    };

    metrics_row->add_view(make_metric_card("🌐", "Outbound Sockets", m_outbound_badge));
    metrics_row->add_view(make_metric_card("🛡️", "TLS Encrypted", m_encrypted_badge));
    metrics_row->add_view(make_metric_card("⚡", "Live Throughput", m_bandwidth_badge));
    metrics_row->add_view(make_metric_card("📦", "Active Apps", m_apps_badge));
    m_layout->add_view(metrics_row);

    // =========================================================================
    // 2. FILTER & CONTROLS BAR
    // =========================================================================
    auto filter_card = std::make_shared<CardView>();
    filter_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    filter_card->set_padding(10, 8);
    filter_card->set_margin(0, 0, 0, 14);

    auto filter_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    filter_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    std::vector<std::string> scope_items = {
        "🌐 Internet Only",
        "🔍 All Traffic (inc. Local)",
        "💻 Localhost Only",
        "🛡️ Encrypted Only (TLS)"
    };

    m_spinner_scope = SpinnerBuilder::create()
        ->items(scope_items)
        ->selectedIndex(static_cast<int>(m_scope_mode))
        ->padding(10, 6)
        ->onItemSelected([this](int idx, const std::string&) {
            m_scope_mode = static_cast<TrafficScopeMode>(idx);
            rebuild_active_connections();
        })
        ->build();
    m_spinner_scope->set_layout_params(LayoutParams(
        230,
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_spinner_scope->set_margin(0, 0, 10, 0);
    filter_row->add_view(m_spinner_scope);

    m_search_input = std::make_shared<EditText>();
    m_search_input->set_hint("Filter by app, domain, or IP...");
    m_search_input->set_layout_params(LayoutParams(
        0,
        36,
        1.0f
    ));
    m_search_input->set_on_text_changed_listener([this](std::shared_ptr<EditText>, const std::string& text) {
        m_search_filter = text;
        rebuild_active_connections();
    });
    filter_row->add_view(m_search_input);

    auto live_badge = TextViewBuilder::create()
        ->text("● Press R or ↻ to Refresh")
        ->caption()
        ->muted()
        ->build();
    live_badge->set_margin(12, 0, 4, 0);
    filter_row->add_view(live_badge);

    filter_card->add_view(filter_row);
    m_layout->add_view(filter_card);

    // =========================================================================
    // 3. RECENT REQUEST ACTIVITY STREAM
    // =========================================================================
    auto feed_hdr = TextViewBuilder::create()
        ->text("⚡ Live Request Stream (Recently Initiated)")
        ->h3()
        ->bold(true)
        ->build();
    feed_hdr->set_margin(0, 0, 0, 8);
    m_layout->add_view(feed_hdr);

    m_feed_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_feed_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_feed_container->set_margin(0, 0, 0, 16);
    m_layout->add_view(m_feed_container);

    // =========================================================================
    // 4. ACTIVE NETWORK SOCKETS TABLE
    // =========================================================================
    auto sock_hdr = TextViewBuilder::create()
        ->text("Active Network Sockets")
        ->h3()
        ->bold(true)
        ->build();
    sock_hdr->set_margin(0, 0, 0, 8);
    m_layout->add_view(sock_hdr);

    m_conn_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_conn_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_conn_container->set_margin(0, 0, 0, 16);
    m_layout->add_view(m_conn_container);

    // =========================================================================
    // 5. EDUCATIONAL CALLOUT (Soft Tinted Card)
    // =========================================================================
    auto cfg = Config::get();
    auto guide_card = std::make_shared<FrameLayout>();
    guide_card->set_background_color(cfg->colors.surface_variant);
    guide_card->set_corner_radius(10);
    guide_card->set_padding(14, 10);
    guide_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto guide_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    auto g_title = TextViewBuilder::create()->text("💡 Why Monitor Live Network Traffic?")->bold(true)->build();
    auto g_desc = TextViewBuilder::create()
        ->text("Even when the screen is locked or idle, background processes, telemetry daemons, and browser extensions regularly establish outbound connections to remote servers.\n\nMonitoring live socket requests allows verifying which applications communicate over the network, detecting unexpected data transfers, and confirming that sensitive web traffic is encrypted with TLS (HTTPS / DoT).")
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
    update_info(m_traffic);
}

void TrafficView::update_info(const TrafficReport& traffic) {
    m_traffic = traffic;

    // 1. Update metric badges
    if (m_outbound_badge) {
        m_outbound_badge->set_text(std::to_string(m_traffic.total_outbound));
    }
    if (m_encrypted_badge) {
        int pct = (m_traffic.total_outbound > 0) ? (m_traffic.total_encrypted * 100 / m_traffic.total_outbound) : 100;
        m_encrypted_badge->set_text(std::to_string(pct) + "%");
    }
    if (m_bandwidth_badge) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << "↓" << m_traffic.rx_rate_kb << " ↑" << m_traffic.tx_rate_kb;
        m_bandwidth_badge->set_text(ss.str());
    }
    if (m_apps_badge) {
        m_apps_badge->set_text(std::to_string(m_traffic.total_apps));
    }

    rebuild_recent_feed();
    rebuild_active_connections();
}

void TrafficView::rebuild_recent_feed() {
    if (!m_feed_container) return;
    m_feed_container->clear_views();

    auto feed_card = std::make_shared<CardView>();
    feed_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    feed_card->set_padding(14, 8);

    auto list_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    list_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    if (m_traffic.recent_events.empty()) {
        auto empty_tv = TextViewBuilder::create()
            ->text("Listening for outbound requests... New socket connections will appear here in real time.")
            ->caption()
            ->muted()
            ->build();
        list_col->add_view(empty_tv);
    } else {
        size_t count = std::min(m_traffic.recent_events.size(), static_cast<size_t>(10));
        for (size_t i = 0; i < count; ++i) {
            const auto& event = m_traffic.recent_events[i];
            auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
            row->set_layout_params(LayoutParams(
                static_cast<int>(LayoutDimension::MatchParent),
                static_cast<int>(LayoutDimension::WrapContent),
                Gravity::CenterVertical
            ));
            row->set_margin(4, 5, 4, 5);

            auto time_tv = TextViewBuilder::create()->text(event.timestamp)->caption()->muted()->build();
            time_tv->set_margin(0, 0, 10, 0);
            row->add_view(time_tv);

            auto icon_tv = TextViewBuilder::create()->text(get_proc_icon(event.process_name))->caption()->build();
            icon_tv->set_margin(0, 0, 6, 0);
            row->add_view(icon_tv);

            auto proc_tv = TextViewBuilder::create()->text(event.process_name)->bold(true)->caption()->build();
            proc_tv->set_margin(0, 0, 8, 0);
            row->add_view(proc_tv);

            auto arrow_tv = TextViewBuilder::create()->text("→")->caption()->muted()->build();
            arrow_tv->set_margin(0, 0, 8, 0);
            row->add_view(arrow_tv);

            auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
            col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

            std::string dest_label = event.remote_host;
            if (dest_label.empty() || dest_label == event.remote_addr) {
                dest_label = event.remote_addr + ":" + std::to_string(event.remote_port);
            } else {
                dest_label += " (" + event.remote_addr + ":" + std::to_string(event.remote_port) + ")";
            }

            auto target_tv = TextViewBuilder::create()->text(dest_label)->caption()->ellipsize(true)->build();
            col->add_view(target_tv);
            row->add_view(col);

            std::string badge_str = event.is_encrypted ? ("🛡️ " + event.service_name) : ("⚠️ " + event.service_name);
            auto badge = TextViewBuilder::create()->text(badge_str)->caption()->bold(true)->build();
            row->add_view(badge);

            list_col->add_view(row);
            if (i + 1 < count) {
                list_col->add_view(DividerViewBuilder::create()->margin(0, 3)->build());
            }
        }
    }

    feed_card->add_view(list_col);
    m_feed_container->add_view(feed_card);
}

void TrafficView::rebuild_active_connections() {
    if (!m_conn_container) return;
    m_conn_container->clear_views();

    auto conn_card = std::make_shared<CardView>();
    conn_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    conn_card->set_padding(14, 8);

    auto list_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    list_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    std::string q = m_search_filter;
    for (char& c : q) c = std::tolower(c);

    std::vector<NetworkConnection> filtered;
    for (const auto& conn : m_traffic.active_connections) {
        if (m_scope_mode == TrafficScopeMode::InternetOnly && conn.is_loopback) {
            continue;
        } else if (m_scope_mode == TrafficScopeMode::LocalOnly && !conn.is_loopback) {
            continue;
        } else if (m_scope_mode == TrafficScopeMode::EncryptedOnly && !conn.is_encrypted) {
            continue;
        }

        if (!q.empty()) {
            std::string p_lower = conn.process_name;
            for (char& c : p_lower) c = std::tolower(c);
            std::string h_lower = conn.remote_host;
            for (char& c : h_lower) c = std::tolower(c);
            std::string a_lower = conn.remote_addr;

            if (p_lower.find(q) == std::string::npos &&
                h_lower.find(q) == std::string::npos &&
                a_lower.find(q) == std::string::npos &&
                std::to_string(conn.pid).find(q) == std::string::npos &&
                std::to_string(conn.remote_port).find(q) == std::string::npos) {
                continue;
            }
        }
        filtered.push_back(conn);
    }

    if (filtered.empty()) {
        std::string empty_msg;
        switch (m_scope_mode) {
            case TrafficScopeMode::InternetOnly:
                empty_msg = "No active outbound internet connections matching filter.";
                break;
            case TrafficScopeMode::LocalOnly:
                empty_msg = "No active localhost/loopback connections matching filter.";
                break;
            case TrafficScopeMode::EncryptedOnly:
                empty_msg = "No encrypted TLS connections matching filter.";
                break;
            case TrafficScopeMode::All:
            default:
                empty_msg = "No active network sockets detected.";
                break;
        }
        auto empty_tv = TextViewBuilder::create()
            ->text(empty_msg)
            ->caption()
            ->muted()
            ->build();
        list_col->add_view(empty_tv);
    } else {
        for (size_t i = 0; i < filtered.size(); ++i) {
            const auto& conn = filtered[i];
            auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
            row->set_layout_params(LayoutParams(
                static_cast<int>(LayoutDimension::MatchParent),
                static_cast<int>(LayoutDimension::WrapContent),
                Gravity::CenterVertical
            ));
            row->set_margin(4, 6, 4, 6);

            auto icon_tv = TextViewBuilder::create()->text(get_proc_icon(conn.process_name))->h3()->build();
            icon_tv->set_margin(0, 0, 10, 0);
            row->add_view(icon_tv);

            auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
            col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

            auto name_tv = TextViewBuilder::create()
                ->text(conn.process_name + (conn.pid > 0 ? (" (PID " + std::to_string(conn.pid) + ")") : ""))
                ->bold(true)
                ->build();

            std::string dest_desc = conn.remote_host;
            if (dest_desc.empty() || dest_desc == conn.remote_addr) {
                dest_desc = conn.remote_addr + ":" + std::to_string(conn.remote_port);
            } else {
                dest_desc += " • " + conn.remote_addr + ":" + std::to_string(conn.remote_port);
            }

            auto dest_tv = TextViewBuilder::create()
                ->text(dest_desc)
                ->caption()
                ->muted()
                ->ellipsize(true)
                ->build();

            col->add_view(name_tv);
            col->add_view(dest_tv);
            row->add_view(col);

            std::string svc_badge = conn.is_encrypted ? ("🛡️ " + conn.service_name) : (conn.is_loopback ? "💻 Local" : ("⚠️ " + conn.service_name));
            auto svc_tv = TextViewBuilder::create()
                ->text(svc_badge)
                ->caption()
                ->bold(true)
                ->build();
            svc_tv->set_margin(0, 0, 10, 0);
            row->add_view(svc_tv);

            if (conn.pid > 1) {
                int pid = conn.pid;
                auto btn_end = ButtonBuilder::create()
                    ->text("🛑 End")
                    ->flat(true)
                    ->bold(true)
                    ->padding(8, 4)
                    ->onClick([pid, this]() {
                        SecurityBackend::terminate_process(pid, false);
                    })
                    ->build();
                row->add_view(btn_end);
            }

            list_col->add_view(row);
            if (i + 1 < filtered.size()) {
                list_col->add_view(DividerViewBuilder::create()->margin(0, 4)->build());
            }
        }
    }

    conn_card->add_view(list_col);
    m_conn_container->add_view(conn_card);
}

} // namespace miqusecure

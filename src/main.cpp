#include "backend/security_backend.hpp"
#include "ui/firewall_view.hpp"
#include "ui/traffic_view.hpp"
#include "ui/dns_view.hpp"
#include "ui/metadata_view.hpp"
#include "ui/hardware_view.hpp"
#include "ui/sandbox_view.hpp"
#include "ui/testbed_view.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <iostream>
#include <memory>
#include <array>

using namespace miqu;
using namespace miqusecure;

int main(int argc, char** argv) {
    auto engine = AppEngine::create();
    if (!engine) {
        std::cerr << "[miqusecure] Failed to initialize AppEngine.\n";
        return 1;
    }

    FirewallInfo initial_fw = SecurityBackend::read_firewall();

    auto config = Config::get();
    std::shared_ptr<Window> window;

    // =========================================================================
    // UI BUILDER - ROOT CONTAINER (Matching miqugallery)
    // =========================================================================
    auto root_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    root_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    root_container->set_padding(0);


    // Forward declarations for view references
    std::shared_ptr<BottomNavigationView> bottom_nav;
    std::shared_ptr<ViewPager> pager;
    std::shared_ptr<FirewallView> firewall_view;
    std::shared_ptr<TrafficView> traffic_view;
    std::shared_ptr<DnsView> dns_view;
    std::shared_ptr<MetadataView> metadata_view;
    std::shared_ptr<HardwareView> hardware_view;
    std::shared_ptr<SandboxView> sandbox_view;
    std::shared_ptr<TestbedView> testbed_view;

    const std::array<std::string, 7> SECTION_SUBTITLES = {
        "Firewall Rules & Network Defense",
        "Active Sockets & Bandwidth Monitor",
        "Encrypted Resolvers & Leak Prevention",
        "mat2 Scrubber & Document Sanitizer",
        "Killswitches & Sensor Access",
        "Kernel LSMs & Flatpak Confinement",
        "Distrobox Disposable Environments"
    };

    std::shared_ptr<Toolbar> toolbar;

    auto update_section_subtitle = [&](int idx) {
        if (toolbar && idx >= 0 && idx < static_cast<int>(SECTION_SUBTITLES.size())) {
            toolbar->set_subtitle(SECTION_SUBTITLES[idx]);
        }
    };

    std::array<bool, 7> page_loaded = {true, false, false, false, false, false, false};

    auto load_page_if_needed = [&](int idx) {
        if (idx >= 0 && idx < 7 && !page_loaded[idx]) {
            page_loaded[idx] = true;
            switch (idx) {
                case 1:
                    if (traffic_view) traffic_view->update_info(SecurityBackend::read_traffic());
                    break;
                case 2:
                    if (dns_view) dns_view->update_info(SecurityBackend::read_dns());
                    break;
                case 3:
                    if (metadata_view) metadata_view->update_info(SecurityBackend::read_metadata());
                    break;
                case 4:
                    if (hardware_view) hardware_view->update_info(SecurityBackend::read_hardware());
                    break;
                case 5:
                    if (sandbox_view) sandbox_view->update_info(SecurityBackend::read_lsm());
                    break;
                case 6:
                    if (testbed_view) testbed_view->update_info(SecurityBackend::read_testbed());
                    break;
                default:
                    break;
            }
            if (window) window->schedule_redraw();
        }
    };

    auto refresh_fn = [&]() {
        int current_page = pager ? pager->get_current_page() : 0;
        switch (current_page) {
            case 0:
                if (firewall_view) firewall_view->update_info(SecurityBackend::read_firewall());
                break;
            case 1:
                if (traffic_view) traffic_view->update_info(SecurityBackend::read_traffic());
                break;
            case 2:
                if (dns_view) dns_view->update_info(SecurityBackend::read_dns());
                break;
            case 3:
                if (metadata_view) metadata_view->update_info(SecurityBackend::read_metadata());
                break;
            case 4:
                if (hardware_view) hardware_view->update_info(SecurityBackend::read_hardware());
                break;
            case 5:
                if (sandbox_view) sandbox_view->update_info(SecurityBackend::read_lsm());
                break;
            case 6:
                if (testbed_view) testbed_view->update_info(SecurityBackend::read_testbed());
                break;
            default:
                break;
        }
        if (window) window->schedule_redraw();
    };

    // -------------------------------------------------------------------------
    // 1. TOP TOOLBAR
    // -------------------------------------------------------------------------
    toolbar = ToolbarBuilder::create()
        ->title("Security & Privacy")
        ->subtitle(SECTION_SUBTITLES[0])
        ->titleAlignment(TitleAlignment::Center)
        ->onRefresh(refresh_fn)
        ->onClose([engine]() {
            engine->quit();
        })
        ->build();
    toolbar->set_margin(14, 12, 14, 8);
    root_container->add_view(toolbar);

    // =========================================================================
    // 2. VIEW PAGER PAGES (7 DEDICATED SECTIONS)
    // =========================================================================
    pager = ViewPagerBuilder::create()
        ->currentPage(0)
        ->onPageChanged([&](int old_idx, int new_idx) {
            if (bottom_nav && bottom_nav->get_selected_index() != new_idx) {
                bottom_nav->set_selected_index(new_idx);
            }
            update_section_subtitle(new_idx);
            load_page_if_needed(new_idx);
        })
        ->build();
    pager->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        0,
        1.0f
    ));
    pager->set_margin(14, 0, 14, 0);

    // Page 0: Firewall
    firewall_view = std::make_shared<FirewallView>(initial_fw, refresh_fn);
    pager->add_page(firewall_view);

    // Page 1: Live Traffic
    TrafficReport dummy_traffic;
    traffic_view = std::make_shared<TrafficView>(dummy_traffic);
    pager->add_page(traffic_view);

    // Page 2: DNS Privacy
    DnsInfo dummy_dns;
    dns_view = std::make_shared<DnsView>(dummy_dns, refresh_fn);
    pager->add_page(dns_view);

    // Page 3: Metadata Scrubber
    MetadataInfo dummy_meta;
    metadata_view = std::make_shared<MetadataView>(dummy_meta);
    pager->add_page(metadata_view);

    // Page 4: Hardware Sensors & Radios
    HardwareInfo dummy_hw;
    hardware_view = std::make_shared<HardwareView>(dummy_hw, refresh_fn);
    pager->add_page(hardware_view);

    // Page 5: Sandbox Confinement
    LsmInfo dummy_lsm;
    sandbox_view = std::make_shared<SandboxView>(dummy_lsm);
    pager->add_page(sandbox_view);

    // Page 6: Disposable Testbed
    DistroboxInfo dummy_tb;
    testbed_view = std::make_shared<TestbedView>(dummy_tb, refresh_fn);
    pager->add_page(testbed_view);

    root_container->add_view(pager);

    // =========================================================================
    // 3. MASTER BOTTOM NAVIGATION BAR (7 SECTIONS)
    // =========================================================================
    bottom_nav = BottomNavigationViewBuilder::create()
        ->addItem("Firewall", "🛡️")
        ->addItem("Traffic", "🌐")
        ->addItem("DNS", "🔒")
        ->addItem("Metadata", "🧹")
        ->addItem("Hardware", "💻")
        ->addItem("Sandbox", "📦")
        ->addItem("Testbed", "🧪")
        ->selectedIndex(0)
        ->pillSize(44, 26)
        ->showDivider(true)
        ->barHeight(58)
        ->onItemSelected([&](int idx) {
            if (pager) pager->set_current_page(idx);
        })
        ->build();
    bottom_nav->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        58
    ));
    bottom_nav->set_margin(0, 0, 0, 0);
    root_container->add_view(bottom_nav);


    // =========================================================================
    // 4. WAYLAND WINDOW
    // =========================================================================
    window = WindowBuilder::create()
        ->title("Security & Privacy - miquland")
        ->appId("miqusecure")
        ->role(WindowRole::Toplevel)
        ->preferredSize(600, 680)
        ->closeOnEscape(false)
        ->contentView(root_container)
        ->onClose([engine]() {
            engine->quit();
        })
        ->onKey([&](const KeyPressEvent& ev) {
            if (!ev.pressed) return;
            switch (ev.keysym) {
                case XKB_KEY_1:
                    if (pager) pager->set_current_page(0);
                    break;
                case XKB_KEY_2:
                    if (pager) pager->set_current_page(1);
                    break;
                case XKB_KEY_3:
                    if (pager) pager->set_current_page(2);
                    break;
                case XKB_KEY_4:
                    if (pager) pager->set_current_page(3);
                    break;
                case XKB_KEY_5:
                    if (pager) pager->set_current_page(4);
                    break;
                case XKB_KEY_6:
                    if (pager) pager->set_current_page(5);
                    break;
                case XKB_KEY_7:
                    if (pager) pager->set_current_page(6);
                    break;
                case XKB_KEY_r:
                case XKB_KEY_R:
                    refresh_fn();
                    break;
                default:
                    break;
            }
        })
        ->build();

    if (!window) {
        std::cerr << "[miqusecure] Failed to create Wayland window.\n";
        return 1;
    }

    window->show();
    return engine->enter_loop();
}

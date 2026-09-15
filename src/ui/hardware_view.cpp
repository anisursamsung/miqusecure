#include "hardware_view.hpp"
#include <thread>

using namespace miqu;

namespace miqusecure {

HardwareView::HardwareView(const HardwareInfo& info, std::function<void()> on_changed)
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
    // 1. MASTER AIRPLANE MODE / RADIO KILLSWITCH (Clean Borderless Hero)
    // =========================================================================
    auto hero_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    hero_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    hero_row->set_margin(0, 4, 0, 16);

    auto hero_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    hero_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    m_airplane_lbl = TextViewBuilder::create()
        ->text(m_info.airplane_mode ? "✈️ Airplane Mode is ON (Radios Silenced)" : "✈️ Airplane Mode is OFF")
        ->h2()
        ->bold(true)
        ->build();

    std::string air_desc = m_info.airplane_mode ?
        "All wireless transmitters (Wi-Fi, Bluetooth, and cellular) are powered down." :
        "Wireless radios are active and communicating normally.";

    m_airplane_desc = TextViewBuilder::create()
        ->text(air_desc)
        ->caption()
        ->muted()
        ->build();
    m_airplane_desc->set_margin(0, 2, 0, 0);

    hero_col->add_view(m_airplane_lbl);
    hero_col->add_view(m_airplane_desc);
    hero_row->add_view(hero_col);

    m_switch_airplane = SwitchBuilder::create()
        ->checked(m_info.airplane_mode)
        ->build();
    std::weak_ptr<Switch> weak_air = m_switch_airplane;
    m_switch_airplane->set_on_checked_changed_listener([this, weak_air](bool checked) {
        std::thread([this, checked, weak_air]() {
            bool ok = SecurityBackend::set_airplane_mode(checked);
            if (auto engine = AppEngine::instance()) {
                engine->post([this, checked, ok, weak_air]() {
                    if (ok) {
                        if (m_on_changed) m_on_changed();
                    } else {
                        if (auto s = weak_air.lock()) {
                            s->set_checked(!checked);
                        }
                    }
                });
            }
        }).detach();
    });
    hero_row->add_view(m_switch_airplane);
    m_layout->add_view(hero_row);

    // =========================================================================
    // 2. WIRELESS RADIOS & TRANSMITTERS (Grouped Card with Dividers)
    // =========================================================================
    auto rad_title = TextViewBuilder::create()
        ->text("Wireless Transmitters")
        ->h3()
        ->bold(true)
        ->build();
    rad_title->set_margin(0, 0, 0, 8);
    m_layout->add_view(rad_title);

    auto rad_card = std::make_shared<CardView>();
    rad_card->set_padding(14, 6);
    rad_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    rad_card->set_margin(0, 0, 0, 16);

    auto rad_list = std::make_shared<LinearLayout>(Orientation::Vertical);
    rad_list->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    // Wi-Fi Row
    auto wifi_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    wifi_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    wifi_row->set_margin(4, 6, 4, 6);

    auto wifi_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    wifi_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    auto wifi_title = TextViewBuilder::create()->text("📶 Wi-Fi Wireless Radio")->bold(true)->build();
    auto wifi_desc = TextViewBuilder::create()
        ->text("Connects to local wireless networks. Disabling Wi-Fi physically powers down the antenna and drops all incoming over-the-air connections.")
        ->caption()
        ->multiline(true)
        ->maxLines(2)
        ->ellipsize(true)
        ->build();
    wifi_desc->set_margin(0, 2, 0, 2);
    auto wifi_tech = TextViewBuilder::create()->text("⚙ nmcli: radio wifi on/off | rfkill: wlan")->caption()->muted()->ellipsize(true)->build();

    wifi_col->add_view(wifi_title);
    wifi_col->add_view(wifi_desc);
    wifi_col->add_view(wifi_tech);
    wifi_row->add_view(wifi_col);

    m_switch_wifi = SwitchBuilder::create()
        ->checked(m_info.wifi_enabled)
        ->build();
    std::weak_ptr<Switch> weak_wifi = m_switch_wifi;
    m_switch_wifi->set_on_checked_changed_listener([this, weak_wifi](bool checked) {
        std::thread([this, checked, weak_wifi]() {
            bool ok = SecurityBackend::set_wifi_enabled(checked);
            if (auto engine = AppEngine::instance()) {
                engine->post([this, checked, ok, weak_wifi]() {
                    if (ok) {
                        if (m_on_changed) m_on_changed();
                    } else {
                        if (auto s = weak_wifi.lock()) {
                            s->set_checked(!checked);
                        }
                    }
                });
            }
        }).detach();
    });
    wifi_row->add_view(m_switch_wifi);
    rad_list->add_view(wifi_row);

    rad_list->add_view(DividerViewBuilder::create()->margin(0, 4)->build());

    // Bluetooth Row
    auto bt_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    bt_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    bt_row->set_margin(4, 6, 4, 6);

    auto bt_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    bt_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    auto bt_title = TextViewBuilder::create()->text("ᛒ Bluetooth Wireless Radio")->bold(true)->build();
    auto bt_desc = TextViewBuilder::create()
        ->text("Disables Bluetooth discovery and connections. Prevents physical beacon tracking in malls/airports and shuts down BlueBorne peripheral attacks.")
        ->caption()
        ->multiline(true)
        ->maxLines(2)
        ->ellipsize(true)
        ->build();
    bt_desc->set_margin(0, 2, 0, 2);
    auto bt_tech = TextViewBuilder::create()->text("⚙ bluetoothctl: power on/off | rfkill: bluetooth")->caption()->muted()->ellipsize(true)->build();

    bt_col->add_view(bt_title);
    bt_col->add_view(bt_desc);
    bt_col->add_view(bt_tech);
    bt_row->add_view(bt_col);

    m_switch_bt = SwitchBuilder::create()
        ->checked(m_info.bluetooth_enabled)
        ->build();
    std::weak_ptr<Switch> weak_bt = m_switch_bt;
    m_switch_bt->set_on_checked_changed_listener([this, weak_bt](bool checked) {
        std::thread([this, checked, weak_bt]() {
            bool ok = SecurityBackend::set_bluetooth_enabled(checked);
            if (auto engine = AppEngine::instance()) {
                engine->post([this, checked, ok, weak_bt]() {
                    if (ok) {
                        if (m_on_changed) m_on_changed();
                    } else {
                        if (auto s = weak_bt.lock()) {
                            s->set_checked(!checked);
                        }
                    }
                });
            }
        }).detach();
    });
    bt_row->add_view(m_switch_bt);
    rad_list->add_view(bt_row);

    rad_card->add_view(rad_list);
    m_layout->add_view(rad_card);

    // =========================================================================
    // 3. AUDIO & CAMERA HARDWARE PRIVACY (Grouped Card with Dividers)
    // =========================================================================
    auto sen_title = TextViewBuilder::create()
        ->text("Audio & Visual Privacy Sensors")
        ->h3()
        ->bold(true)
        ->build();
    sen_title->set_margin(0, 0, 0, 8);
    m_layout->add_view(sen_title);

    auto sen_card = std::make_shared<CardView>();
    sen_card->set_padding(14, 6);
    sen_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    sen_card->set_margin(0, 0, 0, 16);

    auto sen_list = std::make_shared<LinearLayout>(Orientation::Vertical);
    sen_list->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    // Microphone Mute Row
    auto mic_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    mic_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    mic_row->set_margin(4, 6, 4, 6);

    auto mic_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    mic_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    auto mic_title = TextViewBuilder::create()->text("🎙️ Microphone Hardware Mute")->bold(true)->build();
    auto mic_desc = TextViewBuilder::create()
        ->text("Silences the physical audio input at the PipeWire server layer. Guarantees no web browser or background application can eavesdrop.")
        ->caption()
        ->multiline(true)
        ->maxLines(2)
        ->ellipsize(true)
        ->build();
    mic_desc->set_margin(0, 2, 0, 2);
    auto mic_tech = TextViewBuilder::create()->text("⚙ wpctl: set-mute @DEFAULT_AUDIO_SOURCE@ 1/0 | PipeWire hardware source")->caption()->muted()->ellipsize(true)->build();

    mic_col->add_view(mic_title);
    mic_col->add_view(mic_desc);
    mic_col->add_view(mic_tech);
    mic_row->add_view(mic_col);

    m_switch_mic = SwitchBuilder::create()
        ->checked(m_info.mic_muted)
        ->build();
    std::weak_ptr<Switch> weak_mic = m_switch_mic;
    m_switch_mic->set_on_checked_changed_listener([this, weak_mic](bool checked) {
        std::thread([this, checked, weak_mic]() {
            bool ok = SecurityBackend::set_mic_muted(checked);
            if (auto engine = AppEngine::instance()) {
                engine->post([this, checked, ok, weak_mic]() {
                    if (ok) {
                        if (m_on_changed) m_on_changed();
                    } else {
                        if (auto s = weak_mic.lock()) {
                            s->set_checked(!checked);
                        }
                    }
                });
            }
        }).detach();
    });
    mic_row->add_view(m_switch_mic);
    sen_list->add_view(mic_row);

    sen_list->add_view(DividerViewBuilder::create()->margin(0, 4)->build());

    // Webcam Live Sensor Row
    auto cam_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    cam_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    cam_row->set_margin(4, 6, 4, 6);

    auto cam_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    cam_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    auto cam_title = TextViewBuilder::create()->text("📷 Webcam Sensor Live Guard")->bold(true)->build();
    auto cam_desc = TextViewBuilder::create()
        ->text("Actively monitors camera hardware devices (/dev/video*). Alerts immediately if any application or browser is streaming video.")
        ->caption()
        ->multiline(true)
        ->maxLines(2)
        ->ellipsize(true)
        ->build();
    cam_desc->set_margin(0, 2, 0, 2);

    std::string cam_tech_str = m_info.camera_in_use ?
        ("⚠️ CAMERA ACTIVE: " + m_info.camera_active_proc) :
        (m_info.camera_detected ? ("✔ Idle - Camera present (" + m_info.camera_device_name + ") • No apps streaming") : "○ No webcam hardware detected");

    m_cam_proc_lbl = TextViewBuilder::create()->text("⚙ " + cam_tech_str)->caption()->muted()->ellipsize(true)->build();

    cam_col->add_view(cam_title);
    cam_col->add_view(cam_desc);
    cam_col->add_view(m_cam_proc_lbl);
    cam_row->add_view(cam_col);

    m_cam_status_badge = TextViewBuilder::create()
        ->text(m_info.camera_in_use ? "● IN USE" : (m_info.camera_detected ? "● IDLE" : "DISABLED"))
        ->bold(true)
        ->build();
    m_cam_status_badge->set_margin(8, 0, 8, 0);
    cam_row->add_view(m_cam_status_badge);

    sen_list->add_view(cam_row);
    sen_card->add_view(sen_list);
    m_layout->add_view(sen_card);

    // =========================================================================
    // 4. "WHY HARDWARE SECURITY MATTERS" (Soft Tinted, Borderless Callout)
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
    auto g_title = TextViewBuilder::create()->text("💡 Why Hardware & Wireless Security Matters")->bold(true)->build();
    auto g_desc = TextViewBuilder::create()
        ->text("Even when software is secure, hardware transmitters emit physical radio signals that broadcast device presence:\n\n"
               "• Bluetooth Beacons: Commercial venues and public spaces deploy tracking beacons that record unique Bluetooth MAC addresses, constructing physical movement profiles.\n"
               "• Wi-Fi Over-The-Air: Disabling the Wi-Fi interface physically cuts power to the radio. Remote network packets cannot reach the machine, preventing over-the-air port probing.\n"
               "• Hardware Mic Muting: Application-level permissions can sometimes be bypassed by privileged background processes. Server-level PipeWire source muting cuts the audio stream at the audio subsystem.")
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

void HardwareView::update_info(const HardwareInfo& info) {
    m_info = info;

    if (m_airplane_lbl) {
        m_airplane_lbl->set_text(m_info.airplane_mode ? "✈️ Airplane Mode is ON (Radios Silenced)" : "✈️ Airplane Mode is OFF");
    }

    if (m_airplane_desc) {
        std::string air_desc = m_info.airplane_mode ?
            "All wireless transmitters (Wi-Fi, Bluetooth, and cellular) are powered down." :
            "Wireless radios are active and communicating normally.";
        m_airplane_desc->set_text(air_desc);
    }

    if (m_switch_airplane) {
        m_switch_airplane->set_checked(m_info.airplane_mode);
    }

    if (m_switch_wifi) {
        m_switch_wifi->set_checked(m_info.wifi_enabled);
    }

    if (m_switch_bt) {
        m_switch_bt->set_checked(m_info.bluetooth_enabled);
    }

    if (m_switch_mic) {
        m_switch_mic->set_checked(m_info.mic_muted);
    }

    if (m_cam_proc_lbl) {
        std::string cam_tech_str = m_info.camera_in_use ?
            ("⚠️ CAMERA ACTIVE: " + m_info.camera_active_proc) :
            (m_info.camera_detected ? ("✔ Idle - Camera present (" + m_info.camera_device_name + ") • No apps streaming") : "○ No webcam hardware detected");
        m_cam_proc_lbl->set_text("⚙ " + cam_tech_str);
    }

    if (m_cam_status_badge) {
        m_cam_status_badge->set_text(m_info.camera_in_use ? "● IN USE" : (m_info.camera_detected ? "● IDLE" : "DISABLED"));
    }
}

} // namespace miqusecure

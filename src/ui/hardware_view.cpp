#include "hardware_view.hpp"
#include "ui_components.hpp"
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
    m_layout->set_padding(22, 18);

    // =========================================================================
    // 1. MASTER AIRPLANE MODE HERO (Direct Placement)
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
        "airplane-mode",
        m_info.airplane_mode ? auto_cfg->colors.primary_container : auto_cfg->colors.surface_variant,
        34, 8, 14
    );
    hero_row->add_view(hero_badge);

    auto hero_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    hero_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    m_airplane_lbl = TextViewBuilder::create()
        ->text(m_info.airplane_mode ? "Airplane Mode is Active (Radios Silenced)" : "Airplane Mode is Disabled")
        ->h2()
        ->bold(true)
        ->build();
    m_airplane_lbl->set_margin(0, 0, 0, 4);

    std::string air_desc = m_info.airplane_mode ?
        "All wireless transmitters (Wi-Fi, Bluetooth, and cellular) are powered down." :
        "Wireless radios are active and communicating normally.";

    m_airplane_desc = TextViewBuilder::create()
        ->text(air_desc)
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
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
    // 2. WIRELESS RADIOS & TRANSMITTERS (Direct Placement)
    // =========================================================================
    auto sec_rad_hdr = ui::make_section_header("WIRELESS TRANSMITTERS");
    sec_rad_hdr->set_margin(0, 18, 0, 12);
    m_layout->add_view(sec_rad_hdr);

    auto rad_list = std::make_shared<LinearLayout>(Orientation::Vertical);
    rad_list->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    rad_list->set_margin(0, 4, 0, 20);

    // Wi-Fi Row
    auto wifi_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    wifi_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    wifi_row->set_margin(0, 10, 0, 10);

    auto wifi_badge = ui::make_icon_badge("network-wireless", auto_cfg->colors.surface_variant, 34, 8, 14);
    wifi_row->add_view(wifi_badge);

    auto wifi_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    wifi_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    auto wifi_title = TextViewBuilder::create()->text("Wi-Fi Wireless")->bold(true)->build();
    auto wifi_desc = TextViewBuilder::create()
        ->text("Disconnects antenna and drops all incoming over-the-air connections.")
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    wifi_desc->set_margin(0, 2, 0, 0);

    wifi_col->add_view(wifi_title);
    wifi_col->add_view(wifi_desc);
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

    // Bluetooth Row
    auto bt_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    bt_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    bt_row->set_margin(0, 10, 0, 10);

    auto bt_badge = ui::make_icon_badge("bluetooth", auto_cfg->colors.surface_variant, 34, 8, 14);
    bt_row->add_view(bt_badge);

    auto bt_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    bt_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    auto bt_title = TextViewBuilder::create()->text("Bluetooth Radio")->bold(true)->build();
    auto bt_desc = TextViewBuilder::create()
        ->text("Disables wireless radio and blocks beacon tracking in public spaces.")
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    bt_desc->set_margin(0, 2, 0, 0);

    bt_col->add_view(bt_title);
    bt_col->add_view(bt_desc);
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

    m_layout->add_view(rad_list);

    // =========================================================================
    // 3. AUDIO & CAMERA HARDWARE PRIVACY (Direct Placement)
    // =========================================================================
    auto sec_sen_hdr = ui::make_section_header("AUDIO & VISUAL PRIVACY SENSORS");
    sec_sen_hdr->set_margin(0, 18, 0, 12);
    m_layout->add_view(sec_sen_hdr);

    auto sen_list = std::make_shared<LinearLayout>(Orientation::Vertical);
    sen_list->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    sen_list->set_margin(0, 4, 0, 20);

    // Microphone Mute Row
    auto mic_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    mic_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    mic_row->set_margin(0, 10, 0, 10);

    auto mic_badge = ui::make_icon_badge("audio-input-microphone", auto_cfg->colors.surface_variant, 34, 8, 14);
    mic_row->add_view(mic_badge);

    auto mic_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    mic_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    auto mic_title = TextViewBuilder::create()->text("Microphone Input")->bold(true)->build();
    auto mic_desc = TextViewBuilder::create()
        ->text("Hardware-level audio mute preventing background apps from recording.")
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    mic_desc->set_margin(0, 2, 0, 0);

    mic_col->add_view(mic_title);
    mic_col->add_view(mic_desc);
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

    // Webcam Live Sensor Row
    auto cam_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    cam_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    cam_row->set_margin(0, 10, 0, 10);

    auto cam_badge = ui::make_icon_badge("camera-web", auto_cfg->colors.surface_variant, 34, 8, 14);
    cam_row->add_view(cam_badge);

    auto cam_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    cam_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    auto cam_title = TextViewBuilder::create()->text("Webcam Live Guard")->bold(true)->build();

    std::string cam_str = m_info.camera_in_use ?
        ("⚠️ Video in use by " + m_info.camera_active_proc) :
        (m_info.camera_detected ? "Camera idle. No applications streaming video." : "No camera hardware detected.");

    m_cam_proc_lbl = TextViewBuilder::create()->text(cam_str)->caption()->muted()->multiline(true)->ellipsize(false)->build();
    m_cam_proc_lbl->set_margin(0, 2, 0, 0);

    cam_col->add_view(cam_title);
    cam_col->add_view(m_cam_proc_lbl);
    cam_row->add_view(cam_col);

    std::string cam_status_txt = m_info.camera_in_use ? "● IN USE" : (m_info.camera_detected ? "● IDLE" : "DISABLED");
    auto cam_pill = ui::make_status_pill(cam_status_txt, m_cam_status_badge, m_info.camera_in_use ? auto_cfg->colors.primary_container : auto_cfg->colors.surface_variant);
    cam_row->add_view(cam_pill);

    sen_list->add_view(cam_row);
    m_layout->add_view(sen_list);

    // =========================================================================
    // 4. "WHY HARDWARE SECURITY MATTERS" (Direct Placement Callout)
    // =========================================================================
    auto guide_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    guide_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    guide_col->set_margin(0, 18, 0, 24);

    auto g_title = TextViewBuilder::create()->text("💡 Why Hardware Controls?")->bold(true)->build();
    auto g_desc = TextViewBuilder::create()
        ->text("Hardware kill switches cut power to wireless antennas and audio/video sensors, preventing tracking and eavesdropping even if software is compromised.")
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

void HardwareView::update_info(const HardwareInfo& info) {
    m_info = info;

    if (m_airplane_lbl) {
        m_airplane_lbl->set_text(m_info.airplane_mode ? "Airplane Mode is Active (Radios Silenced)" : "Airplane Mode is Disabled");
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
        std::string cam_str = m_info.camera_in_use ?
            ("⚠️ Video in use by " + m_info.camera_active_proc) :
            (m_info.camera_detected ? "Camera idle. No applications streaming video." : "No camera hardware detected.");
        m_cam_proc_lbl->set_text(cam_str);
    }

    if (m_cam_status_badge) {
        m_cam_status_badge->set_text(m_info.camera_in_use ? "● IN USE" : (m_info.camera_detected ? "● IDLE" : "DISABLED"));
    }
}

} // namespace miqusecure

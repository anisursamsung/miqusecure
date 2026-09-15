#pragma once

#include "backend/security_backend.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <functional>

namespace miqusecure {

class FirewallView : public miqu::ScrollView {
public:
    FirewallView(const FirewallInfo& info, std::function<void()> on_rules_changed);
    ~FirewallView() override = default;

    void update_info(const FirewallInfo& info);

private:
    void rebuild_modes_list();
    void rebuild_rules_list();

    FirewallInfo m_info;
    std::function<void()> m_on_changed;

    std::shared_ptr<miqu::LinearLayout> m_layout;
    std::shared_ptr<miqu::TextView> m_status_lbl;
    std::shared_ptr<miqu::TextView> m_status_desc;
    std::shared_ptr<miqu::Switch> m_switch_master;

    // Network modes container
    std::shared_ptr<miqu::TextView> m_network_lbl;
    std::shared_ptr<miqu::LinearLayout> m_modes_container;
    std::shared_ptr<miqu::Switch> m_switch_mode_home;
    std::shared_ptr<miqu::Switch> m_switch_mode_hotspot;
    std::shared_ptr<miqu::Switch> m_switch_mode_public;
    std::shared_ptr<miqu::Switch> m_switch_mode_lockdown;

    // Service switches
    std::shared_ptr<miqu::Switch> m_switch_ssh;
    std::shared_ptr<miqu::Switch> m_switch_web;
    std::shared_ptr<miqu::Switch> m_switch_syncthing;

    std::shared_ptr<miqu::LinearLayout> m_rules_container;

    // Add custom rule form
    std::shared_ptr<miqu::EditText> m_input_port;
    std::shared_ptr<miqu::Button> m_btn_proto_tcp;
    std::shared_ptr<miqu::Button> m_btn_proto_udp;
    std::string m_selected_proto = "tcp";
    std::string m_selected_action = "ALLOW";

    // Material Design 3 docked linear progress indicator
    std::shared_ptr<miqu::ProgressBar> m_progress_bar;
};

} // namespace miqusecure

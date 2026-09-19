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
    void rebuild_rules_list();

    FirewallInfo m_info;
    std::function<void()> m_on_changed;

    std::shared_ptr<miqu::LinearLayout> m_layout;
    std::shared_ptr<miqu::TextView> m_status_icon;
    std::shared_ptr<miqu::TextView> m_status_lbl;
    std::shared_ptr<miqu::Switch> m_switch_master;

    // Network mode & details
    std::shared_ptr<miqu::Spinner> m_spinner_mode;
    std::shared_ptr<miqu::TextView> m_network_lbl;
    std::shared_ptr<miqu::TextView> m_mode_tech_desc;
    std::shared_ptr<miqu::TextView> m_mode_desc;
    bool m_updating_ui = false;

    void update_mode_descriptions(NetworkMode mode);
    static int mode_to_index(NetworkMode mode);
    static NetworkMode index_to_mode(int index);

    // Service switches
    std::shared_ptr<miqu::Switch> m_switch_ssh;
    std::shared_ptr<miqu::Switch> m_switch_web;
    std::shared_ptr<miqu::Switch> m_switch_syncthing;
    std::shared_ptr<miqu::Switch> m_switch_samba;

    std::shared_ptr<miqu::LinearLayout> m_rules_container;

    // Add custom rule form
    std::shared_ptr<miqu::EditText> m_input_port;
    std::shared_ptr<miqu::Spinner> m_spinner_proto;
    std::string m_selected_proto = "tcp";
    std::string m_selected_action = "ALLOW";

    // Material Design 3 docked linear progress indicator
    std::shared_ptr<miqu::ProgressBar> m_progress_bar;
};

} // namespace miqusecure

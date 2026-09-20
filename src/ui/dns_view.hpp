#pragma once

#include "backend/security_backend.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <functional>

namespace miqusecure {

class DnsView : public miqu::ScrollView {
public:
    DnsView(const DnsInfo& info, std::function<void()> on_changed);
    ~DnsView() override = default;

    void update_info(const DnsInfo& info);

private:
    DnsInfo m_info;
    std::function<void()> m_on_changed;
    bool m_updating_ui = false;

    std::shared_ptr<miqu::LinearLayout> m_layout;
    std::shared_ptr<miqu::TextView> m_status_lbl;
    std::shared_ptr<miqu::TextView> m_status_desc;
    std::shared_ptr<miqu::Switch> m_switch_master;
    std::shared_ptr<miqu::ProgressBar> m_progress_bar;

    // Custom DNS inputs
    std::shared_ptr<miqu::EditText> m_input_primary;
    std::shared_ptr<miqu::EditText> m_input_secondary;

    // 2x2 grid switches
    std::shared_ptr<miqu::Switch> m_switch_quad9;
    std::shared_ptr<miqu::Switch> m_switch_adguard;
    std::shared_ptr<miqu::Switch> m_switch_mullvad;
    std::shared_ptr<miqu::Switch> m_switch_cloudflare;
};

} // namespace miqusecure

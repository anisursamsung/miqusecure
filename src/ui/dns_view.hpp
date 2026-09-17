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
    void rebuild_profiles_list();
    void update_profile_descriptions(int index);
    static int get_profile_index(const DnsInfo& info);

    DnsInfo m_info;
    std::function<void()> m_on_changed;
    bool m_updating_ui = false;

    std::shared_ptr<miqu::LinearLayout> m_layout;
    std::shared_ptr<miqu::TextView> m_mode_lbl;
    std::shared_ptr<miqu::TextView> m_status_desc;
    std::shared_ptr<miqu::TextView> m_conn_lbl;
    std::shared_ptr<miqu::LinearLayout> m_profiles_container;
    std::shared_ptr<miqu::Spinner> m_spinner_dns;
    std::shared_ptr<miqu::TextView> m_profile_desc;
    std::shared_ptr<miqu::TextView> m_profile_tech_desc;
    std::shared_ptr<miqu::ProgressBar> m_progress_bar;
};

} // namespace miqusecure

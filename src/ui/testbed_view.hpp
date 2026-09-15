#pragma once

#include "backend/security_backend.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <memory>
#include <functional>

namespace miqusecure {

class TestbedView : public miqu::ScrollView {
public:
    explicit TestbedView(const DistroboxInfo& info, std::function<void()> on_refresh);
    ~TestbedView() override = default;

    void update_info(const DistroboxInfo& info);

private:
    void rebuild_boxes_list();

    DistroboxInfo m_info;
    std::function<void()> m_on_refresh;

    std::shared_ptr<miqu::LinearLayout> m_layout;
    std::shared_ptr<miqu::LinearLayout> m_boxes_container;
    std::shared_ptr<miqu::TextView> m_status_lbl;
    std::shared_ptr<miqu::ProgressBar> m_progress_bar;
    std::shared_ptr<miqu::Button> m_btn_create;
};

} // namespace miqusecure

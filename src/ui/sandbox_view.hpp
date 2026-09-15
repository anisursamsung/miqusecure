#pragma once

#include "backend/security_backend.hpp"
#include <miqutoolkit/miqutoolkit.hpp>

namespace miqusecure {

class SandboxView : public miqu::ScrollView {
public:
    explicit SandboxView(const LsmInfo& info);
    ~SandboxView() override = default;

    void update_info(const LsmInfo& info);

private:
    void rebuild_flatpak_section();

    LsmInfo m_info;
    std::shared_ptr<miqu::LinearLayout> m_layout;
    std::shared_ptr<miqu::TextView> m_stack_lbl;
    std::shared_ptr<miqu::LinearLayout> m_flatpak_container;
};

} // namespace miqusecure

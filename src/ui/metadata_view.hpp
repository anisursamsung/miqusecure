#pragma once

#include "backend/security_backend.hpp"
#include <miqutoolkit/miqutoolkit.hpp>

namespace miqusecure {

class MetadataView : public miqu::ScrollView {
public:
    explicit MetadataView(const MetadataInfo& info);
    ~MetadataView() override = default;

    void update_info(const MetadataInfo& info);

private:
    void rebuild_recent_files();
    void clean_file(const std::string& path);

    MetadataInfo m_info;
    std::shared_ptr<miqu::LinearLayout> m_layout;
    std::shared_ptr<miqu::TextView> m_status_lbl;
    std::shared_ptr<miqu::LinearLayout> m_recent_container;

    std::shared_ptr<miqu::EditText> m_path_input;
    std::shared_ptr<miqu::Button> m_clean_btn;
    std::shared_ptr<miqu::ProgressBar> m_progress_bar;
    std::shared_ptr<miqu::TextView> m_result_lbl;
};

} // namespace miqusecure

#include "metadata_view.hpp"
#include <filesystem>
#include <thread>

namespace fs = std::filesystem;
using namespace miqu;

namespace miqusecure {

MetadataView::MetadataView(const MetadataInfo& info) : m_info(info) {
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
    // 1. STATUS HEADER (Clean Borderless Hero)
    // =========================================================================
    auto status_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    status_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    status_col->set_margin(0, 4, 0, 16);

    auto title = TextViewBuilder::create()->text("🧼 File Metadata Anonymizer (mat2)")->h2()->bold(true)->build();
    title->set_margin(0, 0, 0, 2);

    std::string st_str = m_info.mat2_installed ?
        ("✔ Engine Active (" + (m_info.version.empty() ? "mat2 ready" : m_info.version) + ")") :
        "⚠️ mat2 Engine Not Installed";

    m_status_lbl = TextViewBuilder::create()
        ->text(st_str)
        ->caption()
        ->muted()
        ->build();

    status_col->add_view(title);
    status_col->add_view(m_status_lbl);

    if (!m_info.mat2_installed) {
        auto inst_guide = TextViewBuilder::create()
            ->text("To enable 1-click metadata cleaning on this system, install mat2:\n  sudo pacman -S mat2")
            ->caption()
            ->build();
        inst_guide->set_margin(0, 6, 0, 0);
        status_col->add_view(inst_guide);
    }

    m_layout->add_view(status_col);

    // =========================================================================
    // 2. QUICK-PICK RECENT FILES (ONE Grouped Card with Dividers)
    // =========================================================================
    auto quick_hdr = TextViewBuilder::create()
        ->text("Quick Clean: Recent Downloads & Photos")
        ->h3()
        ->bold(true)
        ->build();
    quick_hdr->set_margin(0, 0, 0, 8);
    m_layout->add_view(quick_hdr);

    m_recent_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_recent_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    rebuild_recent_files();
    m_layout->add_view(m_recent_container);

    // =========================================================================
    // 3. MANUAL PATH INPUT (Clean, Flat Inset)
    // =========================================================================
    auto action_title = TextViewBuilder::create()->text("Or Clean File by Path")->bold(true)->build();
    action_title->set_margin(0, 4, 0, 6);
    m_layout->add_view(action_title);

    auto input_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    input_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    input_row->set_margin(0, 0, 0, 8);

    m_path_input = EditTextBuilder::create()
        ->hint("Path to file (e.g. /home/user/document.pdf)")
        ->padding(10, 6)
        ->build();
    m_path_input->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));
    m_path_input->set_margin(0, 0, 8, 0);
    input_row->add_view(m_path_input);

    m_clean_btn = ButtonBuilder::create()
        ->text("Clean File")
        ->primary(true)
        ->padding(14, 6)
        ->onClick([this]() {
            if (m_path_input) {
                clean_file(m_path_input->get_text());
            }
        })
        ->build();
    input_row->add_view(m_clean_btn);
    m_layout->add_view(input_row);

    m_progress_bar = ProgressBarBuilder::create()
        ->style(ProgressBarStyle::Linear)
        ->indeterminate(true)
        ->trackHeight(3)
        ->build();
    m_progress_bar->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        3
    ));
    m_progress_bar->set_visibility(Visibility::Invisible);
    m_progress_bar->set_margin(0, 0, 0, 6);
    m_layout->add_view(m_progress_bar);

    m_result_lbl = TextViewBuilder::create()
        ->text("")
        ->caption()
        ->build();
    m_result_lbl->set_margin(0, 0, 0, 16);
    m_layout->add_view(m_result_lbl);

    // =========================================================================
    // 4. "WHAT IS METADATA?" (Soft Tinted, Borderless Callout)
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
    auto g_title = TextViewBuilder::create()->text("💡 What is Hidden Metadata?")->bold(true)->build();
    auto g_desc = TextViewBuilder::create()
        ->text("When photos are taken with phones or documents are exported, hidden metadata is automatically attached:\n• Exact GPS coordinates (revealing private residential or workplace locations)\n• Hardware camera models and device serial numbers\n• Operating system usernames, software versions, and editing timestamps\n\nCleaning a file creates a safe '.cleaned' copy with all hidden tracking tags stripped, ready for public sharing.")
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

void MetadataView::rebuild_recent_files() {
    if (!m_recent_container) return;
    m_recent_container->clear_views();

    if (m_info.recent_files.empty()) {
        auto empty_tv = TextViewBuilder::create()
            ->text("No recent photos or documents found in ~/Downloads or ~/Pictures.")
            ->caption()
            ->muted()
            ->build();
        empty_tv->set_margin(0, 4, 0, 8);
        m_recent_container->add_view(empty_tv);
        return;
    }

    auto files_card = std::make_shared<CardView>();
    files_card->set_padding(14, 6);
    files_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    files_card->set_margin(0, 0, 0, 16);

    auto list_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    list_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    for (size_t i = 0; i < m_info.recent_files.size(); ++i) {
        const auto& file = m_info.recent_files[i];
        auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        row->set_margin(4, 6, 4, 6);

        std::string icon = (file.type == "Image") ? "🖼️" :
                           (file.type == "PDF Document" ? "📄" :
                           (file.type == "Office Document" ? "📝" : "🎵"));

        auto icon_tv = TextViewBuilder::create()->text(icon)->h2()->build();
        icon_tv->set_margin(0, 0, 12, 0);
        row->add_view(icon_tv);

        auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
        col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

        auto n_tv = TextViewBuilder::create()->text(file.filename)->bold(true)->ellipsize(true)->build();
        auto d_tv = TextViewBuilder::create()->text(file.type + " • " + file.size_str)->caption()->muted()->build();
        col->add_view(n_tv);
        col->add_view(d_tv);
        row->add_view(col);

        std::string path = file.full_path;
        auto btn = ButtonBuilder::create()
            ->text("Clean")
            ->flat(true)
            ->bold(true)
            ->padding(10, 4)
            ->onClick([this, path]() {
                clean_file(path);
            })
            ->build();
        row->add_view(btn);

        list_col->add_view(row);
        if (i + 1 < m_info.recent_files.size()) {
            list_col->add_view(DividerViewBuilder::create()->margin(0, 4)->build());
        }
    }

    files_card->add_view(list_col);
    m_recent_container->add_view(files_card);
}

void MetadataView::clean_file(const std::string& path) {
    if (!m_info.mat2_installed) {
        if (m_result_lbl) m_result_lbl->set_text("Error: mat2 is not installed on this system. Run: sudo pacman -S mat2");
        return;
    }

    if (path.empty()) {
        if (m_result_lbl) m_result_lbl->set_text("Please enter or select a valid file path.");
        return;
    }

    if (!fs::exists(path)) {
        if (m_result_lbl) m_result_lbl->set_text("Error: File does not exist: " + path);
        return;
    }

    if (m_path_input) {
        m_path_input->set_text(path);
    }
    if (m_clean_btn) {
        m_clean_btn->set_text("Scrubbing...");
    }
    if (m_progress_bar) {
        m_progress_bar->set_visibility(Visibility::Visible);
    }
    if (m_result_lbl) {
        m_result_lbl->set_text("Scrubbing metadata with mat2...");
    }

    std::thread([this, path]() {
        std::string log;
        bool ok = SecurityBackend::clean_metadata(path, log);
        if (auto engine = AppEngine::instance()) {
            engine->post([this, ok, log]() {
                if (m_clean_btn) {
                    m_clean_btn->set_text("Clean File");
                }
                if (m_progress_bar) {
                    m_progress_bar->set_visibility(Visibility::Invisible);
                }
                if (m_result_lbl) {
                    if (ok) {
                        m_result_lbl->set_text("✔ " + (log.empty() ? "Successfully scrubbed metadata! Created '.cleaned' copy in same folder." : log));
                    } else {
                        m_result_lbl->set_text("Result: " + log);
                    }
                }
            });
        }
    }).detach();
}

void MetadataView::update_info(const MetadataInfo& info) {
    m_info = info;

    std::string st_str = m_info.mat2_installed ?
        ("✔ Engine Active (" + (m_info.version.empty() ? "mat2 ready" : m_info.version) + ")") :
        "⚠️ mat2 Engine Not Installed";

    if (m_status_lbl) m_status_lbl->set_text(st_str);

    rebuild_recent_files();
}

} // namespace miqusecure

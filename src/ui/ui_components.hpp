#pragma once

#include <miqutoolkit/miqutoolkit.hpp>
#include <string>
#include <memory>

namespace miqusecure {

namespace ui {

// Creates a refined iOS/Win11 squircle icon badge plate using official toolkit colors
inline std::shared_ptr<miqu::FrameLayout> make_icon_badge(
    const std::string& icon_text,
    const miqu::Color& bg_color = miqu::Color::transparent(),
    int size = 32,
    int radius = 8,
    int margin_right = 12)
{
    auto cfg = miqu::Config::get();
    auto frame = std::make_shared<miqu::FrameLayout>();
    frame->set_background_color(bg_color.a > 0.01f ? bg_color : cfg->colors.surface_variant);
    frame->set_corner_radius(radius);
    frame->set_layout_params(miqu::LayoutParams(
        size,
        size,
        miqu::Gravity::CenterVertical
    ));
    frame->set_margin(0, 0, margin_right, 0);

    auto tv = miqu::TextViewBuilder::create()
        ->text(icon_text)
        ->h3()
        ->build();
    tv->set_layout_params(miqu::LayoutParams(
        static_cast<int>(miqu::LayoutDimension::WrapContent),
        static_cast<int>(miqu::LayoutDimension::WrapContent),
        miqu::Gravity::Center
    ));

    frame->add_view(tv);
    return frame;
}

// Creates an iOS-style inset divider that starts exactly where text begins
inline std::shared_ptr<miqu::View> make_inset_divider(int inset_left = 50, int v_margin = 4) {
    return miqu::DividerViewBuilder::create()
        ->margin(inset_left, v_margin, 8, v_margin)
        ->build();
}

// Creates a subtle, crisp group section header above cards
inline std::shared_ptr<miqu::TextView> make_section_header(const std::string& title) {
    auto tv = miqu::TextViewBuilder::create()
        ->text(title)
        ->caption()
        ->bold(true)
        ->muted()
        ->build();
    tv->set_margin(4, 12, 4, 6);
    return tv;
}

// Creates a status pill / metric chip (e.g. "Active", "142 MB")
inline std::shared_ptr<miqu::FrameLayout> make_status_pill(
    const std::string& text,
    std::shared_ptr<miqu::TextView>& out_label,
    const miqu::Color& bg_color = miqu::Color::transparent(),
    bool is_bold = true)
{
    auto frame = std::make_shared<miqu::FrameLayout>();
    auto cfg = miqu::Config::get();
    frame->set_background_color(bg_color.a > 0.01f ? bg_color : cfg->colors.surface_variant);
    frame->set_corner_radius(6);
    frame->set_padding(10, 4);
    frame->set_layout_params(miqu::LayoutParams(
        static_cast<int>(miqu::LayoutDimension::WrapContent),
        static_cast<int>(miqu::LayoutDimension::WrapContent),
        miqu::Gravity::CenterVertical
    ));

    out_label = miqu::TextViewBuilder::create()
        ->text(text)
        ->caption()
        ->bold(is_bold)
        ->build();
    out_label->set_layout_params(miqu::LayoutParams(
        static_cast<int>(miqu::LayoutDimension::WrapContent),
        static_cast<int>(miqu::LayoutDimension::WrapContent),
        miqu::Gravity::Center
    ));

    frame->add_view(out_label);
    return frame;
}

} // namespace ui
} // namespace miqusecure

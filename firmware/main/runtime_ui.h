#pragma once
#include <array>
#include <string>
#include <vector>
#include "offline.h"
#include "generated/dashboard_fonts.h"
#include "generated/runtime_assets.h"

class RuntimeUi {
public:
    explicit RuntimeUi(const uint8_t* glyphs,const uint8_t* dashboard=nullptr):glyphs_(glyphs),dashboard_(dashboard) {}
    std::array<uint8_t,15000> frame{};
    // True means the whole answer fits. Throws no exceptions; displays overflow warning.
    bool Draw(const wm::Offline& model,int64_t now,const std::string& notice="",unsigned answer_page=0);
    unsigned missing_glyphs=0,answer_pages=1;
private:
    const uint8_t* glyphs_;
    const uint8_t* dashboard_;
    const dashboard::Glyph* DashboardGlyph(uint32_t code,int size) const;
    int DashboardWidth(const std::string& value,int size) const;
    void DashboardText(int x,int y,const std::string& value,int size);
    void DashboardCenter(int y,const std::string& value,int size);
    std::vector<std::string> DashboardWrap(const std::string& value,int size,int width) const;
    void Capsule(int x,int y,int width,int height);
    bool Dashboard(const wm::Offline& model,int64_t raw);
    const assets::Glyph* Glyph(uint32_t code,int size) const;
    void Pixel(int x,int y,bool black=true);
    void Box(int x,int y,int w,int h,bool black=true);
    void Text(int x,int y,const std::string& value,int size=16,bool invert=false);
    int Width(const std::string& value,int size) const;
    std::vector<std::string> Wrap(const std::string& value,int size,int width) const;
    void Center(int y,const std::string& value,int size=16);
    void Footer(const std::string& a,const std::string& b,const std::string& c);
};

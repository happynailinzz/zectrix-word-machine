#include "runtime_ui.h"
#include "model_catalog.h"
#include <cassert>
#include <ctime>
#include <fstream>
#include <iterator>
#include <iostream>
void Write(const std::string& name,const RuntimeUi& ui){
    std::ofstream out("output/home-design/"+name+".pgm",std::ios::binary);out<<"P5\n400 300\n255\n";
    for(int y=0;y<300;++y)for(int x=0;x<400;++x){char c=ui.frame[y*50+x/8]&(1U<<(7-x%8))?char(255):0;out.write(&c,1);}
}
int main(){
    std::ifstream f("firmware/main/generated/glyphs.bin",std::ios::binary),d("firmware/main/generated/dashboard_fonts.bin",std::ios::binary);
    std::vector<uint8_t> glyphs((std::istreambuf_iterator<char>(f)),{}),dash((std::istreambuf_iterator<char>(d)),{});
    assert(!glyphs.empty()&&!dash.empty());RuntimeUi ui(glyphs.data(),dash.data());auto m=DeviceModel();
    // 2026-10-17 21:01 Asia/Shanghai (reference image time).
    const int64_t now=1792242060;
    m.session.book=2;m.ReturnDashboard();
    for(unsigned i=0;i<3;++i){auto idx=m.books[2].members[i].index;m.rows[idx].progress.reviews=1;m.rows[idx].progress.due_at=i?now+86400:now-10;}
    wm::Daily day;day.day=wm::Day(now);day.new_words=7;day.ratings=9;day.remembered=9;m.history.push_back(day);
    assert(ui.Draw(m,now));Write("reference",ui);
    m.rows[m.books[2].members[3].index].progress.reviews=1;
    assert(ui.Draw(m,now));Write("progress-updated",ui);
    for(auto title:{"人教版 · 八年级上册","PET / B1 Preliminary 索引","人教8下（缺页）"}){
        m.books[2].title=title;
        for(unsigned goal:{5,10,15,20,50}){
            m.session.new_limit=goal;
            for(unsigned count:{0,1,7,20,100}){m.history[0].new_words=count;if(!ui.Draw(m,now)){std::cerr<<title<<" missing="<<ui.missing_glyphs<<"\n";return 1;}}
        }
    }
    m.books[2].title="人教8年级上册";m.history[0].new_words=0;
    assert(ui.Draw(m,-1));Write("invalid-clock",ui);
    for(auto member:m.SelectedBook().members)m.rows[member.index].progress.reviews=1;
    assert(ui.Draw(m,now));Write("complete",ui);
    std::cout<<"home design: 400x300, title variants, quotas, missing clock, progress transitions PASS\n";
}

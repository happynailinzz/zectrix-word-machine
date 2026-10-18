#include "runtime_ui.h"
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <cmath>

namespace {
std::vector<std::pair<uint32_t,std::string>> Chars(const std::string& text){
    std::vector<std::pair<uint32_t,std::string>> out;
    for(size_t i=0;i<text.size();){
        const unsigned char c=text[i];size_t n=c<128?1:(c&0xe0)==0xc0?2:(c&0xf0)==0xe0?3:4;
        if(i+n>text.size())break;
        uint32_t code=c& (n==1?0x7f:n==2?0x1f:n==3?0x0f:0x07);
        for(size_t j=1;j<n;++j)code=(code<<6)|(text[i+j]&0x3f);
        out.emplace_back(code,text.substr(i,n));i+=n;
    }return out;
}
std::string Num(unsigned n){return std::to_string(n);}
std::string AnswerText(const assets::Word& word){
    std::string pos=word.pos,gloss=word.gloss;
    while(!pos.empty()&&(pos.back()=='.'||pos.back()==' '))pos.pop_back();
    // Enriched dictionaries may already carry the same POS in each sense.
    if(!pos.empty()&&(gloss.rfind(pos+".",0)==0||gloss.rfind(pos+" ",0)==0))return gloss;
    return std::string(word.pos)+(word.pos[0]?"  ":"")+gloss;
}

}
const assets::Glyph* RuntimeUi::Glyph(uint32_t code,int size) const {
    int group=size==16?0:size==20?1:size==28?2:3;
    const auto* first=assets::kGlyphs+group*assets::kGlyphsPerSize;
    auto* found=std::lower_bound(first,first+assets::kGlyphsPerSize,code,[](const assets::Glyph& g,uint32_t c){return g.code<c;});
    return found!=first+assets::kGlyphsPerSize&&found->code==code&&found->height?found:nullptr;
}
void RuntimeUi::Pixel(int x,int y,bool black){
    if(x<0||x>=400||y<0||y>=300)return;
    auto& b=frame[y*50+x/8];uint8_t mask=1U<<(7-x%8);
    if(black)b&=~mask;else b|=mask;
}
void RuntimeUi::Box(int x,int y,int w,int h,bool black){
    for(int yy=std::max(y,0);yy<std::min(y+h,300);++yy)
        for(int xx=std::max(x,0);xx<std::min(x+w,400);++xx)Pixel(xx,yy,black);
}
void RuntimeUi::Text(int x,int y,const std::string& s,int size,bool invert){
    for(const auto& c:Chars(s)){
        const auto* g=Glyph(c.first,size);
        if(!g){++missing_glyphs;g=Glyph('?',size);}
        if(!g)continue;
        if(invert)Box(x,y,g->advance,g->height);
        int stride=(g->width+7)/8;
        for(int yy=0;yy<g->height;++yy)for(int xx=0;xx<g->width;++xx)
            if(glyphs_[g->offset+yy*stride+xx/8]&(1U<<(7-xx%8)))Pixel(x+xx,y+yy,!invert);
        x+=g->advance;
    }
}
int RuntimeUi::Width(const std::string& s,int size) const {
    int width=0;for(const auto& c:Chars(s)){const auto* g=Glyph(c.first,size);width+=g?g->advance:size;}return width;
}
std::vector<std::string> RuntimeUi::Wrap(const std::string& s,int size,int width) const {
    std::vector<std::string> lines;std::string line;
    for(const auto& c:Chars(s)){
        if(Width(line+c.second,size)>width&&!line.empty()){
            // Prefer an English word break where it does not waste most of the row.
            const auto space=line.find_last_of(' ');
            if(c.first<128&&space!=std::string::npos&&space>line.size()/3){
                lines.push_back(line.substr(0,space));line=line.substr(space+1);
            }else{lines.push_back(line);line.clear();}
        }
        if(line.empty()&&c.first==' ')continue;
        line+=c.second;
    }
    if(!line.empty()){lines.push_back(line);}
    return lines;
}
void RuntimeUi::Center(int y,const std::string& s,int size){Text(std::max(10,(400-Width(s,size))/2),y,s,size);}
void RuntimeUi::Footer(const std::string& a,const std::string& b,const std::string& c){
    Box(12,258,376,1);Text(14,270,a);Text(146,270,b);Text(277,270,c);
}
const dashboard::Glyph* RuntimeUi::DashboardGlyph(uint32_t code,int size) const {
    const auto key=std::make_pair(size,code);
    const auto* first=dashboard::kGlyphs;
    auto* found=std::lower_bound(first,first+dashboard::kCount,key,[](const dashboard::Glyph& g,const auto& k){
        return std::make_pair(int(g.size),g.code)<k;});
    return found!=first+dashboard::kCount&&found->size==size&&found->code==code&&found->height?found:nullptr;
}
int RuntimeUi::DashboardWidth(const std::string& value,int size) const {
    int width=0;for(const auto& c:Chars(value)){const auto* g=DashboardGlyph(c.first,size);width+=g?g->advance:size;}
    return width;
}
void RuntimeUi::DashboardText(int x,int y,const std::string& value,int size){
    for(const auto& c:Chars(value)){
        const auto* g=DashboardGlyph(c.first,size);
        if(!g||!dashboard_){++missing_glyphs;continue;}
        const int stride=(g->width+7)/8;
        for(int yy=0;yy<g->height;++yy)for(int xx=0;xx<g->width;++xx)
            if(dashboard_[g->offset+yy*stride+xx/8]&(1U<<(7-xx%8)))Pixel(x+xx,y+yy);
        x+=g->advance;
    }
}
void RuntimeUi::DashboardCenter(int y,const std::string& value,int size){
    DashboardText((400-DashboardWidth(value,size))/2,y,value,size);
}
std::vector<std::string> RuntimeUi::DashboardWrap(const std::string& value,int size,int width) const {
    std::vector<std::string> result;std::string line;
    for(const auto& c:Chars(value)){
        if(DashboardWidth(line+c.second,size)>width&&!line.empty()){result.push_back(line);line.clear();}
        line+=c.second;
    }
    if(!line.empty())result.push_back(line);
    return result;
}
void RuntimeUi::Capsule(int x,int y,int width,int height){
    const double radius=(height-1)/2.0;
    for(int yy=0;yy<height;++yy)for(int xx=0;xx<width;++xx){
        const double cx=std::max(radius,std::min(double(xx),width-1-radius));
        const double dx=xx-cx,dy=yy-radius;
        const double distance=std::sqrt(dx*dx+dy*dy);
        if(distance<=radius&&distance>=radius-1.1)Pixel(x+xx,y+yy);
    }
}
bool RuntimeUi::Dashboard(const wm::Offline& m,int64_t raw){
    frame.fill(255);
    const auto stats=m.Today(raw);const auto progress=m.CurrentProgress(raw);
    std::string clock="--:--",date="日期待校准";
    if(raw>=0){
        const time_t value=raw+wm::kZoneOffset;tm parts{};gmtime_r(&value,&parts);
        char hour[8],day[32];std::strftime(hour,sizeof(hour),"%H:%M",&parts);clock=hour;
        std::snprintf(day,sizeof(day),"%d月%d日 ",parts.tm_mon+1,parts.tm_mday);
        const char* week[]={"周日","周一","周二","周三","周四","周五","周六"};date=std::string(day)+week[parts.tm_wday];
    }
    DashboardCenter(-7,clock,88);
    DashboardCenter(84,date,20);
    std::string book=m.SelectedBook().title;
    // Typography only: expand numeric textbook titles without changing registry IDs.
    const std::pair<const char*,const char*> names[]={{"人教7年级上册","人教版 · 七年级上册"},
        {"人教7年级下册","人教版 · 七年级下册"},{"人教8年级上册","人教版 · 八年级上册"},
        {"人教9年级上册","人教版 · 九年级上册"},{"人教9年级下册","人教版 · 九年级下册"}};
    for(const auto& name:names)if(book==name.first){book=name.second;break;}
    int title_size=DashboardWidth(book,20)<=350?20:16;
    const int capsule_width=std::min(380,DashboardWidth(book,title_size)+28);
    Capsule((400-capsule_width)/2,116,capsule_width,29);
    DashboardCenter(118,book,title_size);
    const unsigned completed=std::min(stats.new_words,m.session.new_limit);
    const std::string today="今日新词 "+Num(stats.new_words)+"/"+Num(m.session.new_limit);
    DashboardText(12,157,today,16);
    const std::string due="到期 "+Num(progress.due);
    DashboardText(388-DashboardWidth(due,16),157,due,16);
    // Ten equal segments represent the fraction of any chosen daily target.
    const int left=DashboardWidth(today,16)+24;
    const int right=382-DashboardWidth(due,16)-9;
    const int segments=10,gap=3;
    const int side=std::max(3,std::min(10,(right-left-(segments-1)*gap)/segments));
    const unsigned filled=m.session.new_limit?completed*10/m.session.new_limit:0;
    for(int i=0;i<segments;++i){
        int x=left+i*(side+gap),y=163;
        if(x+side>right)break;
        if(progress.total&&unsigned(i)<filled)Box(x,y,side,side);
        else{Box(x,y,side,1);Box(x,y+side-1,side,1);Box(x,y,1,side);Box(x+side-1,y,1,side);}
    }
    if(progress.total){
        const std::string summary="全书 "+Num(progress.studied)+"/"+Num(progress.total);
        DashboardText(12,190,summary,14);
        const unsigned percent=progress.studied*100/progress.total;
        const std::string percent_text=Num(percent)+"%";
        const int bar_x=std::max(112,DashboardWidth(summary,14)+22);
        const int bar_end=380-DashboardWidth(percent_text,14)-9;
        Box(bar_x,197,bar_end-bar_x,8);Box(bar_x+1,198,bar_end-bar_x-2,6,false);
        const int fill=(bar_end-bar_x-2)*progress.studied/progress.total;
        if(fill>0)Box(bar_x+1,198,fill,6);
        DashboardText(388-DashboardWidth(percent_text,14),190,percent_text,14);
    }else{
        DashboardCenter(189,"英文索引 · 中文待补",16);
    }
    const auto quote=DashboardWrap(assets::kQuotes[wm::Offline::QuoteIndex(raw)],16,372);
    const int quote_y=quote.size()>1?219:229;
    for(size_t i=0;i<quote.size()&&i<2;++i)DashboardCenter(quote_y+i*20,quote[i],16);
    // Draw arrows as geometry, rather than relying on font coverage at 1bpp.
    for(int i=0;i<7;++i){Box(9+i,279-i,1,1+i*2);Box(390-i,279-i,1,1+i*2);}
    DashboardText(22,266,"继续",20);
    DashboardCenter(266,"OK 菜单",20);
    DashboardText(377-DashboardWidth("选词书",20),266,"选词书",20);
    return quote.size()<=2&&DashboardWidth(book,title_size)<=352;
}
bool RuntimeUi::Draw(const wm::Offline& m,int64_t raw,const std::string& notice,unsigned answer_page){
    answer_pages=1;
    frame.fill(255);missing_glyphs=0;bool fits=true;
    if(m.session.page==wm::Page::Dashboard){
        fits=Dashboard(m,raw);
        if(!notice.empty()){Box(12,216,376,39,false);DashboardCenter(225,notice,14);}
        return fits&&missing_glyphs==0;
    }
    Text(12,7,m.SelectedBook().title,20);Text(275,11,"词库草稿",16);Box(12,37,376,1);
    const auto& s=m.session;auto page=s.page;
    const std::string scope=s.scope==wm::Scope::Wrong?"错词回练":s.scope==wm::Scope::Favorites?"我的收藏":m.SelectedBook().units[s.unit];
    if(page==wm::Page::Books){
        const unsigned start=s.cursor/5*5;
        for(unsigned i=start;i<std::min(start+5,unsigned(m.books.size()));++i){
            const int y=49+(i-start)*36;if(i==s.cursor)Box(12,y,376,33);
            Text(20,y+2,m.books[i].title,20,i==s.cursor);
            if(m.books[i].members.empty())Text(316,y+5,"待导入",16,i==s.cursor);
        }
        Footer("上键 上一项","OK 选择","下键 下一项");
    }else if(page==wm::Page::Unavailable){
        Center(72,m.books[s.cursor].title,20);Center(125,"词库待导入",28);
        Center(177,"尚未安装词条，不能开始学习",20);
        Footer("","OK 返回","");
    }else if(page==wm::Page::Home){
        std::vector<std::string> labels={"继续学习","选择单元","错词回练","我的收藏","今日统计","每日新词 "+Num(s.new_limit),"当前词书学习","选择词书","返回首页"};
        const unsigned start=s.cursor/5*5;
        for(unsigned i=start;i<std::min(start+5,unsigned(labels.size()));++i){
            const int y=49+(i-start)*36;if(i==s.cursor)Box(12,y,376,33);
            Text(24,y+2,labels[i],20,i==s.cursor);
        }
        Footer("上键 上一项","OK 确认","下键 下一项");
    }else if(page==wm::Page::Units){
        const unsigned start=s.cursor/5*5;
        for(unsigned i=start;i<std::min(start+5,unsigned(m.SelectedBook().units.size()));++i){
            unsigned count=0;for(const auto& item:m.SelectedBook().members)if(!i||(item.units&(1U<<(i-1))))++count;
            const int y=49+(i-start)*36;if(i==s.cursor)Box(12,y,376,33);
            Text(24,y+2,m.SelectedBook().units[i]+"  "+Num(count),20,i==s.cursor);
        }Footer("上键 上一项","OK 开始","下键 下一项");
    }else if(page==wm::Page::Stats){
        const auto d=m.Today(raw);unsigned stars=0,wrong=0;
        for(const auto& r:m.rows){stars+=r.favorite;wrong+=r.wrong;}
        Center(46,"今日统计",28);
        Text(20,90,"新词 "+Num(d.new_words)+"    评分次数 "+Num(d.ratings),20);
        Text(20,122,"记住 "+Num(d.remembered)+"    模糊 "+Num(d.uncertain),20);
        Text(20,154,"忘了 "+Num(d.forgot)+"    到期 "+Num(m.Due(raw)),20);
        Text(20,186,"错词 "+Num(wrong)+"    收藏 "+Num(stars),20);
        if(raw<0)Text(20,221,"时间无效，今日统计待校时",16);
        Footer("","OK 返回","");
    }else if(page==wm::Page::Done){
        Center(72,scope,20);Center(118,"暂无到期任务",28);
        Center(165,"或已达新词额度、已暂跳",20);
        Center(205,"可稍后继续复习",16);Footer("上键 首页","OK 再检查","下键 首页");
    }else if(s.current>=0&&size_t(s.current)<assets::kCount){
        const auto& w=assets::kWords[s.current];
        if(page==wm::Page::Options){
            Center(44,w.lemma,20);
            std::vector<std::string> labels={m.rows[s.current].favorite?"取消收藏":"收藏本词",m.rows[s.current].wrong?"移出错词本":"加入错词本","返回当前卡片","返回首页"};
            for(unsigned i=0;i<labels.size();++i){const int y=84+i*36;if(i==s.cursor)Box(12,y,376,33);Text(24,y+2,labels[i],20,i==s.cursor);}
            Footer("上键 上一项","OK 确认","下键 下一项");
        }else{
            Text(14,42,scope,16);
            Text(325,42,m.rows[s.current].favorite?"已收藏":"",16);
            int size=Width(w.lemma,36)<=370?36:28;
            if(Wrap(w.lemma,size,370).size()>2)size=20;
            const auto name=Wrap(w.lemma,size,370);
            int y=68;
            for(const auto& line:name){Center(y,line,size);y+=size+8;}
            if(name.size()>3)fits=false;
            std::vector<std::string> detail;
            if(w.ipa[0]){auto ipa=Wrap(w.ipa,16,370);detail.insert(detail.end(),ipa.begin(),ipa.end());}
            if(s.back){
                const auto gloss=Wrap(AnswerText(w),16,370);
                detail.insert(detail.end(),gloss.begin(),gloss.end());
                const unsigned per_page=std::max(1,(228-y)/22);
                answer_pages=std::max(1U,static_cast<unsigned>((detail.size()+per_page-1)/per_page));
                const unsigned page=std::min(answer_page,answer_pages-1);
                for(unsigned i=page*per_page;i<std::min<unsigned>((page+1)*per_page,detail.size());++i){
                    Center(y,detail[i],16);y+=22;
                }
                if(answer_pages>1)Text(160,234,std::to_string(page+1)+" / "+std::to_string(answer_pages),16);
                if(page+1<answer_pages)Footer("上键 上一页","OK 下一页","下键 暂跳");
                else if(m.catalog[s.current].gradable)Footer("上键 忘了","OK 记住","下键 模糊");
                else Footer("上键 下一词","OK 下一词","下键 下一词");
            }else{
                // Long IPA is shown in full on the answer pages, never clipped.
                for(const auto& line:detail){if(y+22>174)break;Center(y,line,16);y+=22;}
                Center(194,m.catalog[s.current].gradable?"先回忆，再看答案":"英文索引，中文待补充",20);
                Footer("上键 管理","OK 答案","下键 暂跳");
            }
        }
    }
    if(!fits){Box(12,225,376,29,false);Text(15,225,"释义过长，暂不评分请返回",16);}
    if(!notice.empty()){Box(12,226,376,30,false);Text(16,228,notice,16);}
    return fits&&missing_glyphs==0;
}

#include "offline.h"
#include <algorithm>
#include <limits>
#include <set>
#include <utility>
#include <unordered_map>
namespace wm {
namespace {
uint32_t Crc(const std::vector<uint8_t>& data,size_t n) {
    uint32_t crc=0xffffffff;
    for(size_t i=0;i<n;++i){crc^=data[i];for(int b=0;b<8;++b)crc=(crc>>1)^(0xedb88320U & (0U-(crc&1)));}
    return ~crc;
}
void Put(std::vector<uint8_t>& b,uint64_t value,int n) {for(int i=0;i<n;++i)b.push_back((value>>(8*i))&255);}
struct Reader {
    const std::vector<uint8_t>& b;size_t at=0;bool ok=true;
    uint64_t Get(int n) {if(at+n>b.size()){ok=false;return 0;}uint64_t v=0;for(int i=0;i<n;++i)v|=uint64_t(b[at++])<<(i*8);return v;}
    std::string String() {size_t n=Get(1);if(!ok||n>63||at+n>b.size()){ok=false;return {};}
        std::string s(b.begin()+at,b.begin()+at+n);at+=n;return s;}
};
void String(std::vector<uint8_t>& b,const std::string& s) {Put(b,s.size(),1);b.insert(b.end(),s.begin(),s.end());}
}
Offline::Offline(std::vector<Item> c,uint32_t sig):catalog(std::move(c)),rows(catalog.size()),signature(sig) {
    Book fallback{"pep-g7-upper","人教7年级上册",{"全部单元","Starter 1","Starter 2","Starter 3","Unit 1","Unit 2","Unit 3","Unit 4","Unit 5","Unit 6","Unit 7"},{}};
    for(size_t i=0;i<catalog.size();++i)fallback.members.push_back({static_cast<uint32_t>(i),catalog[i].units});
    ConfigureBooks({std::move(fallback)});
}
void Offline::ConfigureBooks(std::vector<Book> value){
    if(value.empty())return;
    books=std::move(value);
    membership_masks_.assign(books.size(),std::vector<uint16_t>(catalog.size(),0));
    for(size_t b=0;b<books.size();++b)for(const auto& member:books[b].members)
        if(member.index<catalog.size())membership_masks_[b][member.index]|=member.units;
    if(session.book>=books.size())session.book=0;
}
int64_t Offline::Now(int64_t raw) const {
    if(raw<0)return -1;
    for(const auto& row:rows)raw=std::max(raw,row.progress.last_review_at);
    return raw;
}
const Daily Offline::Today(int64_t raw) const {
    const auto now=Now(raw);if(now<0)return {};
    for(const auto& day:history)if(day.day==Day(now))return day;
    Daily d;d.day=Day(now);return d;
}
unsigned Offline::Due(int64_t raw) const {
    const auto now=Now(raw);if(now<0)return 0;
    unsigned n=0;for(const auto& r:rows)if(r.progress.reviews && r.progress.due_at<=now)++n;return n;
}
Offline::BookProgress Offline::CurrentProgress(int64_t raw) const {
    BookProgress out;const auto now=Now(raw);
    for(const auto& member:SelectedBook().members){
        if(member.index>=rows.size())continue;
        if(!catalog[member.index].gradable)continue;
        ++out.total;const auto& p=rows[member.index].progress;
        if(p.reviews){++out.studied;if(now>=0&&p.due_at<=now)++out.due;}
    }
    return out;
}
unsigned Offline::QuoteIndex(int64_t raw){
    if(raw<0)return 0;
    const uint64_t day=Day(raw),cycle=day/50;
    // Fisher-Yates, deterministic per 50-day block: shuffled, no repeats within block.
    unsigned order[50];for(unsigned i=0;i<50;++i)order[i]=i;
    uint32_t seed=static_cast<uint32_t>(cycle)^0x93a47e21U;
    for(unsigned i=49;i>0;--i){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;
        unsigned j=seed%(i+1);std::swap(order[i],order[j]);}
    return order[day%50];
}
bool Offline::Eligible(size_t i) const {
    const uint16_t mask=membership_masks_[session.book][i];
    if(!mask)return false;
    if(session.unit && !(mask&(1U<<(session.unit-1))))return false;
    if(session.scope==Scope::Wrong && !rows[i].wrong)return false;
    if(session.scope==Scope::Favorites && !rows[i].favorite)return false;
    return true;
}
void Offline::Select(int64_t raw) {
    const auto now=Now(raw);session.current=-1;session.back=false;
    if(now<0){session.page=Page::Home;return;}
    int due=-1,unseen=-1;unsigned introduced=0;
    for(size_t i=0;i<rows.size();++i){
        const auto& r=rows[i];const auto& p=r.progress;
        if(p.reviews&&p.introduced_day==Day(now))++introduced;
        if(!Eligible(i)||r.skipped)continue;
        if(p.reviews && p.due_at<=now && (due<0||p.due_at<rows[due].progress.due_at))due=i;
        if(!p.reviews && unseen<0)unseen=i;
    }
    session.current=due>=0?due:((introduced<session.new_limit||(unseen>=0&&!catalog[unseen].gradable))?unseen:-1);
    session.page=session.current>=0?Page::Card:Page::Done;
}
bool Offline::Input(Button key,bool hold,int64_t raw) {
    if(hold){if(key==Button::Ok){ReturnDashboard();return true;}return false;}
    const auto now=Now(raw);
    auto move=[&](unsigned count){session.cursor=(session.cursor+(key==Button::Up?count-1:1))%count;};
    if(session.page==Page::Dashboard){
        if(key==Button::Down){session.page=Page::Books;session.cursor=session.book;}
        else if(key==Button::Up){
            if(now<0)return false;
            if(session.current>=0)session.page=Page::Card;
            else{for(auto& r:rows){r.skipped=false;}Select(now);}
        }else{session.page=Page::Home;session.cursor=0;}
        return true;
    }
    if(session.page==Page::Books){
        if(key!=Button::Ok){move(books.size());return true;}
        if(books[session.cursor].members.empty()) {session.page=Page::Unavailable;return true;}
        session.book=session.cursor;session.unit=0;session.scope=Scope::All;
        session.current=-1;session.back=false;session.cursor=0;
        for(auto& r:rows){r.skipped=false;}
        session.page=Page::Units;return true;
    }
    if(session.page==Page::Unavailable){session.page=Page::Books;return true;}
    if(session.page==Page::Home){
        if(key!=Button::Ok){move(9);return true;}
        switch(session.cursor){
        case 0: if(now<0)return false;
            if(session.current>=0)session.page=Page::Card;
            else{for(auto& r:rows){r.skipped=false;}Select(now);}break;
        case 1:session.page=Page::Units;session.cursor=session.unit;break;
        case 2:case 3:if(now<0)return false;
            session.scope=session.cursor==2?Scope::Wrong:Scope::Favorites;session.unit=0;
            for(auto& r:rows){r.skipped=false;}Select(now);break;
        case 4:session.page=Page::Stats;session.cursor=0;break;
        case 5:session.new_limit=session.new_limit==5?10:session.new_limit==10?15:session.new_limit==15?20:5;break;
        case 8:ReturnDashboard();break;
        case 7:session.page=Page::Books;session.cursor=session.book;break;
        case 6:if(now<0)return false;
            session.scope=Scope::All;session.unit=0;for(auto& r:rows){r.skipped=false;}Select(now);break;
        }return true;
    }
    if(session.page==Page::Units){
        if(key!=Button::Ok){move(SelectedBook().units.size());return true;}
        if(now<0)return false;
        session.unit=session.cursor;session.scope=Scope::All;
        for(auto& r:rows){r.skipped=false;}Select(now);return true;
    }
    if(session.page==Page::Stats){ReturnDashboard();return true;}
    if(session.page==Page::Done){
        if(key==Button::Ok && now>=0){for(auto& r:rows){r.skipped=false;}Select(now);}
        else{ReturnDashboard();}return true;
    }
    if(session.current<0 || size_t(session.current)>=rows.size())return false;
    auto& current=rows[session.current];
    if(session.page==Page::Options){
        if(key!=Button::Ok){move(4);return true;}
        if(session.cursor==0)current.favorite=!current.favorite;
        else if(session.cursor==1)current.wrong=!current.wrong;
        else if(session.cursor==3){ReturnDashboard();return true;}
        session.page=Page::Card;return true;
    }
    if(session.page!=Page::Card)return false;
    if(!session.back){
        if(key==Button::Up){session.page=Page::Options;session.cursor=0;}
        else if(key==Button::Ok)session.back=true;
        else {current.skipped=true;Select(now);}return true;
    }
    if(!catalog[session.current].gradable){current.skipped=true;Select(now);return true;}
    if(now<0)return false;
    const int rating=key==Button::Up?0:key==Button::Ok?2:1;
    bool first=current.progress.reviews==0;
    if(!Review(current.progress,rating,now))return false;
    // One correct self-rating does not automatically remove a previously wrong word.
    if(rating==0)current.wrong=true;
    auto it=std::find_if(history.begin(),history.end(),[&](const Daily& d){return d.day==Day(now);});
    if(it==history.end()){Daily d;d.day=Day(now);history.push_back(d);it=history.end()-1;}
    ++it->ratings;if(first)++it->new_words;
    if(rating==0)++it->forgot;else if(rating==1)++it->uncertain;else ++it->remembered;
    if(history.size()>90)history.erase(history.begin());
    Select(now);return true;
}
std::vector<uint8_t> Offline::Encode() const {
    std::vector<uint8_t> out;Put(out,0x324d575a,4);Put(out,3,4);Put(out,generation,8);Put(out,signature,4);
    Put(out,static_cast<uint32_t>(session.page),4);Put(out,static_cast<uint32_t>(session.scope),4);
    Put(out,session.back,1);Put(out,session.unit,4);Put(out,session.cursor,4);Put(out,session.new_limit,4);
    String(out,session.current>=0?catalog[session.current].id:std::string{});
    String(out,SelectedBook().id);
    Put(out,history.size(),4);
    for(const auto& d:history){Put(out,d.day,8);for(auto n:{d.ratings,d.new_words,d.forgot,d.uncertain,d.remembered})Put(out,n,4);}
    size_t touched=0;
    for(const auto& row:rows)if(row.progress.reviews||row.favorite||row.wrong||row.skipped)++touched;
    Put(out,touched,4);
    for(size_t i=0;i<rows.size();++i){
        const auto& r=rows[i];const auto& p=r.progress;
        if(!p.reviews&&!r.favorite&&!r.wrong&&!r.skipped)continue;
        String(out,catalog[i].id);
        Put(out,p.due_at,8);Put(out,p.last_review_at,8);Put(out,p.introduced_day,8);
        Put(out,p.reviews,4);Put(out,p.lapses,4);Put(out,p.stage,4);
        Put(out,unsigned(r.favorite)|(unsigned(r.wrong)<<1)|(unsigned(r.skipped)<<2),1);
    }
    Put(out,Crc(out,out.size()),4);return out;
}
bool Offline::Decode(const std::vector<uint8_t>& bytes){
    if(bytes.size()<48 || bytes.size()>1024*1024)return false;
    Reader tail{bytes,bytes.size()-4};if(tail.Get(4)!=Crc(bytes,bytes.size()-4))return false;
    Reader in{bytes};if(in.Get(4)!=0x324d575a)return false;
    const auto version=in.Get(4);if(version!=2&&version!=3)return false;
    Offline next(catalog,signature);next.ConfigureBooks(books);next.generation=in.Get(8);const uint32_t saved_sig=in.Get(4);
    auto& s=next.session;s.page=static_cast<Page>(in.Get(4));s.scope=static_cast<Scope>(in.Get(4));
    s.back=in.Get(1);s.unit=in.Get(4);s.cursor=in.Get(4);s.new_limit=in.Get(4);
    const auto current=in.String();
    bool missing_book=false;
    if(version==3){
        const auto book_id=in.String();
        auto found=std::find_if(books.begin(),books.end(),[&](const Book& b){return b.id==book_id;});
        if(found==books.end())missing_book=true;else s.book=found-books.begin();
    }
    const auto days=in.Get(4);
    if(days>90||s.page>Page::Dashboard||s.scope>Scope::Favorites||s.unit>16||s.cursor>1000||s.new_limit<1||s.new_limit>50)return false;
    std::set<int64_t> day_ids;
    for(size_t i=0;i<days;++i){Daily d;d.day=static_cast<int64_t>(in.Get(8));
        d.ratings=in.Get(4);d.new_words=in.Get(4);d.forgot=in.Get(4);d.uncertain=in.Get(4);d.remembered=in.Get(4);
        if(d.day<0||!day_ids.insert(d.day).second || uint64_t(d.forgot)+d.uncertain+d.remembered!=d.ratings||d.new_words>d.ratings)return false;
        next.history.push_back(d);
    }
    const auto count=in.Get(4);if(count>10000)return false;
    std::set<std::string> ids;
    std::unordered_map<std::string,size_t> index;
    for(size_t i=0;i<catalog.size();++i)index[catalog[i].id]=i;
    const auto current_index=index.find(current);
    if(current_index!=index.end())s.current=current_index->second;
    for(size_t n=0;n<count;++n){
        const auto id=in.String();Row r;auto& p=r.progress;
        p.due_at=static_cast<int64_t>(in.Get(8));p.last_review_at=static_cast<int64_t>(in.Get(8));p.introduced_day=static_cast<int64_t>(in.Get(8));
        p.reviews=in.Get(4);p.lapses=in.Get(4);p.stage=in.Get(4);const auto flags=in.Get(1);
        if(!in.ok||id.empty()||!ids.insert(id).second||p.stage>5||p.due_at<0||p.last_review_at<0||flags>7)return false;
        r.favorite=flags&1;r.wrong=flags&2;r.skipped=flags&4;
        const auto found=index.find(id);
        if(found!=index.end()){const auto idx=found->second;next.rows[idx]=r;if(id==current)s.current=idx;}
    }
    if(!in.ok || in.at+4!=bytes.size())return false;
    next.content_changed=saved_sig!=signature||missing_book;
    if(next.content_changed){
        // Keep progress by ID, but rebuild the session when text/ordering changes.
        s.page=Page::Home;s.current=-1;s.back=false;s.cursor=0;
        for(auto& row:next.rows)row.skipped=false;
    } else if((s.page==Page::Card||s.page==Page::Options)&&s.current<0)return false;
    if(s.page==Page::Home&&s.cursor>8)return false;
    if(s.page==Page::Options&&s.cursor>3)return false;
    if(s.unit>=next.SelectedBook().units.size()){
        s.unit=0;s.current=-1;s.page=Page::Home;s.cursor=0;s.back=false;
    }
    if((s.page==Page::Books||s.page==Page::Unavailable)&&s.cursor>=books.size())return false;
    if(s.page==Page::Units&&s.cursor>=next.SelectedBook().units.size())return false;
    if(s.current>=0&&!next.Eligible(s.current)) {s.current=-1;s.back=false;s.page=Page::Home;s.cursor=0;}
    *this=std::move(next);return true;
}
}

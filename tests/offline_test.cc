#include "offline.h"
#include <cassert>
#include <iostream>
using namespace wm;
std::vector<Item> Items(){return {{"a_very_long_word_id_over_15_characters",1},{"second",1},{"third",2}};}
void Click(Offline& m,Button key,int64_t t){assert(m.Input(key,false,t));}
int main(){
    const int64_t t=1800000000;
    Offline m(Items(),123);
    Click(m,Button::Ok,t);assert(m.session.current==0&&!m.session.back);
    // Manage favorite without grading, return, skip and preserve the skip queue.
    Click(m,Button::Up,t);Click(m,Button::Ok,t);
    assert(m.rows[0].favorite&&m.rows[0].progress.reviews==0);
    Click(m,Button::Down,t);assert(m.rows[0].skipped&&m.session.current==1);
    Click(m,Button::Ok,t);assert(m.session.back);
    m.generation=8;const auto blob=m.Encode();
    Offline reboot(Items(),123);assert(reboot.Decode(blob));
    assert(reboot.generation==8&&reboot.session.current==1&&reboot.session.back);
    assert(reboot.rows[0].favorite&&reboot.rows[0].skipped);
    Click(reboot,Button::Up,t);assert(reboot.rows[1].wrong);
    assert(reboot.Today(t).ratings==1&&reboot.Today(t).new_words==1);
    assert(reboot.Today(t).forgot==1&&reboot.session.current==2);
    auto candidate=reboot;Click(candidate,Button::Ok,t);Click(candidate,Button::Ok,t);
    // Failed persistence means caller retains the old model, not candidate.
    assert(reboot.Today(t).ratings==1&&candidate.Today(t).ratings==2);
    auto broken=candidate.Encode();broken[20]^=1;
    const auto original=reboot.Encode();assert(!reboot.Decode(broken));assert(reboot.Encode()==original);
    for(size_t n:{size_t(0),size_t(30),original.size()-1}){
        auto truncated=std::vector<uint8_t>(original.begin(),original.begin()+n);assert(!reboot.Decode(truncated));}
    // A content update re-associates by ID, never by array index.
    Offline reordered({{"third",2},{"second",1},{"a_very_long_word_id_over_15_characters",1},{"new",4}},124);
    assert(reordered.Decode(original));assert(reordered.rows[1].progress.reviews==1);
    assert(reordered.rows[2].favorite&&reordered.session.page==Page::Home&&reordered.session.current==-1);
    assert(reordered.content_changed);
    // Unit filtering still uses the global daily new-word quota.
    Offline units(Items(),5);units.session.new_limit=1;units.session.unit=1;units.Select(t);
    Click(units,Button::Ok,t);Click(units,Button::Ok,t);assert(units.session.page==Page::Done);
    units.session.unit=2;units.Select(t);assert(units.session.page==Page::Done);
    units.Select(t+86400);assert(units.session.current==2);
    assert(units.Now(t-100)==t);
    // Wrong/favorite queues share progress and wait for due reviews.
    reboot.session.scope=Scope::Wrong;reboot.session.unit=0;
    for(auto& row:reboot.rows)row.skipped=false;
    reboot.Select(t);assert(reboot.session.page==Page::Done);
    reboot.Select(t+600);assert(reboot.session.current==1);
    reboot.session.scope=Scope::Favorites;reboot.Select(t);assert(reboot.session.current==0);
    // No rating permitted without a valid clock.
    reboot.session.back=true;auto before=reboot.Encode();assert(!reboot.Input(Button::Ok,false,-1));assert(reboot.Encode()==before);
    // Persisted daily totals survive crossing midnight/reboot without double counting.
    Offline stats(Items(),123);assert(stats.Decode(original));assert(stats.Today(t).ratings==1);
    assert(stats.Today(t+86400).ratings==0);
    // Exercise actual menu paths (rather than assigning unit/scope directly).
    Offline menu(Items(),1);
    Click(menu,Button::Down,t);Click(menu,Button::Ok,t);
    assert(menu.session.page==Page::Units);
    Click(menu,Button::Down,t);Click(menu,Button::Down,t);Click(menu,Button::Ok,t);
    assert(menu.session.unit==2&&menu.session.current==2);
    assert(menu.Input(Button::Ok,true,t));assert(menu.session.page==Page::Dashboard);Click(menu,Button::Ok,t);
    for(int n=0;n<5;++n)Click(menu,Button::Down,t);
    Click(menu,Button::Ok,t);assert(menu.session.new_limit==15);
    Click(menu,Button::Up,t);Click(menu,Button::Ok,t);assert(menu.session.page==Page::Stats);
    Click(menu,Button::Ok,t);assert(menu.session.page==Page::Dashboard);Click(menu,Button::Ok,t);
    // Favorites can be cancelled; wrong status can be explicitly removed.
    Offline manage(Items(),1);Click(manage,Button::Ok,t);Click(manage,Button::Up,t);
    Click(manage,Button::Ok,t);assert(manage.rows[0].favorite);
    Click(manage,Button::Up,t);Click(manage,Button::Ok,t);assert(!manage.rows[0].favorite);
    Click(manage,Button::Up,t);Click(manage,Button::Down,t);Click(manage,Button::Ok,t);
    assert(manage.rows[0].wrong);
    Click(manage,Button::Up,t);Click(manage,Button::Down,t);Click(manage,Button::Ok,t);assert(!manage.rows[0].wrong);
    std::cout<<"offline: identity, long IDs, session/face/skips, favorites, wrong, quotas, stats, CRC/reorder PASS\n";
}

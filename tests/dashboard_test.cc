#include "offline.h"
#include "display_update.h"
#include "answer_navigation.h"
#include <cassert>
#include <set>
#include <iostream>
using namespace wm;
int main(){
    const int64_t t=1800000000;
    Offline model({{"one",1},{"two",1},{"english_only",1,false}},123);
    model.ReturnDashboard();
    assert(model.session.page==Page::Dashboard);
    assert(model.CurrentProgress(t).total==2&&model.CurrentProgress(t).studied==0);
    assert(model.Input(Button::Ok,false,t)&&model.session.page==Page::Home);
    assert(model.Input(Button::Ok,false,t)&&model.session.current==0);
    assert(model.Input(Button::Ok,false,t)&&model.session.back);
    assert(model.Input(Button::Ok,true,t)&&model.session.page==Page::Dashboard);
    auto persisted=model.Encode();Offline resumed(model.catalog,123);assert(resumed.Decode(persisted));
    assert(resumed.session.page==Page::Dashboard&&resumed.session.back);
    assert(resumed.Input(Button::Up,false,t)&&resumed.session.page==Page::Card&&resumed.session.back);
    assert(resumed.Input(Button::Ok,false,t));
    assert(resumed.Input(Button::Ok,true,t));
    assert(resumed.CurrentProgress(t).studied==1);
    assert(resumed.Today(t).new_words==1&&resumed.Today(t).ratings==1);
    assert(resumed.CurrentProgress(t+86400).due==1);
    // Done -> short Up/Down go to dashboard. Menu's last entry also goes home.
    for(auto key:{Button::Up,Button::Down}){resumed.session.page=Page::Done;
        assert(resumed.Input(key,false,t)&&resumed.session.page==Page::Dashboard);}
    resumed.session.page=Page::Home;resumed.session.cursor=8;
    assert(resumed.Input(Button::Ok,false,t)&&resumed.session.page==Page::Dashboard);
    assert(resumed.Input(Button::Down,false,t)&&resumed.session.page==Page::Books);
    for(auto page:{Page::Home,Page::Units,Page::Card,Page::Done,Page::Stats,Page::Options,Page::Books,Page::Unavailable}){
        resumed.session.page=page;
        assert(resumed.Input(Button::Ok,true,t)&&resumed.session.page==Page::Dashboard);
    }
    // Each calendar day has a stable shuffled quote; no repeat in each 50-day block.
    for(int block=400;block<403;++block){std::set<unsigned> values;
        for(int i=0;i<50;++i){auto raw=int64_t(block*50+i)*kDay;
            assert(Offline::QuoteIndex(raw)<50);values.insert(Offline::QuoteIndex(raw));
            assert(Offline::QuoteIndex(raw)==Offline::QuoteIndex(raw+100));}
        assert(values.size()==50);}
    std::array<uint8_t,15000> a,b;a.fill(255);b=a;
    auto first=PlanFrame(a,b,false,0);assert(first.full&&first.pixels.size()==15000);
    assert(PlanFrame(a,b,true,0).width==0);
    b[100*50+2]=0;b[102*50+4]=1;
    auto normal=PlanFrame(a,b,true,0);assert(normal.full);
    auto rejected_clock=PlanFrame(a,b,true,0,true);assert(rejected_clock.full);
    a.fill(255);b=a;b[10*50+2]=0;b[12*50+4]=1;
    auto partial=PlanFrame(a,b,true,0,true);
    assert(!partial.full&&partial.x==16&&partial.y==10&&partial.width==24&&partial.height==3);
    assert(partial.pixels.size()==9&&partial.pixels[0]==0&&partial.pixels[8]==1);
    assert(PlanFrame(a,b,true,12,true).full);
    // Full redraw is required for midnight quote/date/progress changes outside header.
    auto rollover=b;rollover[200*50]=0;
    assert(PlanFrame(a,rollover,true,0,true).full);
    auto big_clock=a;big_clock[70*50+30]=0;
    assert(!PlanFrame(a,big_clock,true,0,true).full);
    auto date_change=big_clock;date_change[90*50]=0;
    assert(PlanFrame(a,date_change,true,0,true).full);
    // Patch reproduces exact target and byte-aligned bounds contain every changed pixel.
    auto reconstructed=a;
    for(int y=0;y<partial.height;++y)for(int x=0;x<partial.width/8;++x)
        reconstructed[(partial.y+y)*50+partial.x/8+x]=partial.pixels[y*(partial.width/8)+x];
    assert(reconstructed==b);
    assert(AnswerInput(0,3,Button::Ok)==AnswerAction::Next);
    assert(AnswerInput(1,3,Button::Up)==AnswerAction::Previous);
    assert(AnswerInput(0,3,Button::Down)==AnswerAction::Skip);
    assert(AnswerInput(2,3,Button::Ok)==AnswerAction::Grade);
    assert(AnswerInput(0,1,Button::Up)==AnswerAction::Grade);
    std::cout<<"dashboard: navigation, live progress, quotes, atomic resume, partial refresh plan PASS\n";
}

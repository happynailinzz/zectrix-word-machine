#include "learning.h"
#include <cassert>
#include <iostream>
using namespace wm;
int main() {
    const int64_t t=1800000000;
    Progress p;
    assert(!Review(p,3,t) && p.reviews==0);
    assert(Review(p,2,t) && p.due_at==t+kDay && p.stage==1);
    assert(Review(p,2,t+kDay) && p.due_at==t+4*kDay);
    assert(Review(p,0,t+4*kDay) && p.stage==0 && p.lapses==1);
    assert(p.due_at==t+4*kDay+600);
    const auto previous=p.last_review_at;
    assert(Review(p,1,t) && p.last_review_at==previous);
    Progress rows[3];
    assert(Choose(rows,3,t,1)==0);
    Review(rows[1],2,t);
    assert(Choose(rows,3,t,1)==-1);
    rows[1].due_at=t-1;
    assert(Choose(rows,3,t,1)==1);
    uint8_t excluded[]={0,1,0};
    assert(Choose(rows,3,t,1,excluded)==-1);
    assert(Choose(rows,3,t+kDay,1,excluded)==0);
    // No grade on down-key press/hold/release, exactly one hold.
    Key down;
    assert(down.Sample(false,0,3000)==Gesture::None);
    assert(down.Sample(true,10,3000)==Gesture::None);
    assert(down.Sample(true,50,3000)==Gesture::None);
    assert(down.Sample(true,3050,3000)==Gesture::Hold);
    assert(down.Sample(true,3100,3000)==Gesture::None);
    assert(down.Sample(false,3200,3000)==Gesture::None);
    assert(down.Sample(false,3240,3000)==Gesture::None);
    // Short click emits on release after debounce, only once.
    assert(down.Sample(true,4000,3000)==Gesture::None);
    assert(down.Sample(true,4040,3000)==Gesture::None);
    assert(down.Sample(false,4100,3000)==Gesture::None);
    assert(down.Sample(false,4140,3000)==Gesture::Click);
    assert(down.Sample(false,4180,3000)==Gesture::None);
    Key boot;
    boot.Sample(true,0,3000);boot.Sample(true,40,3000);
    assert(boot.Sample(true,4000,3000)==Gesture::None);
    boot.Sample(false,4010,3000);
    assert(boot.Sample(false,4050,3000)==Gesture::None);
    assert(Day(16*3600-1)==0 && Day(16*3600)==1);
    // Reproduce 20ms FreeRTOS sampling after a 1.3s display gate.
    for(uint32_t hold_ms:{0U,700U,3000U}){
        Key sampled;unsigned clicks=0,holds=0;
        for(uint64_t ms=0;ms<=1800;ms+=20){
            if(ms<1300){sampled.Reset();continue;}
            const auto gesture=sampled.Sample(ms>=1400&&ms<1520,ms,hold_ms);
            clicks+=gesture==Gesture::Click;holds+=gesture==Gesture::Hold;
        }
        assert(clicks==1&&holds==0);
    }
    Key return_home;unsigned clicks=0,holds=0;
    for(uint64_t ms=0;ms<=1400;ms+=20){
        auto gesture=return_home.Sample(ms>=100&&ms<1100,ms,700);
        clicks+=gesture==Gesture::Click;holds+=gesture==Gesture::Hold;
    }
    assert(holds==1&&clicks==0);
    std::cout<<"core: scheduling, quotas, rollback, hold/click, boot-held PASS\n";
}

#pragma once
#include <array>
#include <cstdint>
#include <vector>
namespace wm {
struct FrameChange {int x=0,y=0,width=0,height=0;bool full=false;std::vector<uint8_t> pixels;};
inline FrameChange PlanFrame(const std::array<uint8_t,15000>& old_frame,
                            const std::array<uint8_t,15000>& next,bool base,unsigned partials,bool clock_only=false){
    FrameChange change;
    int left=50,right=-1,top=300,bottom=-1;
    for(int y=0;y<300;++y)for(int x=0;x<50;++x)
        if(old_frame[y*50+x]!=next[y*50+x]){
            if(x<left){left=x;} if(x>right){right=x;} if(y<top){top=y;} if(y>bottom){bottom=y;}}
    if(base&&right<0)return change;
    // Hardware-proven full refresh for interaction. Partial updates are restricted
    // to automatic dashboard ticks whose changed pixels stay inside the time row.
    const bool safe_clock=clock_only && base && top>=0 && bottom<82 && left>=0;
    if(!safe_clock||partials>=12){change.full=true;change.width=400;change.height=300;
        change.pixels.assign(next.begin(),next.end());return change;}
    change.x=left*8;change.y=top;change.width=(right-left+1)*8;change.height=bottom-top+1;
    for(int y=top;y<=bottom;++y)change.pixels.insert(change.pixels.end(),next.begin()+y*50+left,next.begin()+y*50+right+1);
    return change;
}
}

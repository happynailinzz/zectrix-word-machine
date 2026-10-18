// Local preview of the exact V2 device model and renderer. No network required.
#include "runtime_ui.h"
#include "model_catalog.h"
#include <cstdio>
#include <cassert>
#include <ctime>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <sys/stat.h>
int main(int argc,char** argv){
    if(argc<2){std::cerr<<"Usage: offline-preview state|up|ok|down|hold-ok [state-file]\n";return 2;}
    const std::string command=argv[1];
    if(command!="state"&&command!="up"&&command!="ok"&&command!="down"&&command!="hold-ok")return 2;
    const std::string path=argc>2?argv[2]:".local/offline-v2.state";
    auto model=DeviceModel();
    std::ifstream saved(path,std::ios::binary);
    if(saved){std::vector<uint8_t> data((std::istreambuf_iterator<char>(saved)),{});
        if(!model.Decode(data)){std::cerr<<"Invalid snapshot; file preserved\n";return 1;}}
    if(!saved)model.ReturnDashboard();
    if(command!="state"){
        auto candidate=model;
        const auto key=command=="up"?wm::Button::Up:command=="down"?wm::Button::Down:wm::Button::Ok;
        if(!candidate.Input(key,command=="hold-ok",std::time(nullptr))){std::cerr<<"Action rejected\n";return 1;}
        ++candidate.generation;const auto bytes=candidate.Encode();
        std::ofstream out(path+".tmp",std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());out.close();
        if(!out||std::rename((path+".tmp").c_str(),path.c_str())!=0){std::cerr<<"Save failed; previous state kept\n";return 1;}
        model=std::move(candidate);
    }
    std::ifstream file("firmware/main/generated/glyphs.bin",std::ios::binary);
    std::vector<uint8_t> glyphs((std::istreambuf_iterator<char>(file)),{});
    if(glyphs.size()<100000){std::cerr<<"Run from zectrix-word-machine directory\n";return 1;}
    std::ifstream dashfile("firmware/main/generated/dashboard_fonts.bin",std::ios::binary);
    std::vector<uint8_t> dash((std::istreambuf_iterator<char>(dashfile)),{});
    assert(!dash.empty());
    RuntimeUi ui(glyphs.data(),dash.data());if(!ui.Draw(model,std::time(nullptr)))return 1;
    std::ofstream image("output/offline-v2.pgm",std::ios::binary);image<<"P5\n400 300\n255\n";
    for(int y=0;y<300;++y)for(int x=0;x<400;++x){const char p=(ui.frame[y*50+x/8]&(1U<<(7-x%8)))?char(255):0;image.write(&p,1);}
    const auto stats=model.Today(std::time(nullptr));
    std::cout<<"generation="<<model.generation<<" page="<<static_cast<unsigned>(model.session.page)
        <<" cursor="<<model.session.cursor<<" unit="<<model.session.unit<<" back="<<model.session.back
        <<" current="<<(model.session.current<0?"none":model.catalog[model.session.current].id)
        <<" ratings="<<stats.ratings<<" new="<<stats.new_words<<" snapshot_bytes="<<model.Encode().size()
        <<" image=output/offline-v2.pgm\n";
}

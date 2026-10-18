#include "runtime_ui.h"
#include "model_catalog.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
int main(int argc,char** argv){
    assert(argc==3);
    std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> glyphs((std::istreambuf_iterator<char>(file)),{});
    assert(glyphs.size()>100000);
    auto model=DeviceModel();std::ifstream dashfile("firmware/main/generated/dashboard_fonts.bin",std::ios::binary);
    std::vector<uint8_t> dash((std::istreambuf_iterator<char>(dashfile)),{});
    assert(!dash.empty());
    RuntimeUi ui(glyphs.data(),dash.data());
    auto save=[&](const std::string& name){
        std::ofstream out(std::string(argv[2])+"/"+name+".pgm",std::ios::binary);
        out<<"P5\n400 300\n255\n";
        for(int y=0;y<300;++y)for(int x=0;x<400;++x){const char p=(ui.frame[y*50+x/8]&(1U<<(7-x%8)))?char(255):0;out.write(&p,1);}
    };
    unsigned frames=0,errors=0;
    for(size_t i=0;i<assets::kCount;++i){
        for(bool back:{false,true}){
            for(size_t b=0;b<model.books.size();++b){
                bool found=false;for(const auto& m:model.books[b].members)if(m.index==i)found=true;
                if(found){model.session.book=b;break;}
            }
            model.session.page=wm::Page::Card;model.session.current=i;model.session.back=back;
            if(!ui.Draw(model,1800000000)){std::cerr<<"LAYOUT: "<<assets::kWords[i].lemma<<" back="<<back<<" missing="<<ui.missing_glyphs<<"\n";++errors;}
            ++frames;
            if(back){const unsigned pages=ui.answer_pages;
                for(unsigned p=1;p<pages;++p){if(!ui.Draw(model,1800000000,"",p))++errors;++frames;}
                if(pages>1){ui.Draw(model,1800000000,"",0);save("long-answer-first");
                    ui.Draw(model,1800000000,"",pages-1);save("long-answer-last");}
            }
            if(i==0||std::string(assets::kWords[i].lemma)=="gym"||std::string(assets::kWords[i].lemma)=="would"||std::string(assets::kWords[i].lemma)=="Florence Nightingale"||std::string(assets::kWords[i].lemma)=="fox"||std::string(assets::kWords[i].lemma)=="record"||std::string(assets::kWords[i].lemma)=="Hans Christian Andersen")
                save(std::to_string(i)+(back?"-back":"-front"));
        }
    }
    for(size_t b=0;b<model.books.size();++b){
        if(model.books[b].members.empty())continue;
        model.session.book=b;model.session.current=model.books[b].members[0].index;
        model.session.page=wm::Page::Card;model.session.back=true;
        if(!ui.Draw(model,1800000000))++errors;
        save("book-"+std::to_string(b)+"-back");
        model.session.page=wm::Page::Units;model.session.cursor=0;
        if(!ui.Draw(model,1800000000))++errors;
        save("book-"+std::to_string(b)+"-units");
    }
    model.session.book=0;
    for(auto page:{wm::Page::Home,wm::Page::Units,wm::Page::Stats,wm::Page::Options,wm::Page::Done}){
        model.session.page=page;model.session.cursor=0;
        const int count=page==wm::Page::Home?9:page==wm::Page::Units?11:page==wm::Page::Options?4:1;
        for(int i=0;i<count;++i){model.session.cursor=i;if(!ui.Draw(model,1800000000))++errors;++frames;}
        if(!ui.Draw(model,1800000000))++errors;
        save("page-"+std::to_string(static_cast<int>(page)));
    }
    for(unsigned b=0;b<model.books.size();++b){
        if(model.books[b].members.empty())continue;
        model.session.book=b;model.session.page=wm::Page::Units;
        for(unsigned u=0;u<model.books[b].units.size();++u){
            model.session.cursor=u;if(!ui.Draw(model,1800000000))++errors;++frames;
        }
    }
    model.session.book=0;
    for(unsigned i=0;i<assets::kBookCount;++i){
        model.session.page=wm::Page::Books;model.session.cursor=i;
        if(!ui.Draw(model,1800000000))++errors;
        ++frames;if(i==assets::kBookCount-1)save("books");
        if(model.books[i].members.empty()){model.session.page=wm::Page::Unavailable;if(!ui.Draw(model,1800000000))++errors;++frames;}
    }
    model.session.page=wm::Page::Unavailable;model.session.cursor=5;
    ui.Draw(model,1800000000);save("pending-g9-lower");
    for(size_t b=0;b<model.books.size();++b){
        if(model.books[b].members.empty())continue;
        model.session.book=b;model.ReturnDashboard();
        // Check each of the 50 daily quote choices in the actual firmware renderer.
        for(unsigned day=20000;day<20050;++day){
            if(!ui.Draw(model,int64_t(day)*86400))++errors;++frames;
        }
        model.rows[model.books[b].members[0].index].progress.reviews=1;
        if(!ui.Draw(model,1800000000))++errors;
        if(b==0||b==6)save("dashboard-book-"+std::to_string(b));
    }
    model.session.book=0;
    for(const std::string& notice:{"存储损坏，停用学习请保留数据","时间无效，需校时勿评分","保存失败，请重试","恢复旧版：今日评分待核实","词库有变更，会话已重建"}){
        model.session.page=wm::Page::Home;if(!ui.Draw(model,-1,notice))++errors;
    }
    std::cout<<"runtime frames="<<frames<<" errors="<<errors<<"\n";return errors?1:0;
}

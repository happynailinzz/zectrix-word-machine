#pragma once
#include "offline.h"
#include "generated/runtime_assets.h"
inline std::vector<wm::Item> DeviceCatalog(){
    std::vector<wm::Item> items;
    for(const auto& word:assets::kWords)items.push_back({word.id,word.units,word.gradable});
    return items;
}
inline std::vector<wm::Book> DeviceBooks(){
    std::vector<wm::Book> books;
    for(const auto& source:assets::kBooks){
        wm::Book book;book.id=source.id;book.title=source.title;
        for(size_t i=0;i<source.unit_count;++i)book.units.push_back(source.units[i]);
        for(size_t i=0;i<source.member_count;++i)book.members.push_back({source.members[i].index,source.members[i].units,source.members[i].page});
        books.push_back(std::move(book));
    }
    return books;
}
inline wm::Offline DeviceModel(){wm::Offline model(DeviceCatalog(),assets::kContentSignature);model.ConfigureBooks(DeviceBooks());return model;}

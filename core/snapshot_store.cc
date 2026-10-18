#include "snapshot_store.h"
#include <algorithm>
#include <cstdio>
namespace wm {
namespace {
uint32_t Crc(const std::vector<uint8_t>& data){uint32_t c=~0U;for(auto b:data){c^=b;for(int i=0;i<8;++i)c=(c>>1)^(0xedb88320U&(0U-(c&1)));}return ~c;}
void Put(std::vector<uint8_t>& b,uint32_t v){for(int i=0;i<4;++i)b.push_back((v>>(i*8))&255);}
uint32_t Get(const std::vector<uint8_t>& b,size_t i){return uint32_t(b[i])|(uint32_t(b[i+1])<<8)|(uint32_t(b[i+2])<<16)|(uint32_t(b[i+3])<<24);}
std::string Meta(unsigned bank){return bank?"b_meta":"a_meta";}
std::string Key(unsigned bank,size_t index){char key[12];std::snprintf(key,sizeof(key),"%c%03u",bank?'b':'a',static_cast<unsigned>(index));return key;}
}
bool WriteBank(BlobStore& store,unsigned bank,const std::vector<uint8_t>& data){
    if(bank>1||data.empty()||data.size()>kSnapshotMax)return false;
    // Invalidate the target bank before touching any of its chunks. Other bank is intact.
    if(!store.Remove(Meta(bank))||!store.Sync())return false;
    const size_t count=(data.size()+kSnapshotChunk-1)/kSnapshotChunk;
    for(size_t i=0;i<count;++i){const size_t begin=i*kSnapshotChunk,end=std::min(begin+kSnapshotChunk,data.size());
        if(!store.Write(Key(bank,i),std::vector<uint8_t>(data.begin()+begin,data.begin()+end)))return false;}
    if(!store.Sync())return false;
    std::vector<uint8_t> meta;Put(meta,0x33424d57);Put(meta,data.size());Put(meta,count);Put(meta,Crc(data));
    return store.Write(Meta(bank),meta)&&store.Sync();
}
ReadResult ReadBank(BlobStore& store,unsigned bank,std::vector<uint8_t>& out){
    if(bank>1)return ReadResult::Error;
    std::vector<uint8_t> meta;const auto result=store.Read(Meta(bank),meta);
    if(result!=ReadResult::Ok)return result;
    if(meta.size()!=16||Get(meta,0)!=0x33424d57)return ReadResult::Error;
    const auto bytes=Get(meta,4),count=Get(meta,8),crc=Get(meta,12);
    if(!bytes||bytes>kSnapshotMax||count!=(bytes+kSnapshotChunk-1)/kSnapshotChunk)return ReadResult::Error;
    std::vector<uint8_t> all;all.reserve(bytes);
    for(size_t i=0;i<count;++i){std::vector<uint8_t> chunk;
        if(store.Read(Key(bank,i),chunk)!=ReadResult::Ok||chunk.size()!=std::min(kSnapshotChunk,size_t(bytes)-i*kSnapshotChunk))return ReadResult::Error;
        all.insert(all.end(),chunk.begin(),chunk.end());}
    if(Crc(all)!=crc)return ReadResult::Error;
    out=std::move(all);return ReadResult::Ok;
}
}

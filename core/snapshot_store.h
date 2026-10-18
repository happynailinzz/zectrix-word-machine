#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace wm {
enum class ReadResult { Ok, Missing, Error };
class BlobStore {
public:
    virtual ~BlobStore()=default;
    virtual ReadResult Read(const std::string& key,std::vector<uint8_t>& bytes)=0;
    virtual bool Write(const std::string& key,const std::vector<uint8_t>& bytes)=0;
    virtual bool Remove(const std::string& key)=0;
    virtual bool Sync()=0;
};
// Two independently validated banks. Caller selects highest decoded generation.
constexpr size_t kSnapshotChunk=8192;
constexpr size_t kSnapshotMax=1024*1024;
bool WriteBank(BlobStore& store,unsigned bank,const std::vector<uint8_t>& bytes);
ReadResult ReadBank(BlobStore& store,unsigned bank,std::vector<uint8_t>& bytes);
}

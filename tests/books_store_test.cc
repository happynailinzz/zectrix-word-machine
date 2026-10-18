#include "offline.h"
#include "snapshot_store.h"
#include <cassert>
#include <iostream>
#include <map>
using namespace wm;
struct MemoryStore:BlobStore {
    std::map<std::string,std::vector<uint8_t>> data;
    int operations=-1;
    bool Step(){if(operations==0)return false;if(operations>0)--operations;return true;}
    ReadResult Read(const std::string& key,std::vector<uint8_t>& out) override {
        if(!data.count(key))return ReadResult::Missing;out=data[key];return ReadResult::Ok;}
    bool Write(const std::string& key,const std::vector<uint8_t>& bytes) override {
        assert(key.size()<=15);if(!Step())return false;data[key]=bytes;return true;}
    bool Remove(const std::string& key) override {if(!Step())return false;data.erase(key);return true;}
    bool Sync() override {return Step();}
};
void Click(Offline& m,Button k,int64_t t){assert(m.Input(k,false,t));}
int main(){
    int64_t t=1800000000;
    Offline m({{"shared",1},{"a_only",1},{"b_only",1}},10);
    std::vector<Book> books={{"a","Book A",{"All","Unit 1"},{{0,1},{1,1}}},
        {"b","Book B",{"All","Topic 1"},{{0,1},{2,1}}},{"pending","Pending",{"All"},{}}};
    m.ConfigureBooks(books);m.Select(t);assert(m.session.current==0);
    Click(m,Button::Ok,t);Click(m,Button::Ok,t);assert(m.rows[0].progress.reviews==1);
    // Open book chooser from home, switch to B and confirm topic; shared is not relearned.
    assert(m.Input(Button::Ok,true,t));Click(m,Button::Ok,t);Click(m,Button::Up,t);Click(m,Button::Up,t);assert(m.session.cursor==7);
    Click(m,Button::Ok,t);assert(m.session.page==Page::Books);
    Click(m,Button::Down,t);Click(m,Button::Ok,t);assert(m.session.book==1&&m.session.page==Page::Units);
    Click(m,Button::Down,t);Click(m,Button::Ok,t);assert(m.session.current==2);
    assert(m.Today(t).new_words==1);
    // Sparse state restores current unlearned card even without a per-word row.
    m.generation=4;auto blob=m.Encode();Offline restored(m.catalog,10);restored.ConfigureBooks(books);
    assert(restored.Decode(blob)&&restored.session.book==1&&restored.session.current==2);
    assert(restored.rows[0].progress.reviews==1&&restored.session.unit==1);
    assert(m.Input(Button::Ok,true,t));Click(m,Button::Ok,t);Click(m,Button::Up,t);Click(m,Button::Up,t);Click(m,Button::Ok,t);
    Click(m,Button::Down,t);Click(m,Button::Ok,t);assert(m.session.page==Page::Unavailable&&m.session.book==1);
    Click(m,Button::Ok,t);assert(m.session.page==Page::Books);
    // Registry reorder recovers selected book by stable ID, never numeric position.
    auto reordered=books;std::swap(reordered[0],reordered[1]);Offline other(m.catalog,10);other.ConfigureBooks(reordered);
    assert(other.Decode(blob)&&other.session.book==0&&other.SelectedBook().id=="b");
    // Build a version-2 compatible byte layout and verify migration into book A.
    Offline legacy({{"shared",1},{"a_only",1}},10);legacy.Select(t);Click(legacy,Button::Ok,t);
    auto v2=legacy.Encode();
    const size_t book_offset=42+v2[41];const size_t book_bytes=1+v2[book_offset];
    v2.erase(v2.begin()+book_offset,v2.begin()+book_offset+book_bytes);v2[4]=2;
    uint32_t crc=~0U;for(size_t i=0;i<v2.size()-4;++i){crc^=v2[i];for(int bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320U&(0U-(crc&1)));}
    crc=~crc;for(int i=0;i<4;++i)v2[v2.size()-4+i]=(crc>>(8*i))&255;
    Offline migrated(legacy.catalog,10);assert(migrated.Decode(v2));
    assert(migrated.session.current==0&&migrated.session.back&&migrated.session.book==0);
    // English-only official indices are browseable but never grade placeholder answers.
    Offline english({{"index_only",1,false}},1);english.Select(t);
    Click(english,Button::Ok,t);Click(english,Button::Ok,t);
    assert(english.rows[0].skipped&&english.rows[0].progress.reviews==0);
    assert(english.Today(t).ratings==0&&english.session.page==Page::Done);
    Offline quota({{"learned",1,true},{"browse",1,false}},1);
    assert(Review(quota.rows[0].progress,2,t));quota.session.new_limit=1;quota.Select(t);
    assert(quota.session.current==1); // Browsing an untranslated index consumes no learning quota.
    // KET/PET W-X/Y-Z groups (>10) must persist without invalidating snapshots.
    Offline alphabet({{"zebra",1U<<12}},99);
    Book english_book{"english","English",{"All"},{ {0,1U<<12} }};
    for(int i=1;i<=13;++i)english_book.units.push_back("Band "+std::to_string(i));
    alphabet.ConfigureBooks({english_book});alphabet.session.unit=13;alphabet.Select(t);
    alphabet.rows[0].favorite=true;alphabet.session.back=true;
    Offline alphabet_again(alphabet.catalog,99);alphabet_again.ConfigureBooks({english_book});
    assert(alphabet_again.Decode(alphabet.Encode()));
    assert(alphabet_again.session.unit==13&&alphabet_again.session.back&&alphabet_again.rows[0].favorite);
    // 10k synthetic IDs validate capacity only, not actual installed educational content.
    std::vector<Item> large;for(int i=0;i<10000;++i)large.push_back({"word_identity_"+std::to_string(i),1});
    Offline big(large,50);auto empty=big.Encode();assert(empty.size()<256);
    for(auto& row:big.rows){assert(Review(row.progress,2,t));row.favorite=true;}
    big.generation=10;auto full=big.Encode();assert(full.size()>65536&&full.size()<kSnapshotMax);
    Offline read(large,50);assert(read.Decode(full)&&read.rows.back().favorite);
    MemoryStore store;assert(WriteBank(store,0,full));
    std::vector<uint8_t> out;assert(ReadBank(store,0,out)==ReadResult::Ok&&out==full);
    big.generation=11;auto newer=big.Encode();
    // Power failure at every mutation/sync boundary must preserve old complete bank.
    const int writes=3+int((newer.size()+kSnapshotChunk-1)/kSnapshotChunk)+2;
    for(int stop=0;stop<=writes;++stop){auto interrupted=store;interrupted.operations=stop;
        WriteBank(interrupted,1,newer);std::vector<uint8_t> previous,current;
        assert(ReadBank(interrupted,0,previous)==ReadResult::Ok&&previous==full);
        auto r=ReadBank(interrupted,1,current);assert(r!=ReadResult::Ok||current==newer);}
    assert(WriteBank(store,1,newer));store.data["b000"][1]^=1;
    assert(ReadBank(store,1,out)==ReadResult::Error);assert(ReadBank(store,0,out)==ReadResult::Ok);
    std::cout<<"books/store: shared identity, pending gate, selection restore, 10k records "<<full.size()
        <<" bytes, interrupted banks PASS\n";
}

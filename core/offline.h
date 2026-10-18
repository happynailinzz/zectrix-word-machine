#pragma once
#include "learning.h"
#include <string>
#include <vector>

namespace wm {
struct Item {std::string id; uint16_t units; bool gradable=true;};
enum class Page : uint32_t { Home, Units, Card, Done, Stats, Options, Books, Unavailable, Dashboard };
enum class Scope : uint32_t { All, Wrong, Favorites };
struct Membership {uint32_t index; uint16_t units; uint16_t page=0;};
struct Book {std::string id,title; std::vector<std::string> units; std::vector<Membership> members;};
struct Row {Progress progress; bool favorite=false,wrong=false,skipped=false;};
struct Daily {int64_t day=-1; uint32_t ratings=0,new_words=0,forgot=0,uncertain=0,remembered=0;};
struct Session {Page page=Page::Home; Scope scope=Scope::All; int current=-1;
    bool back=false; uint32_t unit=0,cursor=0,new_limit=10,book=0;};
class Offline {
public:
    explicit Offline(std::vector<Item> catalog,uint32_t signature=0);
    std::vector<Item> catalog;
    std::vector<Row> rows;
    std::vector<Book> books;
    Session session;
    std::vector<Daily> history; // Last 90 studied days, not 90 calendar days.
    uint64_t generation=0;
    uint32_t signature=0;
    bool content_changed=false;
    int64_t Now(int64_t raw) const;
    const Daily Today(int64_t now) const;
    unsigned Due(int64_t now) const;
    void Select(int64_t now);
    // UI input mutates a candidate model. Caller commits it only after durable Save.
    bool Input(Button key,bool hold,int64_t now);
    std::vector<uint8_t> Encode() const;
    bool Decode(const std::vector<uint8_t>& bytes);
    bool Eligible(size_t i) const;
    struct BookProgress {unsigned total=0,studied=0,due=0;};
    BookProgress CurrentProgress(int64_t raw) const;
    static unsigned QuoteIndex(int64_t raw);
    void ReturnDashboard(){session.page=Page::Dashboard;session.cursor=0;}
    void ConfigureBooks(std::vector<Book> value);
    const Book& SelectedBook() const {return books[session.book];}
private:
    std::vector<std::vector<uint16_t>> membership_masks_;
};
}

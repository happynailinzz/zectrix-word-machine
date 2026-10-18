#include "learning.h"
#include <algorithm>
#include <limits>

namespace wm {
int64_t Day(int64_t t) { return (t + kZoneOffset) / kDay; }
int64_t EffectiveNow(const Progress* rows, size_t count, int64_t now) {
    for (size_t i = 0; i < count; ++i) now = std::max(now, rows[i].last_review_at);
    return now;
}
bool Review(Progress& p, int rating, int64_t now) {
    if (rating < 0 || rating > 2 || now < 0 ||
        p.reviews == std::numeric_limits<uint32_t>::max()) return false;
    now = std::max(now, p.last_review_at);
    if (p.reviews == 0) p.introduced_day = Day(now);
    static constexpr int intervals[] = {1, 3, 7, 14, 30, 60};
    if (rating == 0) {
        p.stage = 0;
        if (p.lapses < std::numeric_limits<uint32_t>::max()) ++p.lapses;
        p.due_at = now + 600;
    } else if (rating == 1) {
        p.stage = std::min<uint32_t>(p.stage, 5);
        p.due_at = now + kDay;
    } else {
        const uint32_t stage = std::min<uint32_t>(p.stage, 5);
        p.due_at = now + intervals[stage] * kDay;
        p.stage = std::min<uint32_t>(stage + 1, 5);
    }
    p.last_review_at = now;
    ++p.reviews;
    return true;
}
int Choose(const Progress* rows, size_t n, int64_t now, unsigned limit,
           const uint8_t* excluded) {
    if (now < 0 || limit == 0 || limit > 50) return -1;
    now = EffectiveNow(rows, n, now);
    unsigned introduced = 0;
    int due = -1;
    for (size_t i = 0; i < n; ++i) {
        const auto& p = rows[i];
        if (p.reviews && p.introduced_day == Day(now)) ++introduced;
        if ((!excluded || !excluded[i]) && p.reviews && p.due_at <= now &&
            (due < 0 || p.due_at < rows[due].due_at)) due = static_cast<int>(i);
    }
    if (due >= 0) return due;
    if (introduced >= limit) return -1;
    for (size_t i = 0; i < n; ++i)
        if ((!excluded || !excluded[i]) && rows[i].reviews == 0)
            return static_cast<int>(i);
    return -1;
}
void Key::Reset() { *this = Key{}; }
Gesture Key::Sample(bool pressed, uint64_t now, uint32_t hold_ms) {
    if (pressed != candidate_) { candidate_ = pressed; changed_at_ = now; }
    if (candidate_ != stable_ && now - changed_at_ >= 40) {
        stable_ = candidate_;
        if (stable_) {
            if (armed_) { pressed_at_ = now; held_ = false; }
        } else {
            const bool click = armed_ && !held_;
            armed_ = true;
            if (click) return Gesture::Click;
        }
    }
    // A key held at boot/reset must first be released before it can act.
    if (!stable_ && !pressed) armed_ = true;
    if (stable_ && armed_ && !held_ && hold_ms && now - pressed_at_ >= hold_ms) {
        held_ = true;
        return Gesture::Hold;
    }
    return Gesture::None;
}
}
extern "C" {
size_t wm_progress_size() { return sizeof(wm::Progress); }
int wm_review(wm::Progress* p, int rating, int64_t now) {
    return p && wm::Review(*p, rating, now);
}
int wm_choose(const wm::Progress* p, size_t n, int64_t now, unsigned limit,
              const uint8_t* excluded) {
    if (!p && n) return -1;
    return wm::Choose(p, n, now, limit, excluded);
}
}

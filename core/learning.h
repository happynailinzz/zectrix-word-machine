#ifndef WORD_MACHINE_LEARNING_H
#define WORD_MACHINE_LEARNING_H
#include <cstddef>
#include <cstdint>

namespace wm {
constexpr int64_t kDay = 86400;
constexpr int64_t kZoneOffset = 8 * 3600;  // Study days use Asia/Shanghai.
struct Progress {
    int64_t due_at = 0;
    int64_t last_review_at = 0;
    int64_t introduced_day = -1;
    uint32_t reviews = 0;
    uint32_t lapses = 0;
    uint32_t stage = 0;
    uint32_t reserved = 0;
};
static_assert(sizeof(Progress) == 40, "Progress v1 wire layout changed");
int64_t Day(int64_t timestamp);
// Never move the learning clock backwards after a persisted review.
int64_t EffectiveNow(const Progress* rows, size_t count, int64_t now);
// 0=forgot, 1=uncertain, 2=remembered. Returns false without mutation on bad input.
bool Review(Progress& row, int rating, int64_t now);
// Due reviews first; introduced-day quota counts first completed ratings, not views.
int Choose(const Progress* rows, size_t count, int64_t now,
           unsigned new_limit, const uint8_t* excluded = nullptr);

enum class Button : uint8_t { Up, Ok, Down };
enum class Gesture : uint8_t { None, Click, Hold };
class Key {
public:
    Gesture Sample(bool pressed, uint64_t now_ms, uint32_t hold_ms);
    void Reset();
private:
    bool candidate_ = false, stable_ = false, armed_ = false, held_ = false;
    uint64_t changed_at_ = 0, pressed_at_ = 0;
};
}
extern "C" {
size_t wm_progress_size();
int wm_review(wm::Progress* row, int rating, int64_t now);
int wm_choose(const wm::Progress* rows, size_t count, int64_t now,
              unsigned limit, const uint8_t* excluded);
}
#endif

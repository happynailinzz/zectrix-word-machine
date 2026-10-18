#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "rtc_pcf8563.h"
#include "zectrix_board.h"
#include "zectrix_board_config.h"
#include "zectrix_epd.h"
#include "offline.h"
#include "runtime_ui.h"
#include "model_catalog.h"
#include "snapshot_store.h"
#include "display_update.h"
#include "answer_navigation.h"
extern const uint8_t kGlyphs[] asm("_binary_glyphs_bin_start");
extern const uint8_t kDashboardFonts[] asm("_binary_dashboard_fonts_bin_start");
namespace {
constexpr const char* kTag="word-machine-v2";
struct Input { wm::Button button; wm::Gesture gesture; uint32_t epoch; };
std::atomic<bool> accepting{false};
std::atomic<uint32_t> epoch{0};
QueueHandle_t input_queue;
TaskHandle_t key_task=nullptr;
std::atomic<uint32_t> key_scan_count{0},key_edge_count{0},key_sent_count{0},key_drop_count{0};
std::atomic<unsigned> key_levels{7};
constexpr const char* kDiagnosticBuild="HOME-INPUT-DIAG-2";

void ReadKeys(void*) {
    std::array<wm::Key,3> keys{};
    constexpr gpio_num_t pins[] = {ZECTRIX_BUTTON_UP, ZECTRIX_BUTTON_OK, ZECTRIX_BUTTON_DOWN};
    constexpr uint32_t holds[] = {0,700,3000};
    ESP_LOGI(kTag,"keyscan started stack_bytes=6144 GPIO39=UP GPIO0=OK GPIO18=DOWN");
    unsigned previous=8;
    for (;;) {
        const uint64_t now = esp_timer_get_time()/1000;
        unsigned levels=0;
        for(size_t i=0;i<3;++i)if(gpio_get_level(pins[i]))levels|=1U<<i;
        key_levels=levels;++key_scan_count;
        if(levels!=previous){
            if(previous!=8)++key_edge_count;
            ESP_LOGI(kTag,"gpio up=%u ok=%u down=%u accepting=%d epoch=%lu",
                levels&1U,(levels>>1)&1U,(levels>>2)&1U,accepting.load(),
                static_cast<unsigned long>(epoch.load()));
            previous=levels;
        }
        if (!accepting.load()) {
            for (auto& key : keys) key.Reset();
        } else {
            const uint32_t observed = epoch.load();
            for (size_t i=0;i<keys.size();++i) {
                const auto gesture=keys[i].Sample((levels&(1U<<i))==0,now,holds[i]);
                if (gesture!=wm::Gesture::None && accepting.load() && observed==epoch.load()) {
                    Input event{static_cast<wm::Button>(i),gesture,observed};
                    const bool sent=xQueueSend(input_queue,&event,0)==pdTRUE;
                    if(sent)++key_sent_count;else ++key_drop_count;
                    ESP_LOGI(kTag,"key-event button=%u gesture=%u queued=%d",
                        static_cast<unsigned>(event.button),static_cast<unsigned>(event.gesture),sent);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

class NvsStore final:public wm::BlobStore {
public:
    nvs_handle_t handle=0;
    wm::ReadResult Read(const std::string& key,std::vector<uint8_t>& bytes) override {
        size_t size=0;auto err=nvs_get_blob(handle,key.c_str(),nullptr,&size);
        if(err==ESP_ERR_NVS_NOT_FOUND)return wm::ReadResult::Missing;
        if(err!=ESP_OK||size>wm::kSnapshotChunk)return wm::ReadResult::Error;
        bytes.resize(size);err=nvs_get_blob(handle,key.c_str(),bytes.data(),&size);
        return err==ESP_OK?wm::ReadResult::Ok:wm::ReadResult::Error;
    }
    bool Write(const std::string& key,const std::vector<uint8_t>& bytes) override {
        return nvs_set_blob(handle,key.c_str(),bytes.data(),bytes.size())==ESP_OK;
    }
    bool Remove(const std::string& key) override {
        const auto err=nvs_erase_key(handle,key.c_str());return err==ESP_OK||err==ESP_ERR_NVS_NOT_FOUND;
    }
    bool Sync() override {return nvs_commit(handle)==ESP_OK;}
};
class App {
public:
    App():model_(DeviceModel()),ui_(kGlyphs,kDashboardFonts) {}
    void Run(){
        setenv("TZ","CST-8",1);tzset();
        ESP_LOGI(kTag,"build=%s reset_reason=%u main_stack_free=%u",kDiagnosticBuild,
            static_cast<unsigned>(esp_reset_reason()),static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
        ESP_ERROR_CHECK(board_.Init());
        const auto init=nvs_flash_init();
        if(init!=ESP_OK){ESP_LOGE(kTag,"NVS init failed; preserve Flash: %s",esp_err_to_name(init));return;}
        ESP_ERROR_CHECK(nvs_open("word_machine",NVS_READWRITE,&nvs_));
        const auto progress_init=nvs_flash_init_partition("wm_progress");
        if(progress_init!=ESP_OK){ESP_LOGE(kTag,"Progress partition invalid; preserve data");return;}
        ESP_ERROR_CHECK(nvs_open_from_partition("wm_progress","progress",NVS_READWRITE,&store_.handle));
        bool storage_ok=Load();
        zectrix_epd_config_t config;zectrix_epd_get_default_config(&config);
        ESP_ERROR_CHECK(zectrix_epd_new(&config,&epd_));ESP_ERROR_CHECK(zectrix_epd_power_on(epd_));
        input_queue=xQueueCreate(8,sizeof(Input));
        if(!input_queue||xTaskCreate(ReadKeys,"wm_keys",6144,nullptr,5,&key_task)!=pdPASS){
            ESP_LOGE(kTag,"Failed to start key task or queue");return;}
        ESP_LOGI(kTag,"keyscan created handle=%p",static_cast<void*>(key_task));
        ProvisionClock();
        model_.ReturnDashboard();
        ESP_LOGI(kTag,"Home recovery build: input defaults to FULL refresh; storage_ok=%d",storage_ok);
        Show(storage_ok?boot_notice_:"存储损坏，停用学习请保留数据");
        for(;;){
            Input event;
            if(xQueueReceive(input_queue,&event,pdMS_TO_TICKS(100))!=pdTRUE){
                TraceBoardButtons();
                const int64_t ms=esp_timer_get_time()/1000;
                if(ms-last_heartbeat_ms_>=5000){
                    last_heartbeat_ms_=ms;
                    const auto levels=key_levels.load();
                    ESP_LOGI(kTag,"heartbeat build=%s page=%u accept=%d scans=%lu edges=%lu sent=%lu drops=%lu gpio=%u%u%u main_free=%u key_free=%u heap=%u",
                        kDiagnosticBuild,static_cast<unsigned>(model_.session.page),accepting.load(),
                        static_cast<unsigned long>(key_scan_count.load()),static_cast<unsigned long>(key_edge_count.load()),
                        static_cast<unsigned long>(key_sent_count.load()),static_cast<unsigned long>(key_drop_count.load()),
                        levels&1U,(levels>>1)&1U,(levels>>2)&1U,
                        static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)),
                        static_cast<unsigned>(uxTaskGetStackHighWaterMark(key_task)),
                        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
                }
                if(navigation_dirty_ && ms-last_navigation_ms_>=1500){
                    auto candidate=model_;
                    if(Save(candidate)){model_=std::move(candidate);navigation_dirty_=false;}
                    else{last_navigation_ms_=ms;Show("保存失败，请重试");}
                }
                if(model_.session.page==wm::Page::Dashboard && ms-last_clock_poll_ms_>=1000){
                    last_clock_poll_ms_=ms;const auto now=Now();
                    if(now/60!=displayed_minute_)Show("",true);
                }
                continue;
            }
            TraceBoardButtons();
            ESP_LOGI(kTag,"input button=%u gesture=%u page=%u epoch=%lu/%lu storage_ok=%d",
                static_cast<unsigned>(event.button),static_cast<unsigned>(event.gesture),
                static_cast<unsigned>(model_.session.page),static_cast<unsigned long>(event.epoch),
                static_cast<unsigned long>(epoch.load()),storage_ok);
            if(event.epoch!=epoch.load()){ESP_LOGW(kTag,"Ignoring stale input");continue;}
            if(event.gesture==wm::Gesture::Hold && event.button==wm::Button::Down){Shutdown();continue;}
            if(!storage_ok){Show("存储损坏，停用学习请保留数据");continue;}
            if(!layout_ok_ && model_.session.page==wm::Page::Card && model_.session.back &&
               event.gesture!=wm::Gesture::Hold){Show("释义过长，暂不评分请返回");continue;}
            if(model_.session.page==wm::Page::Card&&model_.session.back&&
                event.gesture==wm::Gesture::Click&&answer_page_+1<ui_.answer_pages){
                if(wm::AnswerInput(answer_page_,ui_.answer_pages,event.button)==wm::AnswerAction::Next){++answer_page_;Show();continue;}
                if(wm::AnswerInput(answer_page_,ui_.answer_pages,event.button)==wm::AnswerAction::Previous){if(answer_page_)--answer_page_;Show();continue;}
                // Skip a long answer without claiming to have read/graded it.
                auto skipped=model_;skipped.session.back=false;
                if(skipped.Input(wm::Button::Down,false,Now())&&Save(skipped)){
                    model_=std::move(skipped);answer_page_=0;Show();
                }else Show("保存失败，请重试");
                continue;
            }
            const bool moving=event.gesture==wm::Gesture::Click&&event.button!=wm::Button::Ok&&
                (model_.session.page==wm::Page::Home||model_.session.page==wm::Page::Books||
                 model_.session.page==wm::Page::Units||model_.session.page==wm::Page::Options);
            if(moving){
                model_.Input(event.button,false,Now());navigation_dirty_=true;
                last_navigation_ms_=esp_timer_get_time()/1000;Show();continue;
            }
            ESP_LOGI(kTag,"action begin page=%u copy_start stack_free=%u",
                static_cast<unsigned>(model_.session.page),static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
            auto next=model_;
            ESP_LOGI(kTag,"action copy_ready");
            if(!next.Input(event.button,event.gesture==wm::Gesture::Hold,Now())){
                Show(Now()<0?"时间无效，需校时勿评分":"请重试");continue;
            }
            ESP_LOGI(kTag,"action target_page=%u",static_cast<unsigned>(next.session.page));
            if(Save(next)){model_=std::move(next);navigation_dirty_=false;answer_page_=0;Show();}
            else {ESP_LOGE(kTag,"Action not applied: persistence failure");Show("保存失败，请重试");}
        }
    }
private:
    void TraceBoardButtons(){
        ZectrixButtonEvent event;
        // The original board scanner is an independent observation only.
        // Bound draining to avoid a faulty producer starving the main loop.
        for(unsigned n=0;n<16&&board_.WaitButton(&event,0);++n)
            ESP_LOGI(kTag,"board-observer button=%u action=%u",
                static_cast<unsigned>(event.button),static_cast<unsigned>(event.action));
    }
    bool Save(wm::Offline& next){
        next.generation=model_.generation+1;
        const int64_t started=esp_timer_get_time();
        ESP_LOGI(kTag,"save begin generation=%llu",static_cast<unsigned long long>(next.generation));
        const auto bytes=next.Encode();
        if(!wm::WriteBank(store_,next.generation%2,bytes)){
            ESP_LOGE(kTag,"Chunked save failed; keep previous generation");return false;}
        ESP_LOGI(kTag,"save generation=%llu bytes=%u elapsed_ms=%lld",
            static_cast<unsigned long long>(next.generation),static_cast<unsigned>(bytes.size()),
            static_cast<long long>((esp_timer_get_time()-started)/1000));
        return true;
    }
    bool Load(){
        bool any=false,found=false,corrupt=false;
        for(unsigned bank=0;bank<2;++bank){
            std::vector<uint8_t> bytes;const auto result=wm::ReadBank(store_,bank,bytes);
            if(result==wm::ReadResult::Missing)continue;
            any=true;auto candidate=DeviceModel();
            if(result!=wm::ReadResult::Ok||!candidate.Decode(bytes)){corrupt=true;continue;}
            if(!found||candidate.generation>model_.generation){model_=std::move(candidate);found=true;}
        }
        if(any){
            if(!found)return false;
            if(corrupt)boot_notice_="恢复旧版进度，请核对";
            else if(model_.content_changed)boot_notice_="词库有变更，会话已重建";
            return true;
        }
        // No committed V3 bank: migrate original V2 snapshots from the old NVS partition.

        for(const char* key:{"state0","state1"}){
            size_t size=0;auto err=nvs_get_blob(nvs_,key,nullptr,&size);
            if(err==ESP_ERR_NVS_NOT_FOUND)continue;
            any=true;
            if(err!=ESP_OK||size>65536||size<48){corrupt=true;continue;}
            std::vector<uint8_t> bytes(size);
            err=nvs_get_blob(nvs_,key,bytes.data(),&size);
            auto candidate=DeviceModel();
            if(err!=ESP_OK||!candidate.Decode(bytes)){corrupt=true;continue;}
            if(!found||candidate.generation>model_.generation){model_=std::move(candidate);found=true;}
        }
        if(any&&!found)return false;
        if(found){
            if(corrupt)boot_notice_="恢复旧版进度，请核对";
            else if(model_.content_changed)boot_notice_="词库有变更，会话已重建";
            auto migrated=model_;
            if(!Save(migrated))return false;
            model_=std::move(migrated);return true;
        }
        // Import the subset of v1 records which could actually have been saved.
        // Long keys were never valid; ambiguous every_day records are not guessed.
        struct LegacyRecord{uint32_t version;char id[32];wm::Progress progress;};
        unsigned migrated=0;
        for(size_t i=0;i<assets::kCount;++i){
            const auto& id=model_.catalog[i].id;
            if(id.size()>15||id=="pep_en_every_day")continue;
            LegacyRecord r{};size_t bytes=sizeof(r);
            const auto err=nvs_get_blob(nvs_,id.c_str(),&r,&bytes);
            if(err==ESP_ERR_NVS_NOT_FOUND)continue;
            if(err!=ESP_OK||bytes!=sizeof(r)||r.version!=1||
               std::memcmp(r.id,id.c_str(),id.size()+1)!=0||r.progress.stage>5||
               r.progress.due_at<0||r.progress.last_review_at<0)return false;
            model_.rows[i].progress=r.progress;model_.rows[i].wrong=r.progress.lapses>0;++migrated;
        }
        if(migrated)boot_notice_="恢复旧版：今日评分待核实";
        auto initial=model_;
        if(!Save(initial))return false;
        model_=std::move(initial);return true;
    }
    void ProvisionClock() {
        if (Now()>=0 || !board_.rtc() || std::strlen(CONFIG_WM_INITIAL_LOCAL_TIME)==0) return;
        int y,m,d,h,minute,s;char extra;
        if (std::sscanf(CONFIG_WM_INITIAL_LOCAL_TIME,"%d-%d-%d %d:%d:%d%c",&y,&m,&d,&h,&minute,&s,&extra)!=6) return;
        if (y<2024 || y>2099 || m<1 || m>12 || d<1 || d>31 || h<0 || h>23 || minute<0 || minute>59 || s<0 || s>59) return;
        tm local{};local.tm_year=y-1900;local.tm_mon=m-1;local.tm_mday=d;
        local.tm_hour=h;local.tm_min=minute;local.tm_sec=s;
        if (mktime(&local)<0 || local.tm_year!=y-1900 || local.tm_mon!=m-1 || local.tm_mday!=d) return;
        if (!board_.rtc()->SetTime(local)) ESP_LOGE(kTag,"RTC provisioning failed");
    }
    int64_t Now() {
        tm local{};
        if (!board_.rtc() || !board_.rtc()->GetTime(local) || local.tm_year<124 || local.tm_year>199) return -1;
        const time_t value=mktime(&local);
        if (value<0) return -1;
        return model_.Now(value);
    }

    void Show(const std::string& message="",bool clock_only=false){
        const int64_t started=esp_timer_get_time();
        accepting=false;++epoch;
        const int64_t now=Now();
        std::string notice=message;
        if(notice.empty()&&now<0)notice="时间无效，需校时勿评分";
        layout_ok_=ui_.Draw(model_,now,notice,answer_page_);
        auto update=wm::PlanFrame(previous_frame_,ui_.frame,display_base_,partial_count_,clock_only);
        esp_err_t err=ESP_OK;
        if(update.width){
            if(update.full)err=zectrix_epd_refresh_full_1bpp(epd_,ui_.frame.data(),ui_.frame.size());
            else{
                zectrix_epd_rect_t rect={update.x,update.y,update.width,update.height};
                err=zectrix_epd_refresh_partial_1bpp(epd_,&rect,update.pixels.data(),update.pixels.size());
                // Recover a partial-refresh error by re-establishing a full base.
                if(err!=ESP_OK){update.full=true;
                    err=zectrix_epd_refresh_full_1bpp(epd_,ui_.frame.data(),ui_.frame.size());}
            }
        }
        if(err!=ESP_OK){ESP_LOGE(kTag,"Display failed: %s; grading disabled",esp_err_to_name(err));
            for(;;)vTaskDelay(pdMS_TO_TICKS(1000));}
        if(update.width){previous_frame_=ui_.frame;display_base_=true;
            partial_count_=update.full?0:partial_count_+1;}
        displayed_minute_=now/60;
        ESP_LOGI(kTag,"screen=%u refresh=%s elapsed_ms=%lld",
            static_cast<unsigned>(model_.session.page),!update.width?"none":update.full?"full":"partial",
            static_cast<long long>((esp_timer_get_time()-started)/1000));
        Input ignored;while(xQueueReceive(input_queue,&ignored,0)==pdTRUE){}
        accepting=true;
    }
    void Shutdown(){
        if(navigation_dirty_){auto next=model_;if(!Save(next)){Show("保存失败，请重试");return;}model_=std::move(next);}
        accepting=false;++epoch;
        // Leave the final screen intact after every accepted learning action is committed.
        zectrix_epd_power_off(epd_);nvs_close(nvs_);nvs_close(store_.handle);
        board_.SetAudioPower(false);board_.SetPowerLed(false);board_.CutBatteryPower();
        vTaskDelay(pdMS_TO_TICKS(100));esp_deep_sleep_start();
        for(;;)vTaskDelay(portMAX_DELAY);
    }
    ZectrixBoard board_;
    wm::Offline model_;
    RuntimeUi ui_;
    zectrix_epd_handle_t epd_=nullptr;
    nvs_handle_t nvs_=0;
    NvsStore store_;
    bool layout_ok_=true;
    std::array<uint8_t,15000> previous_frame_{};
    bool display_base_=false,navigation_dirty_=false;
    unsigned partial_count_=0,answer_page_=0;
    int64_t last_heartbeat_ms_=0,last_navigation_ms_=0,last_clock_poll_ms_=0,displayed_minute_=-2;
    std::string boot_notice_;
};
}
extern "C" void app_main(){static App app;app.Run();}

#include "esp_test_platform.h"
#include "services/storage_service.h"
#include "services/blob_journal.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {
using Bytes=std::vector<uint8_t>;
using Space=std::map<std::string,Bytes>;
std::map<std::string,Space> persisted;
struct Handle { std::string ns; nvs_open_mode_t mode; Space staged; };
std::map<nvs_handle_t,Handle> handles;
unsigned next_handle=1, opens_rw=0, commits=0, sets=0, failed=0, passed=0;
unsigned fail_commit=0;
bool failed_commit_persists=false;
size_t free_entries=261;
esp_err_t init_result=ESP_OK;
esp_ota_img_states_t image_state=ESP_OTA_IMG_PENDING_VERIFY;
esp_partition_t partition{};
void check(bool value,const char *label) {
    std::printf("%s %s\n",value?"PASS":"FAIL",label);
    value?++passed:++failed;
}
class Seed final: public persistent::Backend {
public:
    explicit Seed(const char *ns):ns_(ns){}
    persistent::Read get(const char *key,Bytes &out) override {
        auto i=persisted[ns_].find(key);
        if (i==persisted[ns_].end()) return persistent::Read::Missing;
        out=i->second; return persistent::Read::Ok;
    }
    bool put(const char *key,const Bytes &bytes) override { persisted[ns_][key]=bytes; return true; }
private: std::string ns_;
};
}
esp_err_t nvs_flash_init() { return init_result; }
esp_err_t nvs_open(const char *ns,nvs_open_mode_t mode,nvs_handle_t *out) {
    if (mode==NVS_READONLY && !persisted.count(ns)) return ESP_ERR_NVS_NOT_FOUND;
    if (mode==NVS_READWRITE) ++opens_rw;
    *out=next_handle++; handles[*out]={ns,mode,{}}; return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *n) {
    const auto &space=persisted.at(handles.at(h).ns);
    auto i=space.find(key); if(i==space.end()) return ESP_ERR_NVS_NOT_FOUND;
    if(out) { if(*n<i->second.size()) return ESP_FAIL; std::memcpy(out,i->second.data(),i->second.size()); }
    *n=i->second.size(); return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t n) {
    auto &handle=handles.at(h); if(handle.mode!=NVS_READWRITE) return ESP_FAIL;
    ++sets; const auto p=static_cast<const uint8_t *>(data); handle.staged[key]=Bytes(p,p+n); return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h) {
    ++commits;
    const bool fail=commits==fail_commit;
    if(fail && !failed_commit_persists) return ESP_FAIL;
    auto &handle=handles.at(h);
    for(const auto &item:handle.staged) persisted[handle.ns][item.first]=item.second;
    handle.staged.clear(); return fail?ESP_FAIL:ESP_OK;
}
void nvs_close(nvs_handle_t h) { handles.erase(h); }
esp_err_t nvs_get_stats(const char *,nvs_stats_t *out) { *out={495,free_entries,756,persisted.size()}; return ESP_OK; }
const esp_partition_t *esp_ota_get_running_partition() { return &partition; }
esp_err_t esp_ota_get_state_partition(const esp_partition_t *,esp_ota_img_states_t *out) { *out=image_state; return ESP_OK; }
const char *esp_err_to_name(esp_err_t) { return "injected NVS error"; }

int main(int argc,char **argv) {
    if(argc!=2) return 2;
    const std::string scenario=argv[1];
    uint32_t original=123,next=456,observed=0;
    persisted["settings"]["legacy_value"]={1,2,3,4};
    const auto legacy=persisted["settings"];
    Seed seed("settings_v3"); persistent::Journal journal(seed);
    check(journal.save(3,&original,sizeof(original)),"Prior accepted image record is seeded with the production journal format");
    const auto before=persisted;
    if(scenario=="no-free-pages" || scenario=="new-nvs-version") {
        init_result=scenario=="no-free-pages"?ESP_ERR_NVS_NO_FREE_PAGES:ESP_ERR_NVS_NEW_VERSION_FOUND;
        check(!storage_init() && !storage_ready() && !storage_writes_allowed(),"NVS initialization failure disables readiness and all writes");
        check(!storage_init() && !storage_write_blob("settings_v3",3,&next,sizeof(next)),"Retries cannot turn an unrecovered initialization failure into a successful save");
        check(persisted==before && opens_rw==0 && sets==0 && commits==0,"No partition erase or NVS mutation occurs on initialization failure");
        check(std::strstr(storage_status_message(),"preserved")!=nullptr,"Recovery status states that existing data is preserved");
    } else if(scenario=="pending-rollback") {
        check(storage_init() && storage_ready() && !storage_writes_allowed(),"Real ESP storage initializer enters read-only pending-OTA probation");
        check(storage_read_blob("settings_v3",3,&observed,sizeof(observed))==STORAGE_OK && observed==original,"Pending image reads the prior complete saved record");
        check(!storage_write_blob("settings_v3",3,&next,sizeof(next)) && !storage_run_write([]{return true;}),"Every write entry point refuses a save before boot acceptance");
        check(persisted==before && opens_rw==0 && sets==0 && commits==0,"Failed probation leaves byte-identical storage for bootloader rollback");
        // Read as the prior namespace/schema. This is storage compatibility,
        // not an execution of the physical ESP bootloader or old firmware.
        Seed prior("settings_v3"); persistent::Journal old(prior);
        check(old.load(3,&observed,sizeof(observed))==persistent::Read::Ok && observed==original && persisted["settings"]==legacy,"Prior namespace/schema still reads its original value after rejected probation");
    } else if(scenario=="accepted-upgrade") {
        check(storage_init() && !storage_writes_allowed(),"Accepted-upgrade scenario begins read-only");
        storage_accept_boot();
        check(storage_writes_allowed() && storage_write_blob("settings_v4",4,&next,sizeof(next)),"Explicit health acceptance permits a new versioned namespace save");
        check(storage_read_blob("settings_v3",3,&observed,sizeof(observed))==STORAGE_OK && observed==original && persisted["settings"]==legacy,"New namespace save preserves both prior journal and retained legacy keys");
        const auto checkpoint=persisted; const unsigned prior_commits=commits;
        check(!storage_write_blob("settings_v3",4,&next,sizeof(next)) && persisted==checkpoint && commits==prior_commits,"An unknown schema cannot overwrite a prior journal in place");
        free_entries=128;
        check(!storage_write_blob("settings_v4",4,&original,sizeof(original)) && persisted==checkpoint && commits==prior_commits,"Actual ESP capacity guard rejects a save without touching the prior record");
        free_entries=261;
        fail_commit=commits+2; // inactive write succeeds, final selector commit fails
        check(!storage_write_blob("settings_v3",3,&next,sizeof(next)),"NVS selector commit error is returned by the production storage branch");
        check(storage_read_blob("settings_v3",3,&observed,sizeof(observed))==STORAGE_OK && observed==original,"A failed selector commit retains the old active record despite a new inactive blob");
        fail_commit=0;
        check(storage_write_blob("settings_v3",3,&next,sizeof(next)) && storage_read_blob("settings_v3",3,&observed,sizeof(observed))==STORAGE_OK && observed==next,"Retry atomically publishes a complete replacement after selector failure");
        // A real flash operation can persist before an error/acknowledgement is
        // observed. Exercise that distinct outcome as well; never promise that
        // every reported failure means the selector stayed physically unchanged.
        failed_commit_persists=true; fail_commit=commits+2;
        check(!storage_write_blob("settings_v3",3,&original,sizeof(original)),"Lost selector-commit acknowledgement still reports save failure");
        check(storage_read_blob("settings_v3",3,&observed,sizeof(observed))==STORAGE_OK && observed==original,"A commit that persisted before its error leaves the complete new record readable");
    } else if(scenario=="valid-image") {
        image_state=ESP_OTA_IMG_VALID;
        check(storage_init() && storage_writes_allowed(),"An already accepted image does not reenter read-only OTA probation");
        check(storage_write_blob("settings_v3",3,&next,sizeof(next)),"Already accepted image can save normal state");
    } else return 2;
    check(handles.empty(),"All production NVS handles close on success and failure paths");
    std::printf("Results: %u passed, %u failed\n",passed,failed);
    return failed?1:0;
}

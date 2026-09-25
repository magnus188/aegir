#include "services/ota_core.h"
#include "services/c6_recovery_checks.h"
#include "services/blob_journal.h"
#include "services/storage_service.h"
#include "services/maintenance_service.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

namespace {
int passed = 0, failed = 0;
void check(bool ok, const char *label) { std::printf("%s %s\n", ok ? "PASS" : "FAIL", label); ok ? ++passed : ++failed; }
std::string release(const char *family = "pre3", const char *tag = "v0.3.0", const char *digest = "sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef") {
    const std::string name = std::string("Trimix_analyzer_esp32p4_") + family + "_v0.3.0.bin";
    return std::string("{\"tag_name\":\"") + tag + "\",\"body\":\"Changes\",\"assets\":[{\"name\":\"" + name +
        "\",\"browser_download_url\":\"https://github.com/example/trimix/releases/download/" + tag + "/" + name +
        "\",\"size\":4096,\"digest\":\"" + digest + "\"}]}";
}
bool parse(const std::string &s, ota::Release &r, uint32_t slot = 4194304, ota::Family family = ota::Family::Pre3) {
    std::string error;
    return ota::parse_release(s.data(), s.size(), family, "0.2.0", "https://github.com/example/trimix", slot, r, error);
}
class Memory : public persistent::Backend {
public:
    std::map<std::string, std::vector<uint8_t>> data;
    int operations = 0, fail_at = -1;
    persistent::Read get(const char *key, std::vector<uint8_t> &v) override {
        if (operations++ == fail_at) return persistent::Read::Error;
        auto i = data.find(key); if (i == data.end()) return persistent::Read::Missing;
        v = i->second; return persistent::Read::Ok;
    }
    bool put(const char *key, const std::vector<uint8_t> &v) override {
        if (operations++ == fail_at) return false;
        data[key] = v; return true;
    }
};
class FlashModel : public ota::ImageTransport {
public:
    enum Failure { None, Begin, Identity, Transfer, Digest, Activation } failure = None;
    uint32_t clock = 0, bytes = 0;
    unsigned active_image = 1, writes = 0, activations = 0, aborts = 0;
    bool cancel = false, complete_data = true, oversized = false, cancel_after_digest = false;
    bool begin() override { return failure != Begin; }
    bool identity(ota::ImageIdentity &out) override {
        std::snprintf(out.project, sizeof(out.project), "%s", failure == Identity ? "wrong" : "Trimix_analyzer");
        std::snprintf(out.version, sizeof(out.version), "0.3.0"); return true;
    }
    ota::TransferStep transfer() override {
        clock += 1000; ++writes;
        if (failure == Transfer && writes == 2) return ota::TransferStep::Error;
        bytes += oversized ? 8192 : 1024;
        return bytes >= 4096 ? ota::TransferStep::Complete : ota::TransferStep::More;
    }
    uint32_t received() override { return bytes; }
    bool complete() override { return complete_data; }
    bool digest_matches() override { if (cancel_after_digest) cancel = true; return failure != Digest; }
    bool activate() override { ++activations; if (failure == Activation) return false; active_image = 2; return true; }
    void abort() override { ++aborts; }
    bool cancelled() override { return cancel; }
    uint32_t now_ms() override { return clock; }
};
int stops = 0, resumes = 0, flushes = 0;
bool power = true, stop_ok = true, flush_ok = true;
bool stop(uint32_t ms) { ++stops; return ms && stop_ok; }
bool flush(uint32_t ms) { ++flushes; return ms && flush_ok; }
void resume() { ++resumes; }
bool allowed() { return power; }
}
int main() {
    ota::BootProbation probation;
    probation.begin(100, true);
    check(probation.pending() && !probation.expire(60099), "Pending OTA startup remains inside its 60-second total deadline");
    check(probation.complete(60099) == ota::BootProbation::Completion::Timely && !probation.expire(60100),
          "Successful startup completion prevents a later supervisor rollback");
    probation.begin(100, true);
    check(probation.expire(60100) && !probation.pending(), "Supervisor expires essential startup even before the post-init health loop");
    check(probation.complete(60101) == ota::BootProbation::Completion::AlreadyExpired && !probation.expire(60102),
          "Expired startup cannot subsequently accept itself or repeat rollback ownership");
    probation.begin(100, true);
    check(probation.complete(60100) == ota::BootProbation::Completion::Timeout && !probation.expire(60101),
          "Main task arriving at the deadline owns timeout once even if supervisor has not run yet");
    probation.begin(UINT32_MAX - 100, true);
    check(!probation.expire(59898) && probation.expire(59899), "Startup deadline remains correct across millisecond clock wrap");
    probation.begin(0, false);
    check(!probation.pending() && !probation.expire(60000) && probation.complete(60001) == ota::BootProbation::Completion::Timely,
          "An already accepted image has no OTA probation rollback deadline");
    uint32_t numbers[3];
    check(ota::version("v12.34.56", numbers) && numbers[1] == 34, "Semantic release version parsed");
    for (const char *bad : {"", "1.2", "1.2.3beta", "-1.2.3", "999999999999.2.3", "1.2.3.4", "1..3"})
        check(!ota::version(bad, numbers), "Malformed/overflow version rejected");
    ota::Response response;
    check(response.append("abc", 3) && response.append("def", 3) && response.body() == "abcdef", "Chunked response fragments retained");
    std::string large(32769, 'x');
    check(!response.append(large.data(), large.size()) && response.overflowed(), "Oversize response fails instead of silently truncating");
    response.clear(); check(response.body().empty() && !response.overflowed(), "Retry starts with a clean response");
    ota::Release output;
    check(parse(release(), output) && output.newer && output.size == 4096 && output.sha256[0] == 1, "Compatible exact application asset and SHA selected");
    check(parse(release("v3"), output, 6291456, ota::Family::V3), "V3 family selects its own application image");
    check(!parse(release("v3"), output), "Wrong silicon family rejected");
    check(!parse(release("pre3_factory"), output), "Factory binary is never an OTA candidate");
    check(!parse(release(), output, 1024), "Installed slot size limits update");
    check(!parse(release("pre3", "v0.3.0", ""), output), "Missing release digest rejected");
    auto malformed = release(); malformed.replace(malformed.find("4096"), 4, "-1");
    check(!parse(malformed, output), "Negative firmware size rejected");
    malformed = release(); malformed.replace(malformed.find("https://github.com/example/trimix/releases"), 5, "http");
    check(!parse(malformed, output), "Non-HTTPS or mismatched asset URL rejected");
    malformed = release(); auto a = malformed.find("[{"), b = malformed.rfind("}]");
    const auto asset = malformed.substr(a + 1, b - a); malformed.insert(b + 1, "," + asset);
    check(!parse(malformed, output), "Duplicate matching assets rejected");
    malformed = release(); malformed.insert(malformed.find("Changes"), 6000, 'a');
    check(parse(malformed, output) && output.notes.size() == 255, "Large valid metadata accepted with bounded display notes");
    malformed.push_back('x'); check(!parse(malformed, output), "Trailing malformed JSON rejected");
    check(ota::image_identity("Trimix_analyzer", "0.3.0", "Trimix_analyzer", "0.3.0") &&
          !ota::image_identity("other", "0.3.0", "Trimix_analyzer", "0.3.0"), "Downloaded descriptor must match project and release version");
    check(!ota::boot_healthy(3, true) && !ota::boot_healthy(9, false) && ota::boot_healthy(4, true), "Boot acceptance requires UI heartbeat and responsive worker");

    check(c6_recovery_descriptor_valid(2097152, 0x000d, "network_adapter", 16, "2.12.9", 7),
          "Pinned C6 recovery descriptor accepted");
    check(!c6_recovery_descriptor_valid(1000, 0x000d, "network_adapter", 16, "2.12.9", 7) &&
          !c6_recovery_descriptor_valid(2097152, 0x0012, "network_adapter", 16, "2.12.9", 7),
          "Small C6 partition and wrong-chip image rejected");
    const char unterminated[3] = {'a','b','c'};
    check(!c6_recovery_descriptor_valid(2097152, 0x000d, unterminated, sizeof(unterminated), "2.12.9", 7),
          "Damaged C6 metadata never reads beyond its field");
    check(!c6_recovery_needs_activation(2,5) && c6_recovery_needs_activation(2,6) && c6_recovery_needs_activation(3,0),
          "C6 activation protocol boundary preserved");

    ota::Release transfer_release; transfer_release.version = "0.3.0"; transfer_release.size = 4096;
    FlashModel download;
    check(ota::install(download, "Trimix_analyzer", transfer_release) == ota::InstallResult::Ok &&
          download.active_image == 2 && download.activations == 1 && !download.aborts,
          "Production installer activates only the complete verified application");
    for (auto fault : {FlashModel::Begin, FlashModel::Identity, FlashModel::Transfer, FlashModel::Digest, FlashModel::Activation}) {
        FlashModel broken; broken.failure = fault;
        check(ota::install(broken, "Trimix_analyzer", transfer_release) != ota::InstallResult::Ok &&
              broken.active_image == 1 && broken.aborts == 1 && (fault == FlashModel::Activation || broken.activations == 0),
              "Injected transport/metadata/write/digest/activation failure retains previous image");
    }
    FlashModel cancelled; cancelled.cancel = true;
    check(ota::install(cancelled, "Trimix_analyzer", transfer_release) == ota::InstallResult::Cancelled && !cancelled.writes &&
          cancelled.active_image == 1, "Cancellation before beginning never writes flash");
    cancelled = {}; cancelled.cancel_after_digest = true;
    check(ota::install(cancelled, "Trimix_analyzer", transfer_release) == ota::InstallResult::Cancelled && !cancelled.activations,
          "Cancellation after verification still prevents activation");
    FlashModel incomplete; incomplete.complete_data = false;
    check(ota::install(incomplete, "Trimix_analyzer", transfer_release) == ota::InstallResult::Truncated && !incomplete.activations,
          "Incomplete transfer cannot activate despite matching byte count");
    FlashModel excessive; excessive.oversized = true;
    check(ota::install(excessive, "Trimix_analyzer", transfer_release) == ota::InstallResult::Truncated && !excessive.activations,
          "Transfer exceeding declared image size aborts immediately");
    FlashModel timed; timed.clock = UINT32_MAX - 1000;
    check(ota::install(timed, "Trimix_analyzer", transfer_release, 1500) == ota::InstallResult::Timeout && !timed.activations,
          "Installer time limit survives millisecond clock wrap");

    check(!ota::boot_healthy(4, true, false), "Unavailable/incompatible NVS blocks new image acceptance");

    Memory memory; persistent::Journal journal(memory);
    uint32_t old = 123, next = 456, readback = 999;
    check(journal.load(1, &readback, sizeof(readback)) == persistent::Read::Missing && readback == 999, "Missing storage preserves destination");
    check(journal.save(1, &old, sizeof(old)) && journal.load(1, &readback, sizeof(readback)) == persistent::Read::Ok && readback == old, "Complete versioned CRC record round trips");
    const auto snapshot = memory.data;
    for (int fail_at = 0; fail_at < 7; ++fail_at) {
        Memory failure; failure.data = snapshot; failure.fail_at = fail_at;
        persistent::Journal interrupted(failure); const bool saved = interrupted.save(1, &next, sizeof(next));
        failure.fail_at = -1; uint32_t loaded = 0;
        const auto loaded_result = interrupted.load(1, &loaded, sizeof(loaded));
        // If selector commit completed but its readback failed, either fully committed version is valid.
        check(loaded_result == persistent::Read::Ok && (loaded == old || loaded == next) && (!saved || loaded == next),
              "Interruption at each journal operation leaves a complete committed record");
    }
    readback = 999;
    check(journal.load(2, &readback, sizeof(readback)) == persistent::Read::Error && readback == 999,
          "Incompatible schema does not overwrite destination or stored state");
    memory.data["a"].back() ^= 1;
    check(journal.load(1, &readback, sizeof(readback)) == persistent::Read::Error && !journal.save(1, &next, sizeof(next)),
          "Corrupt selected record is preserved for recovery, never replaced by defaults");
    check(storage_init() && storage_write_blob("test_v1", 1, &old, sizeof(old)), "Central storage initializes without destructive reset");
    check(storage_write_blob("test_v2", 2, &next, sizeof(next)) &&
          storage_read_blob("test_v1", 1, &readback, sizeof(readback)) == STORAGE_OK && readback == old,
          "New schema namespace preserves the prior firmware rollback record");
    maintenance_register_hooks({stop, flush, resume, allowed});
    power = false;
    check(!maintenance_begin(MAINTENANCE_OTA, 100) && stops == 0, "OTA does not stop or flash with insufficient power");
    power = true; stop_ok = false;
    check(!maintenance_begin(MAINTENANCE_OTA, 100) && resumes == 1 && !maintenance_is_active(), "Failed stop returns to normal operation");
    stop_ok = true; flush_ok = false;
    check(!maintenance_begin(MAINTENANCE_OTA, 100) && storage_writes_allowed(), "Failed flush prevents maintenance hold");
    flush_ok = true;
    check(maintenance_begin(MAINTENANCE_OTA, 100) && !storage_writes_allowed(), "Successful stop/flush holds persistence before OTA");
    check(!maintenance_begin(MAINTENANCE_SHUTDOWN, 100), "Concurrent maintenance request cannot steal ownership");
    check(!storage_write_blob("test_v1", 1, &next, sizeof(next)), "New writes rejected while maintenance holds storage");
    maintenance_finish(false);
    check(!maintenance_is_active() && storage_writes_allowed(), "Cancelled OTA resumes acquisition and storage");
    std::printf("%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}

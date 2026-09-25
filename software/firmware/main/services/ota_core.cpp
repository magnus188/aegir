#include "ota_core.h"
#include <cJSON.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
namespace ota {
bool Response::append(const char *bytes, size_t count) {
    if ((!bytes && count) || overflow_ || count > 32768 - body_.size()) { overflow_ = true; return false; }
    if (count) body_.append(bytes, count);
    return true;
}
bool version(const char *text, uint32_t (&parts)[3]) {
    if (!text) return false;
    if (*text == 'v') ++text;
    for (unsigned i = 0; i < 3; ++i) {
        if (*text < '0' || *text > '9') return false;
        uint32_t n = 0; unsigned digits = 0;
        do { n = n * 10 + (*text++ - '0'); if (++digits > 5 || n > 65535) return false; } while (*text >= '0' && *text <= '9');
        parts[i] = n;
        if (i < 2 && *text++ != '.') return false;
    }
    return *text == '\0';
}
namespace {
const char *str(const cJSON *o, const char *key) {
    const auto *v = cJSON_GetObjectItemCaseSensitive(o, key);
    return cJSON_IsString(v) ? v->valuestring : nullptr;
}
int hex(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; }
}
bool parse_release(const char *json, size_t size, Family family, const char *current,
                   const char *repository_url, uint32_t slot_size, Release &out, std::string &error) {
    error = "Invalid release metadata";
    if (!json || !repository_url || size > 32768 || !slot_size || std::memchr(json, 0, size)) return false;
    const std::string document(json, size);
    const char *end = nullptr;
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(cJSON_ParseWithOpts(document.c_str(), &end, true), cJSON_Delete);
    if (!root || !cJSON_IsObject(root.get())) return false;
    if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root.get(), "draft")) ||
        cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root.get(), "prerelease"))) return false;
    const char *tag = str(root.get(), "tag_name");
    uint32_t next[3], old[3];
    if (!version(tag, next) || !version(current, old)) return false;
    char canonical[32]; std::snprintf(canonical, sizeof(canonical), "%u.%u.%u", static_cast<unsigned>(next[0]), static_cast<unsigned>(next[1]), static_cast<unsigned>(next[2]));
    const std::string name = std::string(family == Family::Pre3 ? "Trimix_analyzer_esp32p4_pre3_v" : "Trimix_analyzer_esp32p4_v3_v") + canonical + ".bin";
    const auto *assets = cJSON_GetObjectItemCaseSensitive(root.get(), "assets");
    if (!cJSON_IsArray(assets)) return false;
    const cJSON *match = nullptr;
    const cJSON *asset = nullptr;
    cJSON_ArrayForEach(asset, assets) {
        const char *candidate = str(asset, "name");
        if (candidate && name == candidate) {
            if (match) { error = "Ambiguous firmware assets"; return false; }
            match = asset;
        }
    }
    if (!match) { error = "No application image for this P4 revision"; return false; }
    const char *url = str(match, "browser_download_url"), *digest = str(match, "digest");
    const auto *length = cJSON_GetObjectItemCaseSensitive(match, "size");
    const std::string expected_url = std::string(repository_url) + "/releases/download/" + tag + "/" + name;
    if (!url || std::strncmp(repository_url, "https://github.com/", 19) != 0 || expected_url != url || std::strlen(url) >= 256) {
        error = "Firmware URL does not match this release"; return false;
    }
    if (!cJSON_IsNumber(length) || !std::isfinite(length->valuedouble) || length->valuedouble < 256 ||
        length->valuedouble > slot_size || std::floor(length->valuedouble) != length->valuedouble) {
        error = "Firmware image does not fit the installed OTA slot"; return false;
    }
    Release candidate;
    if (!digest || std::strlen(digest) != 71 || std::strncmp(digest, "sha256:", 7)) {
        error = "Release lacks its SHA-256 digest; use a verified recovery image"; return false;
    }
    for (unsigned i = 0; i < 32; ++i) {
        const int hi = hex(digest[7 + i * 2]), lo = hex(digest[8 + i * 2]);
        if (hi < 0 || lo < 0) return false;
        candidate.sha256[i] = (hi << 4) | lo;
    }
    candidate.version = canonical; candidate.url = url; candidate.size = length->valuedouble;
    if (const char *notes = str(root.get(), "body")) candidate.notes.assign(notes, std::strlen(notes) > 255 ? 255 : std::strlen(notes));
    for (unsigned i = 0; i < 3; ++i) { if (next[i] != old[i]) { candidate.newer = next[i] > old[i]; break; } }
    out = candidate; error.clear(); return true;
}
bool image_identity(const char *project, const char *v, const char *expected_project, const char *expected_version) {
    return project && v && expected_project && expected_version && std::strcmp(project, expected_project) == 0 && std::strcmp(v, expected_version) == 0;
}
bool boot_healthy(unsigned heartbeats, bool worker, bool storage) { return heartbeats >= 4 && worker && storage; }
void BootProbation::begin(uint32_t now, bool required) { start_ = now; state_ = required ? Checking : Completed; }
BootProbation::Completion BootProbation::complete(uint32_t now) {
    State expected = Checking;
    const State result = now - start_ < kDeadlineMs ? Completed : Expired;
    if (state_.compare_exchange_strong(expected, result))
        return result == Completed ? Completion::Timely : Completion::Timeout;
    return expected == Completed ? Completion::Timely : Completion::AlreadyExpired;
}
bool BootProbation::expire(uint32_t now) {
    if (now - start_ < kDeadlineMs) return false;
    State expected = Checking;
    return state_.compare_exchange_strong(expected, Expired);
}
bool BootProbation::pending() const { return state_ == Checking; }
InstallResult install(ImageTransport &transport, const char *project, const Release &release, uint32_t timeout_ms) {
    const uint32_t start = transport.now_ms();
    auto stop = [&] {
        return transport.cancelled() ? InstallResult::Cancelled :
            transport.now_ms() - start >= timeout_ms ? InstallResult::Timeout : InstallResult::Ok;
    };
    auto fail = [&](InstallResult result) { transport.abort(); return result; };
    auto state = stop();
    if (state != InstallResult::Ok) return state;
    if (!project || !release.size || !transport.begin()) return fail(InstallResult::BeginFailed);
    ImageIdentity identity{};
    if (!transport.identity(identity) || !std::memchr(identity.project, 0, sizeof(identity.project)) ||
        !std::memchr(identity.version, 0, sizeof(identity.version)) ||
        !image_identity(identity.project, identity.version, project, release.version.c_str())) return fail(InstallResult::IdentityMismatch);
    for (;;) {
        state = stop(); if (state != InstallResult::Ok) return fail(state);
        const auto step = transport.transfer();
        if (step == TransferStep::Error) return fail(InstallResult::TransferFailed);
        if (transport.received() > release.size) return fail(InstallResult::Truncated);
        if (step == TransferStep::Complete) break;
    }
    state = stop(); if (state != InstallResult::Ok) return fail(state);
    if (!transport.complete() || transport.received() != release.size) return fail(InstallResult::Truncated);
    if (!transport.digest_matches()) return fail(InstallResult::DigestMismatch);
    state = stop(); if (state != InstallResult::Ok) return fail(state);
    if (!transport.activate()) return fail(InstallResult::ActivationFailed);
    return InstallResult::Ok;
}
const char *install_message(InstallResult result) {
    switch (result) {
        case InstallResult::Ok: return "Update complete";
        case InstallResult::Cancelled: return "Update cancelled; previous firmware retained";
        case InstallResult::BeginFailed: return "Could not open OTA transport/partition";
        case InstallResult::IdentityMismatch: return "Application identity/version does not match release";
        case InstallResult::TransferFailed: return "Download/write failed; previous firmware retained";
        case InstallResult::Truncated: return "Firmware length/completeness mismatch";
        case InstallResult::DigestMismatch: return "Firmware SHA-256 mismatch";
        case InstallResult::ActivationFailed: return "Firmware activation failed";
        case InstallResult::Timeout: return "Update exceeded its time limit";
    }
    return "Update failed";
}
}

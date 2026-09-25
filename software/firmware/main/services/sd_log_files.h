#pragma once
#include "sd_log_core.h"
#include <cstdio>
namespace sd_log {
class Volume {
public:
    virtual ~Volume() = default;
    virtual bool mount(uint64_t &capacity_bytes) = 0;
    virtual bool unmount() = 0;
    virtual const char *error() const = 0;
};
// Production POSIX/FatFs writer, also exercised against real temporary host files.
class Files final : public Backend {
public:
    Files(Volume &volume,const char *root,uint32_t boot_id,const char *firmware);
    bool mount(uint64_t &capacity_bytes) override;
    bool open(uint32_t session,Mode mode,char *path,size_t size) override;
    bool append(Kind kind,const char *row) override;
    bool flush() override;
    bool close() override;
    bool unmount() override;
    const char *error() const override { return error_; }
private:
    Volume &volume_;
    char root_[64],firmware_[48],error_[96]="SD unavailable";
    uint32_t boot_id_;
    FILE *files_[3]{};
    bool fail(const char *operation);
};
}

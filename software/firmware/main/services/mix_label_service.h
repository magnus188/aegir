#pragma once

#include "services/analysis_history.h"
#include "services/cylinder_profiles.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void mix_label_build_text(const analysis_history_record_t* record,
                          const cylinder_profile_t* profile,
                          char* out,
                          size_t out_size);

void mix_label_build_csv(const analysis_history_record_t* record,
                         const cylinder_profile_t* profile,
                         char* out,
                         size_t out_size);

#ifdef __cplusplus
}
#endif

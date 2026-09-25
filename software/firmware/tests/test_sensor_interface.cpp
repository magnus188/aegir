#include "sensors/sensor_interface.h"
#include "services/storage_service.h"
#include "services/gas_calibration_core.h"
#include "services/gas_calibration_internal.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

int tests_passed = 0;
int tests_failed = 0;

void expect_true(bool condition, const char* name) {
    if (condition) {
        std::printf("  PASS %s\n", name);
        ++tests_passed;
    } else {
        std::printf("  FAIL %s\n", name);
        ++tests_failed;
    }
}

void expect_near(float actual, float expected, float tolerance, const char* name) {
    if (std::fabs(actual - expected) <= tolerance) {
        std::printf("  PASS %s (%.2f ~= %.2f)\n", name, actual, expected);
        ++tests_passed;
    } else {
        std::printf("  FAIL %s (got %.2f, expected %.2f)\n", name, actual, expected);
        ++tests_failed;
    }
}

sensor_readings_t read_many(int count) {
    sensor_readings_t r = {};
    for (int i = 0; i < count; ++i) {
        sensor_read_all(&r);
    }
    return r;
}

}  // namespace

int main() {
    std::printf("Sensor Interface Tests\n");
    std::printf("======================\n\n");

    sensor_set_mock_profile(SENSOR_MOCK_PROFILE_AIR);
    sensor_readings_t unconfigured = read_many(30);
    expect_true(unconfigured.oxygen_configuration_required && std::isnan(unconfigured.oxygen_percent),
                "Unconfigured simulation also withholds primary oxygen");
    expect_true(oxygen_selection_confirm(OXYGEN_AO2, false) == OXYGEN_SELECT_OK, "Installed AO2 is explicitly confirmed");
    read_many(30);
    gas_cal_reference_t known{}; known.oxygen_percent = 20.9f;
    std::snprintf(known.reference_id, sizeof(known.reference_id), "SIM-AIR");
    expect_true(gas_calibration_capture(GAS_CAL_AO2, 0, &known) == GAS_CAL_OK, "Selected simulation captures air reference");
    sensor_set_mock_profile(SENSOR_MOCK_PROFILE_EAN32); read_many(30);
    known.oxygen_percent = 32;
    std::snprintf(known.reference_id, sizeof(known.reference_id), "SIM-EAN32");
    expect_true(gas_calibration_capture(GAS_CAL_AO2, 1, &known) == GAS_CAL_OK && gas_calibration_save(GAS_CAL_AO2) == GAS_CAL_OK,
                "Selected simulation requires and saves two references");
    sensor_set_mock_profile(SENSOR_MOCK_PROFILE_AIR);
    sensor_readings_t air = read_many(10);
    expect_true(air.status == SENSOR_STATUS_STABLE, "Air profile stabilizes");
    expect_near(air.oxygen_percent, 20.9f, 0.2f, "Air O2 target");
    expect_near(air.helium_percent, 0.0f, 0.2f, "Air He target");
    expect_near(air.co2_ppm, 420.0f, 10.0f, "Air CO2 target");

    sensor_set_mock_profile(SENSOR_MOCK_PROFILE_TRIMIX_18_45);
    sensor_readings_t trimix = read_many(10);
    expect_true(trimix.status == SENSOR_STATUS_STABLE, "Trimix profile stabilizes");
    expect_near(trimix.oxygen_percent, 18.0f, 0.2f, "Trimix O2 target");
    expect_near(trimix.helium_percent, 45.0f, 0.3f, "Trimix He target");

    sensor_set_mock_profile(SENSOR_MOCK_PROFILE_HIGH_CO2);
    sensor_readings_t high_co2 = read_many(10);
    expect_true(high_co2.co2_ppm > 800.0f, "High CO2 profile exceeds advisory value");

    sensor_set_mock_profile(SENSOR_MOCK_PROFILE_UNSTABLE);
    sensor_readings_t unstable = read_many(10);
    expect_true(unstable.status == SENSOR_STATUS_UNSTABLE, "Unstable profile reports unstable");

    sensor_set_mock_profile(SENSOR_MOCK_PROFILE_SENSOR_FAULT);
    sensor_readings_t fault = {};
    sensor_read_all(&fault);
    expect_true(fault.status == SENSOR_STATUS_FAULT, "Fault profile reports fault");
    expect_true(sensor_calibrate_oxygen_air() == ESP_ERR_INVALID_STATE, "Fault blocks O2 calibration");

    sensor_set_mock_profile(SENSOR_MOCK_PROFILE_AIR);
    expect_true(sensor_calibrate_oxygen_air() == ESP_ERR_INVALID_STATE, "Legacy one-click O2 calibration cannot bypass reference capture");
    expect_true(sensor_calibrate_co2_zero() == ESP_OK, "CO2 zero calibration succeeds in simulation");
    expect_true(sensor_calibrate_co2_reference(400) == ESP_OK, "CO2 reference calibration succeeds in simulation");
    expect_true(sensor_calibrate_co2_reference(100) == ESP_ERR_INVALID_ARG, "CO2 reference validates range");
    expect_true(std::strcmp(sensor_mock_profile_name(SENSOR_MOCK_PROFILE_EAN32), "EAN32") == 0,
                "Profile names are stable");
    gas_cal_record_t prior{}; gas_calibration_get_record(GAS_CAL_AO2, &prior);
    expect_true(gas_calibration_record_is_current(&prior) && !gas_calibration_record_is_current(nullptr),
                "Saved record compatibility API requires a complete current source record");
    auto incompatible = prior; ++incompatible.acquisition_revision;
    incompatible.crc32 = gas_cal::record_crc(incompatible);
    expect_true(!gas_calibration_record_is_current(&incompatible),
                "Inactive acquisition-profile archive cannot clear the current calibration requirement");
    incompatible = prior; incompatible.simulated = false;
    incompatible.crc32 = gas_cal::record_crc(incompatible);
    expect_true(!gas_calibration_record_is_current(&incompatible),
                "Record compatibility check cannot approve the other data source");
    expect_true(oxygen_selection_confirm(OXYGEN_AO2, true) == OXYGEN_SELECT_OK, "Replacement explicitly invalidates applicability");
    auto replacement = read_many(30);
    gas_cal_record_t retained{}; gas_calibration_get_record(GAS_CAL_AO2, &retained);
    expect_true(replacement.oxygen_calibration_required && std::isnan(replacement.oxygen_percent) && retained.crc32 == prior.crc32,
                "Replacement withholds old output while preserving the prior record");
    expect_true(gas_calibration_capture(GAS_CAL_JJCCR, 0, &known) == GAS_CAL_SELECTION_REQUIRED,
                "Unselected input cannot be calibrated accidentally");
    oxygen_selection_set_probe_active(true);
    expect_true(oxygen_selection_probe_active(), "Explicit diagnostic probe state is available to hardware acquisition");
    oxygen_selection_set_probe_active(false);
    expect_true(!oxygen_selection_probe_active(), "Diagnostic probe can be stopped");
    storage_pause_writes(true);
    expect_true(oxygen_selection_confirm(OXYGEN_JJCCR, false) == OXYGEN_SELECT_STORAGE_ERROR,
                "Maintenance storage lock rejects a configuration change");
    oxygen_selection_status_t config{}; oxygen_selection_get_status(&config);
    expect_true(config.choice == OXYGEN_AO2 && config.calibration_required,
                "Failed selection save retains selected identity and replacement gate");
    storage_pause_writes(false);

    gas_raw_sample_t frame[GAS_CAL_CHANNEL_COUNT]{};
    bool sampled[GAS_CAL_CHANNEL_COUNT]{true,false,true};
    for (int i=0;i<GAS_CAL_CHANNEL_COUNT;++i) {
        frame[i].timestamp_ms=900000; frame[i].sequence=4242;
        frame[i].voltage_v=0.012; frame[i].gain=i==GAS_CAL_HELIUM ? 1:8;
        frame[i].warmed=true; frame[i].simulated=true;
    }
    expect_true(gas_calibration_publish_frame(config.generation,frame,sampled,true),
                "Current generation publishes one atomic acquisition frame");
    gas_raw_sample_t published{};
    expect_true(sensor_get_raw(GAS_CAL_AO2,&published) && published.sequence==4242,
                "Current acquisition frame reaches the raw store");
    const uint32_t obsolete=config.generation;
    expect_true(oxygen_selection_confirm(OXYGEN_JJCCR,false)==OXYGEN_SELECT_OK,
                "Selection can change after a conversion was started");
    expect_true(!gas_calibration_publish_frame(obsolete,frame,sampled,true),
                "Completed conversion from old setup cannot repopulate reset history");
    expect_true(!sensor_get_raw(GAS_CAL_AO2,&published) && !sensor_get_raw(GAS_CAL_HELIUM,&published),
                "Rejected old frame leaves both raw histories empty");
    oxygen_selection_get_status(&config);
    sampled[GAS_CAL_AO2]=false; sampled[GAS_CAL_JJCCR]=true;
    frame[GAS_CAL_JJCCR].sequence=4243;
    expect_true(gas_calibration_publish_frame(config.generation,frame,sampled,true) &&
                sensor_get_raw(GAS_CAL_JJCCR,&published) && published.sequence==4243,
                "Next valid generation starts the new channel history");
    expect_true(!gas_calibration_publish_frame(obsolete,frame,sampled,true) &&
                sensor_get_raw(GAS_CAL_JJCCR,&published) && published.sequence==4243,
                "Late obsolete reset cannot erase a newer generation sample");

    // Exercise the actual service/journal snapshot across a real committed save.
    // Hardware-worker tests separately inject a save at the former split-call
    // boundary, so a later observed record cannot relabel an earlier conversion.
    for(auto c:{GAS_CAL_JJCCR,GAS_CAL_HELIUM}) {
        const auto calibrate=[&](float adjustment) {
            gas_cal_reference_t reference{};reference.oxygen_percent=20.9f;
            std::snprintf(reference.reference_id,sizeof(reference.reference_id),"PROVENANCE-AIR");
            sensor_set_mock_profile(SENSOR_MOCK_PROFILE_AIR);read_many(30);
            if(gas_calibration_capture(c,0,&reference)!=GAS_CAL_OK)return false;
            sensor_set_mock_profile(c==GAS_CAL_HELIUM?SENSOR_MOCK_PROFILE_TRIMIX_18_45:SENSOR_MOCK_PROFILE_EAN32);read_many(30);
            reference.oxygen_percent=c==GAS_CAL_HELIUM?18:32+adjustment;
            reference.helium_percent=c==GAS_CAL_HELIUM?45+adjustment:0;
            std::snprintf(reference.reference_id,sizeof(reference.reference_id),"PROVENANCE-SPAN");
            return gas_calibration_capture(c,1,&reference)==GAS_CAL_OK && gas_calibration_save(c)==GAS_CAL_OK;
        };
        expect_true(calibrate(0),"Production service saves reference provenance fixture");
        double before_percent=NAN;uint32_t before_revision=0;
        expect_true(gas_calibration_convert_snapshot(c,.013,before_percent,before_revision) && before_revision>0,
                    "One calibration lock returns converted value with applied revision");
        const double preserved_percent=before_percent;
        expect_true(calibrate(.4f),"Intervening calibration save commits a changed fit");
        gas_cal_record_t newer{};gas_calibration_get_record(c,&newer);
        double after_percent=NAN;uint32_t after_revision=0;
        expect_true(gas_calibration_convert_snapshot(c,.013,after_percent,after_revision) &&
                    after_revision==newer.revision && after_revision==before_revision+1 &&
                    before_percent==preserved_percent && std::fabs(before_percent-after_percent)>.001,
                    "Save boundary preserves old snapshot and labels new conversion with new fit");
    }
    double invalid_percent=21;uint32_t invalid_revision=999;
    expect_true(!gas_calibration_convert_snapshot(GAS_CAL_JJCCR,NAN,invalid_percent,invalid_revision) &&
                invalid_revision==0 && std::isnan(invalid_percent),"Failed conversion never reports an applied revision");

    std::printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}

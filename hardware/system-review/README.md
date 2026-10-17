# Engineering evidence and qualification

The active hardware sources are the [PCB 2.0 KiCad project](../pcb/analyzer-v2-bottom/Trimix_Analyzer.kicad_pro) and the [bottom-USB Fusion model](../cad/pcb-v2-bottom/Trimix_Enclosure_PCB2_Bottom.f3d). Use the [current CAD folder](../cad/pcb-v2-bottom/) for its STEP, printable parts and assembly notes, and the [current PCB folder](../pcb/analyzer-v2-bottom/) for manufacturing files and source-bound checks.

This folder retains interface contracts, component-source evidence, qualification notes and dated engineering records. It is not an alternative hardware release. Old editable projects, trial layouts, CAM packages and enclosure checkpoints are available through Git history. A retained report applies only to its recorded inputs; references to the former separate USB board or earlier enclosure describe that report's historical scope.

| Purpose | Retained evidence |
|---|---|
| Firmware and hardware interface | [Interface contract](electrical/interface-contract.md), [host pin map](electrical/host-interface.csv), [connector/test-point map](electrical/connector-testpoint-pinmap.csv), [netlist used by firmware checks](electrical/analyzer-netlist.xml) |
| Component identity and unresolved qualification | [Part qualification ledger](electrical/part-qualification.csv), [selected parts](electrical/standard-part-selections.json), [component height contract](electrical/component-height-contract.csv) |
| USB power, protection and charging limits | [USB source/current review](electrical/usb-default-current-review/README.md), [protection research](electrical/usb-protection-research/README.md), [transient review](electrical/ovp-transient-review/README.md), [firmware power review](software/power/) |
| Host harness and power isolation | [Harness contract](electrical/host-harness-review/README.md), [host power review](electrical/host-power-review/README.md), [Guition source documents and measurements](integration-photo/README.md) |
| Sensor and firmware qualification | [Firmware evidence](software/README.md), [oxygen](software/oxygen/README.md), [CO environment](software/co-environment/README.md), [CO module restrictions](electrical/co-module-use-review.md), [SD recording](software/sd-card/README.md), [OTA/storage](software/ota/README.md) |
| Physical measurements and bring-up | [Acceptance ledger](acceptance.csv), [measurement checklist](electrical/measurement-and-bringup.md), [mechanical measurements](mechanical/MEASUREMENTS.md), [first-power procedure](first-power.md), [instruments](lab-equipment.md), [blank test record](test-record-template.csv) |
| Current PCB component-model provenance | [Amphenol 142138](mechanical/components/Amphenol_RF_142138_README.md), [Samtec HTSW](mechanical/components/Samtec_HTSW_113_07_L_D_007_README.md) |
| Manufacturer references | [Electrical source archive](electrical/sources/), source folders beside the individual reviews, and [mechanical component sources](mechanical/components/) |
| Dated software verification | [Verification scope](verification/README.md); run the current firmware checks for a new checkout |

Digital checks, calculations and nominal CAD models do not replace physical electrical, charging, thermal, mechanical or reference-gas tests. Consult each qualification record for its limits and outstanding measurements; a dated passing result is not approval of a later revision.

Project material follows the [repository license map](../../LICENSE.md). Manufacturer documents and third-party models retain their own terms and provenance.

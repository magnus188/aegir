# PCB review images

These KiCad exports show the current [main analyzer PCB](../analyzer/Trimix_Analyzer.kicad_pro)
and separate [USB input PCB](../usb-input/Trimix_USB_Input.kicad_pro) without
requiring KiCad. Click an image to open it at full resolution.

The images were exported on 2026-09-30 from main board SHA-256
`0962ad86f9834ce71b6439d0f95db603753801153490e78f387a88521e582788`
and USB board SHA-256
`63d5742a1f8a2598a80c5d6a962a5551b9bebfa1a59c6780a00ab8ba3dd15ea7`.
The main PCB has four copper layers; the USB board has two. Back-side routing
views are mirrored so they appear as viewed from underneath. The 3D preview
uses available KiCad component models and is not a physical fit check.

**Prototype order status: HOLD.** The [system review](../../system-review/README.md)
records remaining electrical, mechanical and manufacturing work. In particular,
the USB board has unresolved connector copper-to-edge and annular-ring
violations. These images are review aids, not fabrication files.

## Main analyzer PCB

| Component view | Front copper and silkscreen | Back copper and silkscreen |
| :---: | :---: | :---: |
| <a href="01-main-3d-top.png"><img src="01-main-3d-top.png" alt="Top 3D view of the main PCB" width="210"></a> | <a href="02-main-front-routing.png"><img src="02-main-front-routing.png" alt="Main PCB front copper routing" width="160"></a> | <a href="03-main-back-routing-bottom-view.png"><img src="03-main-back-routing-bottom-view.png" alt="Main PCB back copper routing viewed from underneath" width="160"></a> |

### Selected schematic sheets

| Charging and battery | Oxygen inputs | Helium bridge |
| :---: | :---: | :---: |
| <a href="04-charging-battery-schematic.png"><img src="04-charging-battery-schematic.png" alt="Charging and protected battery schematic" width="300"></a> | <a href="05-oxygen-inputs-schematic.png"><img src="05-oxygen-inputs-schematic.png" alt="Two oxygen input schematic" width="300"></a> | <a href="06-helium-bridge-schematic.png"><img src="06-helium-bridge-schematic.png" alt="Helium bridge schematic" width="300"></a> |

See the [main-board review package](../../system-review/electrical/main-final/README.md)
for the complete schematic, inner copper layers, manufacturing exports and
known limitations.

## USB input PCB

| Schematic | Front copper and silkscreen | Back copper, viewed from underneath |
| :---: | :---: | :---: |
| <a href="07-usb-input-schematic.png"><img src="07-usb-input-schematic.png" alt="USB input schematic" width="300"></a> | <a href="08-usb-front-routing.png"><img src="08-usb-front-routing.png" alt="USB board front copper routing" width="260"></a> | <a href="09-usb-back-routing-bottom-view.png"><img src="09-usb-back-routing-bottom-view.png" alt="USB board back copper routing viewed from underneath" width="260"></a> |

See the [USB-board review package](../../system-review/electrical/usb-final/README.md)
for the fabrication holds and source-matched exports.

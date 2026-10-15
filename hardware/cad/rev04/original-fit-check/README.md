# Original enclosure fit check - 2026-10-15

Current working design: **Trimix_Enclosure_A3_Original v1**, saved in the Fusion project **Trimix analyzer**.
The editable **Trimix_Enclosure_A3_InternalSeal v1** remains saved separately for future work.

## Use

Open `multi-bed/Trimix_Original_H2D_FitCheck.3mf` in Bambu Studio. Printer: Bambu Lab H2D, 0.4 mm nozzle, Bambu PLA Basic, 100% scale. Select the actual build surface; the project assumes Textured PEI.

1. Small component fit checks: 14 parts. Reuse existing prints.
2. Sensor manifold P07: reuse existing print.
3. Original housing P01: reprint.
4. Original rear lid P02: reprint.

Only P01 and P02 differ from the previous InternalSeal fit kit. All other 15 meshes were compared triangle-by-triangle at 0.001 mm precision and match; native volumes also match. The six rear-gasket coupons C01-C06 are omitted. D02 includes the PCB outline and height allocation, so D01 remains omitted. Parts have no added physical ID labels. The original sensor gas-path sealing interfaces remain unchanged.

The companion `output/pdf/Trimix_Original_Fit_Check_Guide.pdf` includes the plate plan, illustrated small-parts map and updated checkbox page.

## Design provenance

Restored from the archived pre-gasket `3d-print/Trimix_Enclosure_A3_PrintReview.f3d`, rather than modifying the gasket variant or PR flange. The eleven printed assembly parts match that original archive. Original 85 x 180 x 43 mm enclosure envelope. Housing alone is 40.6 mm deep; the assembled rear cover completes the original 43 mm depth.

The new full native archive and STEP are `Trimix_Enclosure_A3_Original.f3d` and `.step`. An editable native fit-kit archive and STEP are also included. The native original was recomputed, saved and reopened; 79 component bodies and the 1078-item timeline matched the saved source. See `verification/` for cloud IDs, source checks and reuse proof.

## Slicing validation

Four plates / 17 objects; all parts on their assigned beds; original scale and dimensions verified; project reopens in Bambu Studio. No slicer warnings or support conflicts with the manifold channel exclusions. Nominal layers 0.10 mm, with manifold object override 0.16 mm. 4 walls, 20% gyroid infill. Original housing and lid receive accessible supports; no rear-gasket support blockers.

Reprint plates 3-4 estimate: 35,783 seconds (about 9 h 56 min), 154.15 g including supports/brims. Estimates are slicer predictions, not measured print results.

The coarse all-layer screen flagged two thin housing ledges. Local 0.05 mm contact checks resolved both; this is a deposition-contact screen, not a physical strength or bridge qualification. Physical assembly, clearances and repeated service cycles still need checking. PCB replicas retain the original provisional mechanical allocation; they are not an exact routed PCB validation. No print job was sent.

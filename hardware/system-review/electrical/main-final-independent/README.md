# Independent main PCB manufacturing review

The current source-bound review is
[final-0962ad86/](final-0962ad86/README.md). It checks the routed analyzer
board against exported copper, drill, placement and assembly data. The board
source is [the active KiCad project](../../../pcb/analyzer/Trimix_Analyzer.kicad_pro),
and the [manufacturing packet](../main-final/README.md) contains the CAM files
and factory instructions. The board is still on **prototype order HOLD**.

The audit scripts in this directory read exported Gerber/Excellon bytes and
native KiCad coordinates. They do not order fabrication or qualify current,
thermal, analog, or physical performance. The previous diagnostic run and
older board review remain available through Git history.

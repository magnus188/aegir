# Tabler menu icons

The main menu uses these five outline icons from [Tabler Icons](https://tabler.io/icons):

| Menu item | Source icon |
| --- | --- |
| Analyse | `flask.svg` |
| Dive Planner | `chart-line.svg` |
| History | `history.svg` |
| Cylinders | `scuba-diving-tank.svg` |
| Settings | `adjustments-horizontal.svg` |

The original SVGs were retrieved from the [Tabler Icons repository](https://github.com/tabler/tabler-icons) on 2026-09-27. They are used without path edits. The menu uses generated 64 × 64 alpha masks; the Analyse screen's Save Cyl button uses a 24 × 24 mask of the same cylinder artwork. See [LICENSE](LICENSE) for the upstream MIT terms.

Run `python3 scripts/generate_menu_icons.py` from the firmware directory to regenerate `main/ui/images/menu_icons.c`.

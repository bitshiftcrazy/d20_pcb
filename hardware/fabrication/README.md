# Fabrication Files

This directory contains manufacturing outputs generated from the KiCad
project.

## Generic files
- `gerbers/` Gerber files for PCB fabrication
- `drill/` Drill files
- `pickplace/` Generic component placement files
- `bom/` Generic bill of materials

## JLCPCB-specific files
- `jlcpcb/` BOM and placement files adjusted for JLCPCB assembly
   (note: this BOM contains values and parts)

These files can be regenerated from the KiCad sources in `hardware/`.
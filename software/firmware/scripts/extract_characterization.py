#!/usr/bin/env python3
"""Extract the firmware's CSV1 raw gas records from a captured serial log.

No serial port is opened. Example:
  python3 scripts/extract_characterization.py serial.log measurements.csv
Raw rows are evidence, not validated gas concentrations.
"""
import argparse
import csv
from pathlib import Path
FIELDS=['timestamp_ms','channel_id','selected_oxygen','sequence','selection_generation','adc_code','voltage_v','gain','excitation_v','bias_v','temperature_c','humidity_percent','pressure_bar','environment_valid','calibration_revision','fault_mask','warmed','acquisition_revision']
def extract(source, destination):
    rows=0
    with Path(source).open(errors='replace') as src, Path(destination).open('w',newline='') as dst:
        writer=csv.writer(dst); writer.writerow(FIELDS)
        for line in src:
            if 'CSV1,' not in line: continue
            payload=line.split('CSV1,',1)[1].strip()
            data=next(csv.reader([payload]))
            if len(data)!=len(FIELDS): raise ValueError(f'Incomplete CSV1 row after {rows} records')
            for index in [0,1,2,3,4,5,7,13,14,15,16,17]: int(data[index])
            for index in [6,8,9,10,11,12]: float(data[index])
            writer.writerow(data); rows+=1
    return rows
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('serial_log',type=Path);parser.add_argument('csv',type=Path)
    args=parser.parse_args()
    if args.serial_log.resolve()==args.csv.resolve(): parser.error('Input and output must be different files')
    print(f'Extracted {extract(args.serial_log,args.csv)} raw records')

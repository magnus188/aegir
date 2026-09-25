#!/usr/bin/env python3
"""Reconcile the production firmware's board contract with the KiCad netlist.

Run after the electrical audit refreshes analyzer-netlist.xml. This checks the
logical interface; connector mating views, cable ratings and CAD fit stay separate.
"""
import argparse
import csv
import hashlib
import json
import re
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
SIGNALS={'kSda':'I2C_SDA','kScl':'I2C_SCL','kCoTx':'CO_UART_TX','kCoRx':'CO_UART_RX',
         'kPowerInterrupt':'POWER_INT_N','kPowerKill':'POWER_KILL_N','kHeliumEnable':'HE_ENABLE',
         'kChargerAlert':'CHG_INT_N','kUsbPermission':'USB_ILIM_AUTH','kCoEnable':'CO_UART_EN'}

def audit(root=ROOT, contract=None, netlist=None):
    contract=contract or root/'software/firmware/main/hardware_contract.h'
    netlist=netlist or root/'hardware/system-review/electrical/analyzer-netlist.xml'
    paths=[contract,netlist,root/'hardware/system-review/electrical/host-interface.csv']
    text=contract.read_text()
    pins={name:(int(gpio),int(pin)) for name,gpio,pin in re.findall(r'constexpr HostPin (\w+)\{(\d+),(\d+)\};',text)}
    constants={name:int(value,0) for name,value in re.findall(r'constexpr uint\d+_t (\w+)=(0x[0-9a-fA-F]+|\d+);',text)}
    members={}
    for net in ET.parse(netlist).getroot().findall('./nets/net'):
        for node in net.findall('node'):
            members[(node.attrib['ref'],node.attrib['pin'])]=net.attrib['name'].split('/')[-1]
    with paths[2].open() as src: host={row['net']:row for row in csv.DictReader(src)}
    checks=[]
    def check(name,ok,detail=None): checks.append({'check':name,'status':'passed digitally' if ok else 'correction required','detail':detail})
    for name,net in SIGNALS.items():
        gpio,pin=pins.get(name,(-1,-1)); row=host.get(net,{})
        check(net+' firmware / JP1 / netlist', members.get(('J301',str(pin)))==net and
              str(gpio)==row.get('GPIO') and str(pin)==row.get('J301_pin')==row.get('Guition_JP1_pin'),
              {'firmware_gpio':gpio,'connector_pin':pin,'net':members.get(('J301',str(pin)))})
    check('Complete unique assigned GPIOs',all(n in pins for n in SIGNALS) and
          len({pins.get(n,(-1,-1))[0] for n in SIGNALS})==len(SIGNALS))
    check('GPIO52 and J301.7 unused',pins.get('kUnusedOxygenSelector')==(52,7) and
          members.get(('J301','7'),'').startswith('unconnected'))
    for name,value in {'kGaugeAddress':0x36,'kOxygenAddress':0x40,'kHeliumAddress':0x41,'kChargerAddress':0x6a,
                       'kUsbCcAddress':0x47,'kUsbBc12Address':0x5f,
                       'kEnvironmentAddress0':0x76,'kEnvironmentAddress1':0x77,'kI2cFrequencyHz':100000,'kCoBaud':9600}.items():
        check(name+' reviewed value',constants.get(name)==value,constants.get(name))
    for ref,a0 in [('U401','GND'),('U502','HOST_3V3')]:
        check(ref+' fixed address straps',members.get((ref,'1'))==a0 and members.get((ref,'2'))=='GND')
        check(ref+' bus pins',members.get((ref,'15'))=='I2C_SDA' and members.get((ref,'16'))=='I2C_SCL')
    for ref,pinmap in {
        'U110':{'1':'USB_CC1','2':'USB_CC2','3':'GND','5':'GND','7':'I2C_SDA',
                '8':'I2C_SCL','10':'GND','11':'GND','12':'HOST_3V3'},
        'U111':{'3':'I2C_SCL','4':'I2C_SDA','7':'USB_D_M','8':'USB_D_P',
                '9':'GND','10':'GND','11':'GND','12':'HOST_3V3'},
        'J101':{'1':'USB_5V','2':'GND','3':'USB_CC1','4':'USB_CC2',
                '5':'USB_D_P','6':'USB_D_M'},
    }.items():
        check(ref+' source detection pin contract',all(members.get((ref,p))==n for p,n in pinmap.items()),
              {p:members.get((ref,p)) for p in pinmap})
    check('BQ D+/D- isolated from USB negotiation',all(
          members.get(('U101',p),'').startswith('unconnected') for p in ['2','3']))
    check('Guition 5V uses isolated HOST_5V',all(members.get(('J301',p))=='HOST_5V' for p in ['2','4']))
    check('Lost USB latch authorization reaches GPIO49 through Q112',all(
          members.get((ref,pin))==net for ref,pin,net in [
              ('U112','3','USB_PERMISSION_Q_N'),('Q112','1','USB_PERMISSION_Q_N'),
              ('R128','1','USB_PERMISSION_Q_N'),('R128','2','GND'),
              ('Q112','2','GND'),('Q112','3','CHG_INT_N'),
              ('U101','7','CHG_INT_N'),('J301','13','CHG_INT_N'),
              ('R107','1','HOST_3V3'),('R107','2','CHG_INT_N')]))
    # Check actual production consumers, so a correct but unused header is not evidence.
    consumers={
      'software/firmware/main/sensors/system_i2c.cpp':['kSda.gpio','kScl.gpio','kI2cFrequencyHz','kGaugeAddress','kOxygenAddress','kHeliumAddress','kChargerAddress','kUsbCcAddress','kEnvironmentAddress0','kEnvironmentAddress1'],
      'software/firmware/main/services/system_power.cpp':['kPowerKill.gpio','kPowerInterrupt.gpio','kUsbPermission.gpio','kChargerAlert.gpio'],
      'software/firmware/main/sensors/sensor_hardware.cpp':['kHeliumEnable.gpio','kCoEnable.gpio','kCoTx.gpio','kCoRx.gpio','kCoBaud'],
      'software/firmware/main/sensors/power_monitor.h':['kGaugeAddress','kChargerAddress'],
      'software/firmware/main/sensors/ads122c04.h':['kOxygenAddress','kHeliumAddress'],
      'software/firmware/main/sensors/environment_monitor.cpp':['kEnvironmentAddress0','kEnvironmentAddress1'],
      'software/firmware/main/sensors/usb_source_monitor.h':['kUsbCcAddress'],
      'software/firmware/main/sensors/bc12_monitor.h':['kUsbBc12Address']}
    for file,names in consumers.items():
        path=root/file; paths.append(path); source=path.read_text()
        for name in names: check(file+' consumes '+name,'hardware_contract::'+name in source)
        check(file+' does not use selector','kUnusedOxygenSelector' not in source and 'GPIO_NUM_52' not in source)
    profile=root/'software/firmware/main/sensors/gas_acquisition_config.h'; paths.append(profile)
    source=profile.read_text()
    check('Oxygen profile gain8,20SPS,internal2048mV,PGA enabled',bool(re.search(r'kOxygenProfile\{\d+, kOxygenGain, 20, 2048, false, false\}',source)) and 'kOxygenGain = 8' in source)
    check('Helium profile gain1,20SPS,internal2048mV,PGA bypass',bool(re.search(r'kHeliumProfile\{\d+, kHeliumGain, 20, 2048, true, false\}',source)) and 'kHeliumGain = 1' in source)
    return {'scope':'Logical production firmware-to-schematic contract; physical cable and CAD checks excluded',
            'checks':checks,'passed':sum(c['status']=='passed digitally' for c in checks),
            'failed':sum(c['status']!='passed digitally' for c in checks),
            'sources':[{'path':str(p.relative_to(root)) if p.is_relative_to(root) else str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in paths]}

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument('--output',type=Path)
    args=parser.parse_args(); result=audit()
    if args.output: args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(f"System contract: {result['passed']} passed digitally, {result['failed']} corrections required")
    for check in result['checks']:
        if check['status']!='passed digitally': print(check)
    raise SystemExit(bool(result['failed']))

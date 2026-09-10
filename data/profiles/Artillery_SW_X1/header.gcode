G21 ; set units to millimeters
M190 S55 ; wait for bed temperature to be reached
M104 S200 ; set temperature
G28 ; home all axes
G1 X20 Y20 Z20 F10000
M109 S200 ; wait for temperature to be reached
G90 ; use absolute coordinates
M82 ; use absolute distances for extrusion
G92 E0

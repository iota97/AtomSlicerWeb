G21 ; set units to millimeters
M104 S210 ; set extruder temp
M140 S50 ; set bed temp
G28 ; home head
G90 ; use absolute coordinates
M82 ; use absolute extrusion
M190 S50 ; wait for bed temp
M109 S210 ; wait for extruder temp
G0 F1000 Y-70 Z20
G92 E0
G1 F1000 E20.0
G92 E0
G21 ; set units to millimeters
;M106 S204 ; turn fan on
M190 S50 ; wait for bed temperature to be reached
M104 S200 ; set temperature
G28 ; home all axes
G92 E0
M109 S200 ; wait for temperature to be reached
G90 ; use absolute coordinates
G92 E0
M82 ; use absolute distances for extrusion
G92 E0
;M106 S204 ; turn fan on

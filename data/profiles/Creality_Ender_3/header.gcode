G21 ; set units to millimeters
G90 ; use absolute coordinates
M82 ; use absolute distances for extrusion

M190 S50 ; wait for bed temperature to be reached
M104 S200 ; set extruder temperature
G28 ; home all axes
G0 F6200 X0 Y0
M109 S200 ; wait for extruder temperature to be reached

G92 E0
G1 Z1.0 F3000 ; move z up little to prevent scratching of surface
G1 X20 Y10 Z0.3 F5000.0 ; move to start-line position
G1 X150 Y10 Z0.3 F1500.0 E15 ; draw 1st line
G1 X150 Y10 Z0.3 F5000.0 ; move to side a little
G1 X20 Y10.3 Z0.3 F1500.0 E30 ; draw 2nd line
G92 E0 ; reset extruder
; done purging extruder

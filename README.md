LRRC
====

Long-range RC system software for the UAS and the ground station

Root directory is the IAR EW workspace for the MSP430-based flight controller; main.c is the flight controller code.  /LabView contains the ground station software, built on LabView 8.2.1.  Open the .lvproj file, update any path conflicts as needed (newer versions of LV should do this automatically) then open the cSys01.vi instrument to run.  Expects an FTDI VCP to a paired 900MHz radio and a joystick (built and tested with a MS Sidewinder Precision Pro).

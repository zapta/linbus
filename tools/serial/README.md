#SERIAL DUMP UTILITY

The serial dump utility is a python scripts that runs on a computer and help analyze serial data
recieved from the Linbus Analyzer.

##MAC OSX AND LINUX INSTALLATION

###1. Install Python
Go to http://www.python.org/download and install python 2.7.x. Note that 2.7.x is not the latest version
of python but the download page allows you to install older stable versions. 
(tested tested with python 2.7.5 on Mac OSX 10.9.2)

###2. Install PIP
Pip is the python package manager. We need it to install the pyserial library in the next step. 
The pip installtion script is provided here for convenience). To install pip run the following command in this directory (the pip 

```
sudo python ./lib/get-pip.py
```

###3. Intall the PySerial library
This library is used by the serial dump utility to access the serial ports on this computer. To install the pyserial library run the
following

```
sudo pip install pyserial
```

###4. Identify The Serial Port On Your Computer
Plug the linbus analyzer to a USB port of your computer and list the USB serial devices as follows

```
ls /dev/*usb*
```

Look for device with name like /dev/cu.usbserial-A702YSE3. This is your serial port. (you may see also a matching device with 'tty' prefix. Ignore it).

###5. Run the serial utility
With the Linbus Analyzer attached to your computer, run the follwoing command (replace the port name with the one you identified in a previous step)

```
python ./serial_dump.py --port=/dev/cu.usbserial-A702YSE3
```


##WINDOWS INSTALLATION

###1. Install Python
Go to http://www.python.org/download and install python 2.7.x. Note that 2.7.x is not the latest version
of python but the download page allows you to install older stable versions. 
(tested tested with python 2.7.6 on Windows 7 64 bit)

###2. Install PIP
Pip is the python package manager. We need it to install the pyserial library in the next step. 
The pip installtion script is provided here for convenience). To install pip run the following command in this directory (the pip 

```
c:\Python27\python.exe .\lib\get-pip.py
```

###3. Intall the PySerial library
This library is used by the serial dump utility to access the serial ports on this computer. To install the pyserial library run the
following

```
c:\Python27\scripts\python.exe install pyserial
```

###4. Identify The Serial Port On Your Computer
Plug the linbus analyzer to a USB port of your computer, open the Windows device manager and look for the new COM file that were added to your computer (under Ports (COM & LPT). For the rest of this procedue we will assume it is COM7.

###5. Run the serial utility
With the Linbus Analyzer attached to your computer, run the follwoing command (replace the port name with the one you identified in a previous step)

```
c:\Python27\python.exe serial_dump.py --port=com7
```


##GENERAL OPERATION

###Normal Vs. Diff Modes
The serial utility has two modes, normal and diff. In the normal mode which is the defaut mode, the program simply dumps all the serial data it recieves from the analyzer. In diff mode it prints only frames and bits that changed from the previous frame with that id. 
The diff mode is useful to identify affected bits when you toggle switches and controls in your car. To enable the diff mode, add the command line flag --diff=1.

Sample dump in normal mode (filtered for frame id 8e by grepping for '  8e'). Eac frame is dump with timestamp, frame id (0x8e), data bytes (8 bytes in this example) and checksum byte. Notice how the second data byte changes from 0x40 to 0x44 when the button it monitors is pressed: 
```
00004.346  8e 12 40 00 56 44 a2 1b cc f9
00004.538  8e 12 40 00 56 44 a2 1b cc f9
00004.761  8e 12 40 00 56 44 a2 1b cc f9
00004.953  8e 12 44 00 56 44 a2 1b cc f5
00005.177  8e 12 44 00 56 44 a2 1b cc f5
00005.369  8e 12 44 00 56 44 a2 1b cc f5
00005.577  8e 12 44 00 56 44 a2 1b cc f5
00005.784  8e 12 40 00 56 44 a2 1b cc f9
00005.976  8e 12 40 00 56 44 a2 1b cc f9
00006.184  8e 12 40 00 56 44 a2 1b cc f9
```

Sample dump in diff mode. Notice how the bit changes when its button is pressed and released:
```
00005.030  8e: | ---- ---- | ---- -0-- | ---- ---- | ---- ---- | ---- ---- | ---- ---- | ---- ---- | ---- ---- |
00005.608  8e: | ---- ---- | ---- -1-- | ---- ---- | ---- ---- | ---- ---- | ---- ---- | ---- ---- | ---- ---- |
00005.816  8e: | ---- ---- | ---- -0-- | ---- ---- | ---- ---- | ---- ---- | ---- ---- | ---- ---- | ---- ---- |
```

###Timestamps
The program prepend to each line it prints a timestamp, in milliseconds, relative to the start time of the program. This timestamping is done in the python script, not on the Linbus Analyzer and thus can be slightly off due to different frame lengths.

###Filtering
If you want to see data only for a specific frame id you can use a text based filter program like grep and pipe the output of the serial utility into the filter.

###Capturing
If you want to capture the output of the Linbus Analyzer simply redirect the output the serial utility into a file (use the 'tee' filter to have it sent also the screen).

###Direct Port Access
You can access the Linbus Analyzer data directly, without the serial dump utility, by capturing the stream from the serial port you identified eariler (e.g. with a terminal program). The data format is 115,200bps, 8 data bit, 1 stop, no parity.




LINBUS INJECTOR FORK - PORSCHE 981 - MEMORY
===========================================

This directory contains the custom Linbus Injector source code for Porsche 981 that was 
contributed by John Miles (www.ke5fx.com).  

It is based on the reference source code in the src_reference directory with the following modifications:

* Compilation is done using the avr tool chain without dependency on the Arduino IDE and libraries.

(since the Arduino IDE includes the avr tool chain, one simple way to install the tool chain is
to install the Arduion IDE).

* When ignition is turned on, the injector restores the last state of the supported buttons
(vs. the reference source code that restore a predefined state).

* TODO: what buttons and functions are supported?

* TODO: is there a way to disable it when visiting the dealer?

* TODO: how to build? (what OS's are supported by the make file?)

 

//
// Needed when building with the makefile outside of the Arduino environment.  
// Remove or rename this file to restore the original Arduino-ness of the project.
//

#ifndef _ARD_H
#define _ARD_H

#include <stdio.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#include "wstring.h"

typedef unsigned char byte;
typedef unsigned char boolean;

__extension__ typedef int __guard __attribute__((mode (__DI__)));

extern "C" int __cxa_guard_acquire(__guard *);
extern "C" void __cxa_guard_release (__guard *);
extern "C" void __cxa_guard_abort (__guard *); 

#endif

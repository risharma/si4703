//
//  arduino_serial..h
//  ArduinoRadio
//
//  Created by Rishabh Sharma on 9/23/14.
//  Copyright (c) 2014 Gracenote. All rights reserved.
//

#ifndef arduino_serial__h
#define arduino_serial__h

#include <stdint.h>   // Standard types

int serialport_init(const char* serialport, int baud);
int serialport_close(int fd);
int serialport_writebyte( int fd, uint8_t b);
int serialport_write(int fd, const char* str);
int serialport_read_until(int fd, char* buf, char until, int buf_max,int timeout);
int serialport_flush(int fd);

#endif

//
//  arduino_radio.c
//  ArduinoRadio
//
//  Created by Rishabh Sharma on 9/23/14.
//  Copyright (c) 2014 Gracenote. All rights reserved.
//

#include <stdio.h>    // Standard input/output definitions
#include <stdlib.h>
#include <string.h>   // String function definitions
#include <unistd.h>   // for usleep()
#include <getopt.h>
#include <time.h>

#include "arduino_radio.h"

#include "arduino_serial.h"

#define BUF_MAX	256
#define EOL_CHAR	'\n'

#define SEEK_UP			"1"
#define SEEK_DOWN		"2"

#define TUNE_UP			">"
#define TUNE_DOWN		"<"

#define VOLUME_UP		"+"
#define VOLUME_DOWN		"-"

#define SET_STATION		"3"
#define CURRENT_STATION	"4"

#define SET_VOLUME		"5"
#define CURRENT_VOLUME	"6"

#define MUTE_ON			"7"
#define MUTE_OFF		"7"

#define RDS_RT			"8"
#define RDS_PS			"9"

#define POWER_OFF		"0"

#define END_COMMAND		"^"

static void _delay(int milliseconds)
{
    long	pause;
    clock_t now;
	clock_t	then;
	
    pause = milliseconds*(CLOCKS_PER_SEC/1000);
    now = then = clock();
    while( (now-then) < pause )
	{
        now = clock();
	}
}

static void error_info(char* msg)
{
    fprintf(stderr, "%s\n",msg);
    exit(EXIT_FAILURE);
}


int arduino_radio_init(const char* serial_port,
					   arduino_radio_handle_t* h_arduino_radio
					   )
{
	int error = 0;
	
	int fd = -1;
	int baudrate = 9600; //default
	
	fd = serialport_init(serial_port, baudrate);
	if (-1 == fd)
	{
		error_info("Couldn't open port.");
		error = -1;
	}
		
	serialport_flush(fd);

	*h_arduino_radio = fd;
	
	return error;
}

int arduino_radio_config_set(arduino_radio_handle_t h_arduino_radio,
							 const char* configuration_option)
{
	int error	= 0;
	
	int fd = 0;
	char buf[BUF_MAX];
	int rc = 0;
	
	fd = h_arduino_radio;
	
	if (-1 == fd)
	{
		error_info("Serial port not open.");
		error = -1;
	}
	
	if (!strcmp(configuration_option, ARDUINO_RADIO_SEEK_UP))
	{
		sprintf(buf, "%s%s\n", SEEK_UP, END_COMMAND);
		rc = serialport_write(fd, buf);
	}
	
	else if (!strcmp(configuration_option, ARDUINO_RADIO_SEEK_DOWN))
	{
		sprintf(buf, "%s\n", SEEK_DOWN);
		rc = serialport_write(fd, buf);
	}
	
	else if (!strcmp(configuration_option, ARDUINO_RADIO_TUNE_UP))
	{
		sprintf(buf, "%s\n", TUNE_UP);
		rc = serialport_write(fd, buf);
	}
	
	else if (!strcmp(configuration_option, ARDUINO_RADIO_TUNE_DOWN))
	{
		sprintf(buf, "%s\n", TUNE_DOWN);
		rc = serialport_write(fd, buf);
	}
	
	else if (!strcmp(configuration_option, ARDUINO_RADIO_VOLUME_UP))
	{
		sprintf(buf, "%s\n", VOLUME_UP);
		rc = serialport_write(fd, buf);
	}
	
	else if (!strcmp(configuration_option, ARDUINO_RADIO_VOLUME_DOWN))
	{
		sprintf(buf, "%s\n", VOLUME_DOWN);
		rc = serialport_write(fd, buf);
	}
	
	else if (!strcmp(configuration_option, ARDUINO_RADIO_MUTE_ON))
	{
		
	}
	
	else if (!strcmp(configuration_option, ARDUINO_RADIO_MUTE_OFF))
	{
		
	}
	
	else
	{
		error_info("Incorrect configuration.");
		error = -1;
	}
	
	if (-1 == rc)
	{
		error_info("Error writing.");
		error = -1;
	}
	
	return error;
}

int arduino_radio_current_station_get(arduino_radio_handle_t h_arduino_radio,
									 int* current_station
									 )
{
	int error = 0;
	
	int fd = 0;
	char buf[BUF_MAX];
	int rc =	0;
	
	fd = h_arduino_radio;
	
	if (-1 == fd)
	{
		error_info("Serial port not open.");
		error = -1;
	}
	
	memset(buf, 0, BUF_MAX);
	sprintf(buf, "%s%s", CURRENT_STATION, END_COMMAND);
	rc = serialport_write(fd, buf);
	if (0 == rc)
	{
		memset(buf, 0, BUF_MAX);
		serialport_read_until(fd, buf, EOL_CHAR, BUF_MAX, 5000);
		
		*current_station = atoi(buf);
	}
	
	return error;
}

int arduino_radio_current_station_set(arduino_radio_handle_t h_arduino_radio,
									  int current_station
									  )
{
	int		error	= 0;
	int		fd		= 0;
	char	buf[BUF_MAX];
	int		rc		= 0;
	
	fd = h_arduino_radio;
	
	if (-1 == fd)
	{
		error_info("Serial port not open.");
		error = -1;
	}
	
	memset( buf, 0, BUF_MAX );
	sprintf( buf, "%s%d%s", SET_STATION, current_station, END_COMMAND );
	_delay(1);
	rc = serialport_write(fd, buf);

	error = rc;
	
	return error;
}

int arduino_radio_rds_rt(arduino_radio_handle_t h_arduino_radio,
						 int timeout,
						 char* rds_rt)
{
	int error = 0;
	
	int		fd			= 0;
	char	buf[BUF_MAX];
	int		rc			= 0;
	int		iterator	= 0;
	
	fd = h_arduino_radio;
	
	if (-1 == fd)
	{
		error_info("Serial port not open.");
		error = -1;
	}
	
	memset(buf, 0, BUF_MAX);
	sprintf(buf, "%s%d%s", RDS_RT, timeout, END_COMMAND);
	rc = serialport_write(fd, buf);
	if (0 == rc)
	{
		memset(buf, 0, BUF_MAX);
		serialport_read_until(fd, buf, EOL_CHAR, BUF_MAX, timeout);
		for(iterator = 0; iterator < 100; iterator++)
		{
			rds_rt[iterator] = buf[iterator];
		}
	}
	else
	{
		error_info("Serial port error.");
		error = rc;
	}
	
	return error;
}

int arduino_radio_rds_ps(arduino_radio_handle_t h_arduino_radio,
						 int timeout,
						 char* rds_ps)
{
	int error = 0;
	
	return error;
}

int arduino_radio_shutdown(arduino_radio_handle_t h_arduino_radio)
{
	int		error			= 0;
	int		fd				= 0;
	char	buf[BUF_MAX]	= {0};
	int		rc				= 0;
	
	fd = h_arduino_radio;
	
	if (-1 == fd)
	{
		error_info("Serial port not open.");
		error = -1;
	}
	
	else
	{
		sprintf(buf, "%s%s\n", POWER_OFF, END_COMMAND);
		rc = serialport_write(fd, buf);
		
		if (-1 == rc)
		{
			error_info("Error writing.");
			error = -1;
		}
	}
	
	return error;
}


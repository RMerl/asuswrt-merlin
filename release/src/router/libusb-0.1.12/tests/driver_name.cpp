// -*- C++;indent-tabs-mode: t; tab-width: 4; c-basic-offset: 4; -*-
/*
 * driver_name.cpp
 *
 *  Test suite program for C++ bindings
 */

#include <iostream>
#include <iomanip>
#include <usbpp.h>

using namespace std;

int main(void)
{
	USB::Busses buslist; 

	cout << "bus/device  idVendor/idProduct" << endl;

	//  buslist.init();

	USB::Bus *bus;
	list<USB::Bus *>::const_iterator biter;
	USB::Device *device;
	list<USB::Device *>::const_iterator diter;
	int i, j;
	int retval;
	string driver;

	for (biter = buslist.begin(); biter != buslist.end(); biter++) {
		bus = *biter;

		for (diter = bus->begin(); diter != bus->end(); diter++) {
			device = *diter;

			USB::Configuration *this_Configuration;
			this_Configuration = device->firstConfiguration();
			for (i=0; i < device->numConfigurations(); i++) {
				USB::Interface *this_Interface;
				this_Interface = this_Configuration->firstInterface();
				for (j=0; j < this_Configuration->numInterfaces(); j++) {
					retval = this_Interface->driverName(driver);
					if (0 == retval) {
						cout << bus->directoryName() << "/" 
							 << device->fileName() << "     "
							 << uppercase << hex << setw(4) << setfill('0')
							 << device->idVendor() << "/"
							 << uppercase << hex << setw(4) << setfill('0')
							 << device->idProduct() << "    "
							 << "driver: " << driver << endl;
					} else {
						cout << "fetching driver string failed (" << retval << "): " << usb_strerror() << endl;
						return EXIT_FAILURE;
					}
					this_Interface = this_Configuration->nextInterface();
				}
				this_Configuration = device->nextConfiguration();
			}
		}
	}

	return 0;
}

// -*- C++;indent-tabs-mode: t; tab-width: 4; c-basic-offset: 4; -*-
/*
 * get_resolution.cpp
 *
 * Note that this testcase fails if not run as root
 *
 *  Test suite program for C++ bindings
 */

#include <iostream>
#include <iomanip>
#include <usbpp.h>

#define VENDOR_LOGITECH 0x046D

using namespace std;

int main(void)
{
	USB::Busses buslist;
	USB::Device *device;
	list<USB::Device *> miceFound;
	list<USB::Device *>::const_iterator iter;
	unsigned char resolution[1];

	resolution[0] = 'Z';

	cout << "idVendor/idProduct/bcdDevice" << endl;

	USB::DeviceIDList mouseList;

	mouseList.push_back(USB::DeviceID(VENDOR_LOGITECH, 0xC00E)); // Wheel Mouse Optical
	mouseList.push_back(USB::DeviceID(VENDOR_LOGITECH, 0xC012)); // MouseMan Dual Optical
	mouseList.push_back(USB::DeviceID(VENDOR_LOGITECH, 0xC00F)); // MouseMan Traveler
	mouseList.push_back(USB::DeviceID(VENDOR_LOGITECH, 0xC024)); // MX300 optical 
	mouseList.push_back(USB::DeviceID(VENDOR_LOGITECH, 0xC025)); // MX500 optical 
	mouseList.push_back(USB::DeviceID(VENDOR_LOGITECH, 0xC031)); // iFeel Mouse (silver)

	miceFound = buslist.match(mouseList);

	for (iter = miceFound.begin(); iter != miceFound.end(); iter++) {
		device = *iter;
		cout << hex << setw(4) << setfill('0')
			 << device->idVendor() << "  /  "
			 << hex << setw(4) << setfill('0')
			 << device->idProduct() << "  /  "
			 << hex << setw(4) << setfill('0')
			 << device->idRevision() << "       "
			 << endl;

		if ( 0 > device->controlTransfer(USB_TYPE_VENDOR | USB_ENDPOINT_IN,
										 0x01,
										 0x000E,
										 0x0000,
										 0x0001,
										 resolution) ) {
			cout << "control transfer test failed: " << usb_strerror() << endl;
			return EXIT_FAILURE;
		}

		if (3 == resolution[0]) {
			cout << "Resolution is 400cpi" << endl;
		} else if (4 == resolution[0]) {
			cout << "Resolution is 800cpi" << endl;
		} else {
			cout << "Unexpected resolution result" << endl;
		}
	}

	return 0;
}

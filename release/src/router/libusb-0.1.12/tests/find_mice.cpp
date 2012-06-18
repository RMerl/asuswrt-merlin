// -*- C++;indent-tabs-mode: t; tab-width: 4; c-basic-offset: 4; -*-
/*
 * find_mice.cpp
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

	cout << "idVendor/idProduct/bcdDevice" << endl;

	USB::DeviceIDList mouseList;

	mouseList.push_back(USB::DeviceID(VENDOR_LOGITECH, 0xC00E)); // Wheel Mouse Optical
	mouseList.push_back(USB::DeviceID(VENDOR_LOGITECH, 0xC012)); // MouseMan Dual Optical
	mouseList.push_back(USB::DeviceID(VENDOR_LOGITECH, 0xC00F)); // MouseMan Traveler
	mouseList.push_back(USB::DeviceID(VENDOR_LOGITECH, 0xC024)); // MX300 Optical 
	mouseList.push_back(USB::DeviceID(VENDOR_LOGITECH, 0xC025)); // MX500 Optical 
	mouseList.push_back(USB::DeviceID(VENDOR_LOGITECH, 0xC503)); // Logitech Dual receiver
	mouseList.push_back(USB::DeviceID(VENDOR_LOGITECH, 0xC506)); // MX700 Optical Mouse
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
	}

	return 0;
}

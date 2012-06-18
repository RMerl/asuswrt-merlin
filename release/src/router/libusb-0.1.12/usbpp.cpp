// -*- C++;indent-tabs-mode: t; tab-width: 4; c-basic-offset: 4; -*-
/*
 * USB C++ bindings
 *
 * Copyright (C) 2003 Brad Hards <bradh@frogmouth.net>
 *
 * This library is covered by the LGPL, read LICENSE for details.
 */

#include <errno.h>
#include <cstdlib>
#include <stdio.h>

//remove after debugging
#include <iostream>

#include "usbpp.h"

namespace USB {
  Busses::Busses(void)
  {
    usb_init();
    rescan();
  }

  void Busses::rescan(void)
  {
    struct usb_bus *bus;
    struct usb_device *dev;
    Bus *this_Bus;
    Device *this_Device;
    Configuration *this_Configuration;
    Interface *this_Interface;
    AltSetting *this_AltSetting;
    Endpoint *this_Endpoint;
    int i, j, k, l;

    usb_find_busses();
    usb_find_devices();

    for (bus = usb_get_busses(); bus; bus = bus->next) {
      std::string dirName(bus->dirname);

      this_Bus = new Bus;
      this_Bus->setDirectoryName(dirName);
      push_back(this_Bus);

      for (dev = bus->devices; dev; dev = dev->next) {
	std::string buf, fileName(dev->filename);
	usb_dev_handle *dev_handle;
	int ret;

	this_Device = new Device;
	this_Device->setFileName(fileName);
	this_Device->setDescriptor(dev->descriptor);

	dev_handle = usb_open(dev);

	if (dev_handle) {
	  this_Device->setDevHandle(dev_handle);

	  if (dev->descriptor.iManufacturer) {
	    ret = this_Device->string(buf, dev->descriptor.iManufacturer);
	    if (ret > 0)
	      this_Device->setVendor(buf);
	  }

	  if (dev->descriptor.iProduct) {
	    ret = this_Device->string(buf, dev->descriptor.iProduct);
	    if (ret > 0)
	      this_Device->setProduct(buf);
	  }

	  if (dev->descriptor.iSerialNumber) {
	    ret = this_Device->string(buf, dev->descriptor.iSerialNumber);
	    if (ret > 0)
	      this_Device->setSerialNumber(buf);
	  }
	}

	this_Bus->push_back(this_Device);

	for (i = 0; i < this_Device->numConfigurations(); i++) {
	  this_Configuration = new Configuration;
	  this_Configuration->setDescriptor(dev->config[i]);
	  this_Device->push_back(this_Configuration);

	  for (j = 0; j < this_Configuration->numInterfaces(); j ++) {
	    this_Interface = new Interface;
	    this_Interface->setNumAltSettings(dev->config[i].interface[j].num_altsetting);
	    this_Interface->setParent(this_Device);
	    this_Interface->setInterfaceNumber(j);
	    this_Configuration->push_back(this_Interface);

	    for (k = 0; k < this_Interface->numAltSettings(); k ++) {
	      this_AltSetting = new AltSetting;
	      this_AltSetting->setDescriptor(dev->config[i].interface[j].altsetting[k]);
	      this_Interface->push_back(this_AltSetting);

	      for (l = 0; l < this_AltSetting->numEndpoints(); l++) {
		this_Endpoint = new Endpoint;
		this_Endpoint->setDescriptor(dev->config[i].interface[j].altsetting[k].endpoint[l]);
		this_Endpoint->setParent(this_Device);
		this_AltSetting->push_back(this_Endpoint);
	      }
	    }
	  }
	}
      }
    }
  }

  std::list<Device *> Busses::match(u_int8_t class_code)
  {
    std::list<Device *> match_list;
    USB::Bus *bus;
    std::list<USB::Bus *>::const_iterator biter;

    for (biter = begin(); biter != end(); biter++) {
      USB::Device *device;
      std::list<USB::Device *>::const_iterator diter;
      bus = *biter;

      for (diter = bus->begin(); diter != bus->end(); diter++) {
        device = *diter;
	if (device->devClass() == class_code)
	  match_list.push_back(device);
      }
    }
    return match_list;
  }

  std::list<Device *> Busses::match(DeviceIDList devList)
  {
    std::list<Device *> match_list;
    USB::Bus *bus;
    std::list<USB::Bus *>::const_iterator biter;

    for (biter = begin(); biter != end(); biter++) {
      USB::Device *device;
      std::list<USB::Device *>::const_iterator diter;

      bus = *biter;
      for (diter = bus->begin(); diter != bus->end(); diter++) {
	DeviceIDList::iterator it;

        device = *diter;

	for (it = devList.begin(); it != devList.end(); it++) {
	  if (device->idVendor() == (*it).vendor() &&
	      device->idProduct() == (*it).product())
	    match_list.push_back(device);
	}
      }
    }
    return match_list;
  }

  std::string Bus::directoryName(void)
  {
    return m_directoryName;
  }

  void Bus::setDirectoryName(std::string directoryName)
  {
    m_directoryName = directoryName;
  }

  Device::~Device(void)
  {
    usb_close(m_handle);
  }

  std::string Device::fileName(void)
  {
    return m_fileName;
  }

  int Device::string(std::string &buf, int index, u_int16_t langID)
  {
    int retval;
    char tmpBuff[256];

    if (0 == langID) {
      /* we want the first lang ID available, so find out what it is */
      retval = usb_get_string(m_handle, 0, 0, tmpBuff, sizeof(tmpBuff));
      if (retval < 0)
	return retval;

      if (retval < 4 || tmpBuff[1] != USB_DT_STRING)
	return -EIO;

      langID = tmpBuff[2] | (tmpBuff[3] << 8);
    }

    retval = usb_get_string(m_handle, index, langID, tmpBuff, sizeof(tmpBuff));

    if (retval < 0)
      return retval;

    if (tmpBuff[1] != USB_DT_STRING)
      return -EIO;

    if (tmpBuff[0] > retval)
      return -EFBIG;

    /* FIXME: Handle unicode? */
#if 0
    if (retval > 0) {
      std::string.setUnicode((unsigned char *)&tmpBuff[2], tmpBuff[0] / 2 - 1);
    }
#endif
    return retval;
  }

  struct usb_dev_handle *Device::handle(void)
  {
    return m_handle;
  }

#ifdef USE_UNTESTED_LIBUSBPP_METHODS
  int Device::reset(void)
  {
    return usb_reset(handle());
  }

  int Device::setConfiguration(int configurationNumber)
  {
    return usb_set_configuration(handle(), configurationNumber);
  }
#endif /* USE_UNTESTED_LIBUSBPP_METHODS */

  u_int16_t Device::idVendor(void)
  {
    return m_descriptor.idVendor;
  }

  u_int16_t Device::idProduct(void)
  {
    return m_descriptor.idProduct;
  }

  u_int16_t Device::idRevision(void)
  {
    return m_descriptor.bcdDevice;
  }

  u_int8_t Device::devClass(void)
  {
    return m_descriptor.bDeviceClass;
  }

  u_int8_t Device::devSubClass(void)
  {
    return m_descriptor.bDeviceSubClass;
  }

  u_int8_t Device::devProtocol(void)
  {
    return m_descriptor.bDeviceProtocol;
  }

  std::string Device::Vendor(void)
  {
    return m_Vendor;
  }

  std::string Device::Product(void)
  {
    return m_Product;
  }

  std::string Device::SerialNumber(void)
  {
    return m_SerialNumber;
  }

  void Device::setVendor(std::string vendor)
  {
    m_Vendor = vendor;
  }

  void Device::setDevHandle(struct usb_dev_handle *device)
  {
    m_handle = device;
  }

  void Device::setProduct(std::string product)
  {
    m_Product = product;
  }

  void Device::setSerialNumber(std::string serialnumber)
  {
    m_SerialNumber = serialnumber;
  }

  u_int8_t Device::numConfigurations(void)
  {
    return m_descriptor.bNumConfigurations;
  }

  void Device::setFileName(std::string fileName)
  {
    m_fileName = fileName;
  }

  void Device::setDescriptor(struct usb_device_descriptor descriptor)
  {
    m_descriptor = descriptor;
  }

  Configuration *Device::firstConfiguration(void)
  {
    iter = begin();
    return *iter++;
  }

  Configuration *Device::nextConfiguration(void)
  {
    if (iter == end())
      return NULL;

    return *iter++;
  }

  Configuration *Device::lastConfiguration(void)
  {
    return back();
  }

  int Device::controlTransfer(u_int8_t requestType, u_int8_t request,
			       u_int16_t value, u_int16_t index, u_int16_t length,
			       unsigned char *payload, int timeout)
  {
    return usb_control_msg(m_handle, requestType, request, value, index, (char *)payload, length, timeout);
  }

  u_int8_t Configuration::numInterfaces(void)
  {
    return m_NumInterfaces;
  }

  void Configuration::setDescriptor(struct usb_config_descriptor descriptor)
  {
    m_Length = descriptor.bLength;
    m_DescriptorType = descriptor.bDescriptorType;
    m_TotalLength = descriptor.wTotalLength;
    m_NumInterfaces = descriptor.bNumInterfaces;
    m_ConfigurationValue = descriptor.bConfigurationValue;
    m_Configuration = descriptor.iConfiguration;
    m_Attributes = descriptor.bmAttributes;
    m_MaxPower = descriptor.MaxPower;
  }

  void Configuration::dumpDescriptor(void)
  {
    printf("  wTotalLength:         %d\n", m_TotalLength);
    printf("  bNumInterfaces:       %d\n", m_NumInterfaces);
    printf("  bConfigurationValue:  %d\n", m_ConfigurationValue);
    printf("  iConfiguration:       %d\n", m_Configuration);
    printf("  bmAttributes:         %02xh\n", m_Attributes);
    printf("  MaxPower:             %d\n", m_MaxPower);
  }

  Interface *Configuration::firstInterface(void)
  {
    iter = begin();
    return *iter++;
  }

  Interface *Configuration::nextInterface(void)
  {
    if (iter == end())
      return NULL;

    return *iter++;
  }

  Interface *Configuration::lastInterface(void)
  {
    return back();
  }

#ifdef LIBUSB_HAS_GET_DRIVER_NP
  int Interface::driverName(std::string &driver)
  {
    int retval;
    char tmpString[256];

    retval = usb_get_driver_np(m_parent->handle(), m_interfaceNumber, tmpString, sizeof(tmpString));
    if (retval == 0) {
      std::string buf(tmpString);

      driver = buf;
    }
    return retval;
  }
#endif

#ifdef USE_UNTESTED_LIBUSBPP_METHODS
  int Interface::claim(void)
  {
    return usb_claim_interface(m_parent->handle(), m_interfaceNumber);
  }

  int Interface::release(void)
  {
    return usb_claim_interface(m_parent->handle(), m_interfaceNumber);
  }

  int Interface::setAltSetting(int altSettingNumber)
  {
    return usb_set_altinterface(m_parent->handle(), altSettingNumber);
  }
#endif /* USE_UNTESTED_LIBUSBPP_METHODS */

  u_int8_t Interface::numAltSettings(void)
  {
    return m_numAltSettings;
  }

  void Interface::setNumAltSettings(u_int8_t num_altsetting)
  {
    m_numAltSettings = num_altsetting;
  }

  void Interface::setInterfaceNumber(int interfaceNumber)
  {
    m_interfaceNumber = interfaceNumber;
  }

  void Interface::setParent(Device *parent)
  {
    m_parent = parent;
  }

  AltSetting *Interface::firstAltSetting(void)
  {
    iter = begin();
    return *iter++;
  }

  AltSetting *Interface::nextAltSetting(void)
  {
    if (iter == end())
      return NULL;

    return *iter++;
  }

  AltSetting *Interface::lastAltSetting(void)
  {
    return back();
  }

  void AltSetting::setDescriptor(struct usb_interface_descriptor descriptor)
  {
    m_Length = descriptor.bLength;
    m_DescriptorType = descriptor.bDescriptorType;
    m_InterfaceNumber = descriptor.bInterfaceNumber;
    m_AlternateSetting = descriptor.bAlternateSetting;
    m_NumEndpoints = descriptor.bNumEndpoints;
    m_InterfaceClass = descriptor.bInterfaceClass;
    m_InterfaceSubClass = descriptor.bInterfaceSubClass;
    m_InterfaceProtocol = descriptor.bInterfaceProtocol;
    m_Interface = descriptor.iInterface;
  }

  void AltSetting::dumpDescriptor(void)
  {
    printf("    bInterfaceNumber:   %d\n", m_InterfaceNumber);
    printf("    bAlternateSetting:  %d\n", m_AlternateSetting);
    printf("    bNumEndpoints:      %d\n", m_NumEndpoints);
    printf("    bInterfaceClass:    %d\n", m_InterfaceClass);
    printf("    bInterfaceSubClass: %d\n", m_InterfaceSubClass);
    printf("    bInterfaceProtocol: %d\n", m_InterfaceProtocol);
    printf("    iInterface:         %d\n", m_Interface);
  }

  Endpoint *AltSetting::firstEndpoint(void)
  {
    iter = begin();
    return *iter++;
  }

  Endpoint *AltSetting::nextEndpoint(void)
  {
    if (iter == end())
      return NULL;

    return *iter++;
  }

  Endpoint *AltSetting::lastEndpoint(void)
  {
    return back();
  }

  u_int8_t AltSetting::numEndpoints(void)
  {
    return m_NumEndpoints;
  }

  void Endpoint::setDescriptor(struct usb_endpoint_descriptor descriptor)
  {
    m_EndpointAddress = descriptor.bEndpointAddress;
    m_Attributes = descriptor.bmAttributes;
    m_MaxPacketSize = descriptor.wMaxPacketSize;
    m_Interval = descriptor.bInterval;
    m_Refresh = descriptor.bRefresh;
    m_SynchAddress = descriptor.bSynchAddress;
  }

  void Endpoint::setParent(Device *parent)
  {
    m_parent = parent;
  }

#ifdef USE_UNTESTED_LIBUSBPP_METHODS
  int Endpoint::bulkWrite(unsigned char *message, int timeout)
  {
    return usb_bulk_write(m_parent->handle(), m_EndpointAddress, message.data(),
			  message.size(), timeout);
  }

  int Endpoint::bulkRead(int length, unsigned char *message, int timeout)
  {
    char *buf;
    int res;

    buf = (char *)malloc(length);
    res = usb_bulk_read(m_parent->handle(), m_EndpointAddress, buf, length, timeout);

    if (res > 0) {
      message.resize(length);
      message.duplicate(buf, res);
    }

    return res;
  }

  int Endpoint::reset(void)
  {
    return usb_resetep(m_parent->handle(), m_EndpointAddress);
  }

  int Endpoint::clearHalt(void)
  {
    return usb_clear_halt(m_parent->handle(), m_EndpointAddress);
  }

#endif /* USE_UNTESTED_LIBUSBPP_METHODS */

  void Endpoint::dumpDescriptor(void)
  {
    printf("      bEndpointAddress: %02xh\n", m_EndpointAddress);
    printf("      bmAttributes:     %02xh\n", m_Attributes);
    printf("      wMaxPacketSize:   %d\n", m_MaxPacketSize);
    printf("      bInterval:        %d\n", m_Interval);
    printf("      bRefresh:         %d\n", m_Refresh);
    printf("      bSynchAddress:    %d\n", m_SynchAddress);
  }

  DeviceID::DeviceID(u_int16_t vendor, u_int16_t product)
  {
    m_vendor = vendor;
    m_product = product;
  }

  u_int16_t DeviceID::vendor(void)
  {
    return m_vendor;
  }

  u_int16_t DeviceID::product(void)
  {
    return m_product;
  }
}


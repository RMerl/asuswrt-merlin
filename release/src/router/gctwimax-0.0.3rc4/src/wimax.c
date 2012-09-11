/*
 * This is a reverse-engineered driver for mobile WiMAX (802.16e) devices
 * based on GCT Semiconductor GDM7213 & GDM7205 chip.
 * Copyright (�) 2010 Yaroslav Levandovsky <leyarx@gmail.com>
 *
 * Based on  madWiMAX driver writed by Alexander Gordeev
 * Copyright (C) 2008-2009 Alexander Gordeev <lasaine@lvk.cs.msu.su>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>  //typedef
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h> // for mkdir
#include <sys/time.h>
#include <sys/wait.h>

#include <libusb/libusb.h>

#include "logging.h"
#include "protocol.h"
#include "wimax.h"
#include "tap_dev.h"

#include "eap_auth.h"

#ifdef WITH_DBUS
	#include <dbus/dbus.h>
#endif /* WITH_DBUS */

#include <openssl/sha.h>
#include <openssl/aes.h>
#include "zlib.h"
/* variables for the command-line parameters */

static int daemonize = 0;

struct timeval start_sec, curr_sec; // get link status every second

#define QUOTE(name) #name
#define STR(macro) QUOTE(macro)

#define CONF_DIR STR(CONFDIR)

static char *event_script = CONF_DIR"/event.sh";
static char *conf_file = "";
static FILE *logfile = NULL;
static FILE *xml_file = NULL;
char *pid_file = NULL;

#define MATCH_BY_LIST		0
#define MATCH_BY_VID_PID	1
#define MATCH_BY_BUS_DEV	2

//static int match_method = MATCH_BY_LIST;

/* for matching by list...
typedef struct usb_device_id_t {
	unsigned short vendorID;
	unsigned short productID;
	unsigned short targetVID;
	unsigned short targetPID;
} usb_device_id_t; */

/* list of all known devices 
static usb_device_id_t wimax_dev_ids[] = {
	{ 0x04e8, 0x6761 },
	{ 0x04e9, 0x6761 },
	{ 0x04e8, 0x6731 },
	{ 0x04e8, 0x6780 },
};*/

/* for other methods of matching... 
static union {
	struct {
		unsigned short vid;
		unsigned short pid;
	};
	struct {
		unsigned int bus;
		unsigned int dev;
	};
} match_params;*/

/* USB-related parameters */

#define IF_DVD			0

#define EP_IN			(130 | LIBUSB_ENDPOINT_IN)
#define EP_OUT			(1 | LIBUSB_ENDPOINT_OUT)

#define MAX_PACKET_LEN		0x4000

/* information collector */
static struct wimax_dev_status wd_status;

enum wimax_state {
	WS_DEV_NOT_FOUND = 1,	// - Device not found
	WS_DEV_FOUND,			//  - Device found
	WS_ERR_INIT_DRIVER,		// - Error init driver
	WS_RECEIVE_IP,			// - Receive ip-address (event script)
	WS_SCAN,				// - Search network
	WS_NET_NOT_FOUND,		// - Network not found
	WS_NET_FOUND,			// - Network found
	WS_CON_ERR,				// - Connection error
	WS_START_AUTH,			// - Start Authentication
	WS_AUTH_FAILED,			// - Authentication Failed
	WS_DL_XML_START,		// - Download XML image started
	WS_DL_XML_END,			// - Download XML image succeed
};

static
const struct {
	int value;
	const char *name;
} wimax_state_names_vals[] = {
	{ WS_DEV_NOT_FOUND, 	"Device not found" },
	{ WS_DEV_FOUND, 		"Device found" },
	{ WS_ERR_INIT_DRIVER, 	"Error init driver" },
	{ WS_RECEIVE_IP, 		"Receive ip-address (event script)" },
	{ WS_SCAN, 				"Search network" },
	{ WS_NET_NOT_FOUND, 	"No Signal" },
	{ WS_NET_FOUND, 		"Network found" },
	{ WS_CON_ERR, 			"Connection error" },
	{ WS_START_AUTH, 		"Start Authentication" },
	{ WS_AUTH_FAILED, 		"Authentication Failed" },
	{ WS_DL_XML_START, 		"Download XML image started" },
	{ WS_DL_XML_END, 		"Download XML image succeed" },
	{ 0, NULL }
};

const char * wimax_state_to_name(enum wimax_state state)
{
	size_t itr, max;
	
	for(max = 0;;max++)
		if (wimax_state_names_vals[max].name == NULL)
			break;
	
	for (itr = 0; itr < max; itr++)
		if (wimax_state_names_vals[itr].value == state)
			return wimax_state_names_vals[itr].name;
	return NULL;
}

//char *wimax_states[] = {"INIT", "SYNC", "NEGO", "NORMAL", "SLEEP", "IDLE", "HHO", "FBSS", "RESET", "RESERVED", "UNDEFINED", "BE", "NRTPS", "RTPS", "ERTPS", "UGS", "INITIAL_RNG", "BASIC", "PRIMARY", "SECONDARY", "MULTICAST", "NORMAL_MULTICAST", "SLEEP_MULTICAST", "IDLE_MULTICAST", "FRAG_BROADCAST", "BROADCAST", "MANAGEMENT", "TRANSPORT"};

/* libusb stuff */
static struct libusb_context *ctx = NULL;
static struct libusb_device_handle *devh = NULL;
static struct libusb_transfer *req_transfer = NULL;
static int kernel_driver_active = 0;

static unsigned char read_buffer[MAX_PACKET_LEN];

static int tap_fd = -1;
static char tap_dev[20] = "wimax%d";
static int tap_if_up = 0;

static nfds_t nfds;
static struct pollfd* fds = NULL;

static int device_disconnected = 0;

static void exit_release_resources(int code);
static void dbus_conn_info_send(enum wimax_state); // int signal_code
static void dbus_sig_info_send(void);
static void dbus_dev_info_send(void);
static void dbus_bsid_info_send(void);
static void dbus_percent_info_send(short, short);

#define CHECK_NEGATIVE(x) {if((r = (x)) < 0) return r;}
#define CHECK_DISCONNECTED(x) {if((r = (x)) == LIBUSB_ERROR_NO_DEVICE) exit_release_resources(0);}

int dbus_use = 0;

#ifdef WITH_DBUS
DBusConnection *dbus_connection;
DBusError dbus_error;
#endif /* WITH_DBUS */	

//DBusGConnection *dbus_connection;
//GError *dbus_error;
//DBusGProxy *dbus_proxy;
/*
static struct libusb_device_handle* find_wimax_device(void)
{
	struct libusb_device **devs;
	struct libusb_device *found = NULL;
	struct libusb_device *dev;
	struct libusb_device_handle *handle = NULL;
	int i = 0;
	int r;

	if (libusb_get_device_list(ctx, &devs) < 0)
		return NULL;

	while (!found && (dev = devs[i++]) != NULL) {
		struct libusb_device_descriptor desc;
		unsigned int j = 0;
		unsigned short dev_vid, dev_pid;

		r = libusb_get_device_descriptor(dev, &desc);
		if (r < 0) {
			continue;
		}
		dev_vid = libusb_le16_to_cpu(desc.idVendor);
		dev_pid = libusb_le16_to_cpu(desc.idProduct);
		wmlog_msg(1, "Bus %03d Device %03d: ID %04x:%04x", libusb_get_bus_number(dev), libusb_get_device_address(dev), dev_vid, dev_pid);
		switch (match_method) {
			case MATCH_BY_LIST: {
				for (j = 0; j < sizeof(wimax_dev_ids) / sizeof(usb_device_id_t); j++) {
					if (dev_vid == wimax_dev_ids[j].vendorID && dev_pid == wimax_dev_ids[j].productID) {
						found = dev;
						break;
					}
				}
				break;
			}
			case MATCH_BY_VID_PID: {
				if (dev_vid == match_params.vid && dev_pid == match_params.pid) {
					found = dev;
				}
				break;
			}
			case MATCH_BY_BUS_DEV: {
				if (libusb_get_bus_number(dev) == match_params.bus && libusb_get_device_address(dev) == match_params.dev) {
					found = dev;
				}
				break;
			}
		}
	}

	if (found) {
		r = libusb_open(found, &handle);
		if (r < 0)
			handle = NULL;
	}

	libusb_free_device_list(devs, 1);
	return handle;
}
*/
static int switch_wimax_device(void)
{
	struct libusb_device_handle *handle = NULL;
	int r;
	handle = libusb_open_device_with_vid_pid(ctx, 0x1076, 0x7f40);
	if (handle != NULL ){
		int disk_if = 0;
		
		if (libusb_kernel_driver_active(handle, disk_if) == 1)
		{
			r = libusb_detach_kernel_driver(handle, disk_if);
			if (r < 0){
				wmlog_msg(1, "Kernel driver detaching (error %d)", r);
			} else {
				wmlog_msg(1, "Kernel driver deteched!");
			}
		} else if (libusb_kernel_driver_active(handle, 1) == 1) 
		{
			r = libusb_detach_kernel_driver(handle, 1);
			disk_if = 1;
			if (r < 0){
				wmlog_msg(1, "Kernel driver detaching (error %d)", r);
			} else {
				wmlog_msg(1, "Kernel driver deteched!");
			}
		}
		
		r = libusb_claim_interface(handle, disk_if);
		if (r < 0) {
			disk_if = 1;
			r = libusb_claim_interface(handle, disk_if);
		}
		if (r < 0) {
			wmlog_msg(1, "Claim Interface problems (error %d)", r);
		}
		else
		{
			wmlog_msg(1, "Innterface claimed");	
				r = libusb_control_transfer(handle, 0xa1, 0xa0, 0, disk_if, read_buffer, 1, 1000);
			wmlog_msg(1, "Sending Control message (result %d - %s)", r, r ? "bad" : "ok");
				libusb_release_interface(handle, disk_if);
				libusb_close(handle);
		}
		return 1;
	}
	return 0;
}

// ***Edited by fanboy*** 
static struct libusb_device_handle* find_wimax_device(void)
{
	struct libusb_device_handle *handle = NULL;

	//Switch modem the same as in usb_modeswitch
	handle = libusb_open_device_with_vid_pid(ctx, 0x1076, 0x7f00);
	if (handle == NULL){
		if (switch_wimax_device()){
		
			int retry = 0;
			do
			{
				sleep(1); // Wait while device switching
				handle = libusb_open_device_with_vid_pid(ctx, 0x1076, 0x7f00);
			}
			while (retry++ < 5 && !handle);

			if (handle) wmlog_msg(2, "Device switched after %d retries.", retry);
			else  wmlog_msg(1, "Device not switched after %d retries.", retry);
		}
	}
	return handle;
}

static void get_wimax_device(void)
{
	int r;
	
	devh = find_wimax_device();
	if (devh == NULL) {
		if (dbus_use) dbus_conn_info_send(WS_DEV_NOT_FOUND);
		wmlog_msg(0, "Could not find/open device");
		exit_release_resources(1);
	}
	if (dbus_use) dbus_conn_info_send(WS_DEV_FOUND);
	wmlog_msg(0, "Device found");
	if (libusb_kernel_driver_active(devh, IF_DVD) == 1) {
		kernel_driver_active = 1;
		r = libusb_detach_kernel_driver(devh, IF_DVD);
		if (r < 0) {
			wmlog_msg(0, "kernel driver detach error %d", r);
		} else {
			wmlog_msg(0, "detached modem kernel driver");
		}
	}

	r = libusb_claim_interface(devh, 0);
	if (r < 0) {
		if (dbus_use) dbus_conn_info_send(WS_DEV_NOT_FOUND);
		wmlog_msg(0, "Claim usb interface error %d", r);
		exit_release_resources(1);
	}
	wmlog_msg(0, "Claimed interface");
}

static int set_data(unsigned char* data, int size)
{
	int r;
	int transferred;

	wmlog_dumphexasc(3, data, size, "Bulk write:");

	r = libusb_bulk_transfer(devh, EP_OUT, data, size, &transferred, 0);
	if (r < 0) {
		wmlog_msg(1, "bulk write error %d", r);
		if (r == LIBUSB_ERROR_NO_DEVICE) {
			if (dbus_use) dbus_conn_info_send(WS_DEV_NOT_FOUND);
			exit_release_resources(0);
		}
		return r;
	}
	if (transferred < size) {
		wmlog_msg(1, "short write (%d)", r);
		return -1;
	}
	return r;
}

void eap_server_rx(unsigned char *data, int data_len) // Sent data from peer to server
{
	unsigned char req_data[MAX_PACKET_LEN];
	int len;
	
	len = fill_eap_server_rx_req(req_data, data, data_len);
	set_data(req_data, len);
}

void eap_key(unsigned char *data, int data_len)
{
	unsigned char req_data[MAX_PACKET_LEN];
	int len;

	len = fill_eap_key_req(req_data, data, data_len);
	set_data(req_data, len);	
}

static void cb_req(struct libusb_transfer *transfer)
{
	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		wmlog_msg(1, "async bulk read error %d", transfer->status);
		if (transfer->status == LIBUSB_TRANSFER_NO_DEVICE) {
			device_disconnected = 1;
			return;
		}
	} else {
		wmlog_dumphexasc(3, transfer->buffer, transfer->actual_length, "Async read:");
		process_response(&wd_status, transfer->buffer, transfer->actual_length);
	}
	if (libusb_submit_transfer(req_transfer) < 0) {
		wmlog_msg(1, "async read transfer sumbit failed");
	}
}

/* get link_status *//*
int get_link_status()
{
	return wd_status.link_status;
}*/

/* set close-on-exec flag on the file descriptor */
int set_coe(int fd)
{
	int flags;

	flags = fcntl(fd, F_GETFD);
	if (flags == -1)
	{
		wmlog_msg(1, "failed to set close-on-exec flag on fd %d", fd);
		return -1;
	}
	flags |= FD_CLOEXEC;
	if (fcntl(fd, F_SETFD, flags) == -1)
	{
		wmlog_msg(1, "failed to set close-on-exec flag on fd %d", fd);
		return -1;
	}

	return 0;
}

/* run specified script */
static int raise_event(char *event)
{
	int pid = fork();

	if(pid < 0) { /* error */
		return -1;
	} else if (pid > 0) { /* parent */
		return pid;
	} else { /* child */
		char *args[] = {event_script, event, tap_dev, NULL};
		char *env[1] = {NULL};
		/* run the program */
		execve(args[0], args, env);
		exit(1);
	}
}

/* brings interface up and runs a user-supplied script */
static int if_create()
{
	tap_fd = tap_open(tap_dev);
	if (tap_fd < 0) {
		wmlog_msg(0, "failed to allocate tap interface");
		wmlog_msg(0,
				"You should have TUN/TAP driver compiled in the kernel or as a kernel module.\n"
				"If 'modprobe tun' doesn't help then recompile your kernel.");
		exit_release_resources(1);
	}
	tap_set_hwaddr(tap_fd, tap_dev, wd_status.mac);
	tap_set_mtu(tap_fd, tap_dev, 1386);
	set_coe(tap_fd);
	wmlog_msg(0, "Allocated tap interface: %s", tap_dev);
	wmlog_msg(2, "Starting if-create script...");
	raise_event("if-create");
	return 0;
}

/* brings interface up and runs a user-supplied script */
static int if_up()
{
	tap_bring_up(tap_fd, tap_dev);
	wmlog_msg(2, "Starting if-up script...");
	raise_event("if-up");
	tap_if_up = 1;
	if (dbus_use) {
		dbus_conn_info_send(WS_RECEIVE_IP);
		dbus_bsid_info_send();
	}
	return 0;
}

/* brings interface down and runs a user-supplied script */
static int if_down()
{
	if (!tap_if_up) return 0;
	tap_if_up = 0;
	wmlog_msg(2, "Starting if-down script...");
	raise_event("if-down");
	tap_bring_down(tap_fd, tap_dev);
	return 0;
}

/* brings interface down and runs a user-supplied script */
static int if_release()
{
	wmlog_msg(2, "Starting if-release script...");
	raise_event("if-release");
	tap_close(tap_fd, tap_dev);
	return 0;
}

/* set link_status *//*
void set_link_status(int link_status)
{
	wd_status.info_updated |= WDS_LINK_STATUS;

	if (wd_status.link_status == link_status) return;

	if (wd_status.link_status < 2 && link_status == 2) {
		if_up();
	}
	if (wd_status.link_status == 2 && link_status < 2) {
		if_down();
	}
	if (link_status == 1) {
		first_nego_flag = 1;
	}

	wd_status.link_status = link_status;
}*/

/* get state *//*
int get_state()
{
	return wd_status.state;
}*/

/* set state *//*
void set_state(int state)
{
	wd_status.state = state;
	wd_status.info_updated |= WDS_STATE;
	if (state >= 1 && state <= 3 && wd_status.link_status != (state - 1)) {
		set_link_status(state - 1);
	}
}*/

static int alloc_transfers(void)
{
	req_transfer = libusb_alloc_transfer(0);
	if (!req_transfer)
		return -ENOMEM;

	libusb_fill_bulk_transfer(req_transfer, devh, EP_IN, read_buffer,
		sizeof(read_buffer), cb_req, NULL, 0);

	return 0;
}

int write_netif(const void *buf, int count)
{
	return tap_write(tap_fd, buf, count);
}

static int read_tap()
{
	unsigned char buf[MAX_PACKET_LEN];
	int hlen = get_header_len();
	int r;
	int len;

	r = tap_read(tap_fd, buf + hlen, MAX_PACKET_LEN - hlen);

	if (r < 0)
	{
		wmlog_msg(1, "Error while reading from TAP interface");
		return r;
	}

	if (r == 0)
	{
		return 0;
	}

	len = fill_data_packet_header(buf, r);
	wmlog_dumphexasc(4, buf, len, "Outgoing packet:");
	r = set_data(buf, len);

	return r;
}

static int process_events_once(int timeout)
{
	struct timeval tv = {0, 0};
	int r;
	int libusb_delay;
	int delay;
	unsigned int i;
	char process_libusb = 0;

	r = libusb_get_next_timeout(ctx, &tv);
	if (r == 1 && tv.tv_sec == 0 && tv.tv_usec == 0)
	{
		r = libusb_handle_events_timeout(ctx, &tv);
	}

	delay = libusb_delay = tv.tv_sec * 1000 + tv.tv_usec;
	if (delay <= 0 || delay > timeout)
	{
		delay = timeout;
	}

	CHECK_NEGATIVE(poll(fds, nfds, delay));

	process_libusb = (r == 0 && delay == libusb_delay);

	for (i = 0; i < nfds; ++i)
	{
		if (fds[i].fd == tap_fd) {
			if (fds[i].revents)
			{
				CHECK_NEGATIVE(read_tap());
			}
			continue;
		}
		process_libusb |= fds[i].revents;
	}

	if (process_libusb)
	{
		struct timeval tv = {.tv_sec = 0, .tv_usec = 0};
		CHECK_NEGATIVE(libusb_handle_events_timeout(ctx, &tv));
	}

	return 0;
}

/* handle events until timeout is reached or all of the events in event_mask happen */
static int process_events_by_mask(int timeout, unsigned int event_mask)
{
	struct timeval start, curr;
	int r;
	int delay = timeout;

	CHECK_NEGATIVE(gettimeofday(&start, NULL));

	wd_status.info_updated &= ~event_mask;
	
	while ((event_mask == 0 || (wd_status.info_updated & event_mask) != event_mask) && delay >= 0) {
		long a;
		
		CHECK_NEGATIVE(process_events_once(delay));

		if (device_disconnected) {
			if (dbus_use) dbus_conn_info_send(WS_DEV_NOT_FOUND);
			exit_release_resources(0);
		}

		CHECK_NEGATIVE(gettimeofday(&curr, NULL));

		a = (curr.tv_sec - start.tv_sec) * 1000 + (curr.tv_usec - start.tv_usec) / 1000;
		delay = timeout - a;
	}

	wd_status.info_updated &= ~event_mask;

	return (delay > 0) ? delay : 0;
}

int alloc_fds()
{
	int i;
	const struct libusb_pollfd **usb_fds = libusb_get_pollfds(ctx);

	if (!usb_fds)
	{
		return -1;
	}

	nfds = 0;
	while (usb_fds[nfds])
	{
		nfds++;
	}
	if (tap_fd != -1) {
		nfds++;
	}

	if(fds != NULL) {
		free(fds);
	}

	fds = (struct pollfd*)calloc(nfds, sizeof(struct pollfd));
	for (i = 0; usb_fds[i]; ++i)
	{
		fds[i].fd = usb_fds[i]->fd;
		fds[i].events = usb_fds[i]->events;
		set_coe(usb_fds[i]->fd);
	}
	if (tap_fd != -1) {
		fds[i].fd = tap_fd;
		fds[i].events = POLLIN;
		fds[i].revents = 0;
	}

	free(usb_fds);

	return 0;
}

void cb_add_pollfd(int fd, short events, void *user_data)
{
	alloc_fds();
}

void cb_remove_pollfd(int fd, void *user_data)
{
	alloc_fds();
}

static void dm_cmd(char *cmd)
{
	unsigned char req_data[MAX_PACKET_LEN];
	int len;
	len = fill_dm_cmd_req(req_data, cmd);
	set_data(req_data, len);
}

static void DL_xml_image()
{					
	unsigned char req_data[MAX_PACKET_LEN];
	unsigned char data[0x400];
	int len, data_len, offset, size;
	int r;
					
	r = libusb_init(&ctx);
	if (r < 0) {
		wmlog_msg(0, "failed to initialise libusb");
		exit(1);
	}
 		
	get_wimax_device();
	alloc_transfers();

	wmlog_msg(2, "Continuous async read start...");
	CHECK_DISCONNECTED(libusb_submit_transfer(req_transfer));
	
	fseek(xml_file, 0L, SEEK_END);
	size = ftell(xml_file);
	wmlog_msg(0, "XML Image size: %d",size);
	fseek(xml_file, 0L, SEEK_SET);

	if (dbus_use) dbus_conn_info_send(WS_DL_XML_START);
	offset = 0;
	while (offset != 0xffffffff)
	{
		data_len = fread(data, 1, 0x400, xml_file);
		if (data_len == 0)
		{
			if (!feof(xml_file))
				wmlog_msg(0, "bad EOF");
			offset = 0xffffffff;
			if (dbus_use) dbus_percent_info_send(0x0100, 100);
		}
		else
		{
			if (dbus_use) dbus_percent_info_send(0x0100, offset * 100 / size);
		}
		len = fill_image_dl_req(req_data, 0x0100, offset, data, data_len);
//		wmlog_msg(0, "percent %d",offset * 100 / size);
		offset += data_len;
		set_data(req_data, len);
		process_events_by_mask(500, WDS_CERT);					
	}	
	if (dbus_use) dbus_conn_info_send(WS_DL_XML_END);
	fclose(xml_file);
	
	dm_cmd("reboot");
	
	exit_release_resources(1);
}


static
void AES_cbc_decrypt(uint8_t *shakey, uint8_t *inbuf, uint8_t *outbuf)
{
	unsigned char iv[16] = {0x43,0x6c,0x61,0x72,0x6b,0x4a,0x4a,0x61,0x6e,0x67,0x00,0x00,0x00,0x00,0x00,0x00}; //ClarkJJang

	AES_KEY aeskey;	

	AES_set_decrypt_key(shakey, 24*8, &aeskey); //192

	AES_cbc_encrypt(inbuf, outbuf, 16, &aeskey, iv, AES_DECRYPT);
}


static
int decrypt_data(unsigned char *buf, unsigned char *outbuf, int len)
{
	int i, dec_buf_len;
	uint8_t key_sha[0x18];
	uint8_t dec_buf[len];
	
	memset(key_sha,0x00,sizeof(key_sha));
	memset(dec_buf,0x00,sizeof(dec_buf));
	
	SHA1(wd_status.mac, 6, key_sha);

	for (i = 0; i < (len / 16); i++)
		AES_cbc_decrypt(key_sha, buf + 16 * i, dec_buf + 16 * i);	
	dec_buf_len = (dec_buf[0] << 24) + (dec_buf[1] << 16) + (dec_buf[2] << 8) + dec_buf[3];
	memcpy(outbuf, dec_buf + 4, dec_buf_len);
	
	wmlog_msg(2, "Decrypted size is %d bytes.", dec_buf_len);	
	return dec_buf_len;
}


static 
int extract_cert(unsigned char *buf, int len, char *path)
{
	int dec_buf_len, gz_len;
	char gz_path[0x100]; //, pem_path[0x100]
	uint8_t dec_buf[len], gz_buf[MAX_PACKET_LEN];
	FILE *fp;
	voidp gz;
	
	memset(dec_buf,0x00,sizeof(dec_buf));

	dec_buf_len = decrypt_data(buf,  dec_buf, len);
	wmlog_msg(2, "Cert decrypted");
	
	sprintf(gz_path,"%s.gz", path);
	fp=fopen((char *)gz_path, "wb");
	if (fp == NULL)
		return errno;
	fwrite(dec_buf, 1, dec_buf_len, fp); //dec_buf + 4
	fclose(fp);

	gz = gzopen(gz_path, "rb"); 
	if (gz == NULL)
		return errno;
	gz_len = gzread(gz, gz_buf, sizeof(gz_buf));
	gzclose(gz);
	remove(gz_path);
	
	wmlog_msg(2, "Extracted cert size is %d bytes.", gz_len);
	
		fp=fopen((char *)path, "ab");
		if (fp == NULL)
			return errno;
		fwrite(gz_buf,1,gz_len,fp);
		fclose(fp);
		
	return 0;
}


static
int get_nv_data(short code, unsigned char *buf)
{
	unsigned char req_data[MAX_PACKET_LEN];
	int len, data_len, buf_len = 0;
	
	memset(wd_status.cert, 0, sizeof(wd_status.cert));
	
	len = fill_get_data_req_start(req_data, code);
	set_data(req_data, len);
	
	process_events_by_mask(2000, WDS_CERT);
	
	while(1)
	{
		if( ((wd_status.cert[0] << 8) + wd_status.cert[1]) ==  code )
		{
			if (wd_status.cert[2] != 0xff)
			{
				data_len = (wd_status.cert_buf[0] << 8) + wd_status.cert_buf[1];
						
				memcpy(buf+buf_len,wd_status.cert_buf+2,data_len);
				buf_len += data_len;

				len = fill_get_data_req_step(req_data, code, buf_len);
				set_data(req_data, len);
	
				process_events_by_mask(2000, WDS_CERT);
			}
			else break;
		}
		else
			break;
	}

	return buf_len;
}


static
void get_cert()
{
	int r, i;
	char path[0x100];
	sprintf(path,CONF_DIR"/%02x%02x%02x%02x%02x%02x", wd_status.mac[0], wd_status.mac[1], \
												wd_status.mac[2], wd_status.mac[3], wd_status.mac[4], wd_status.mac[5]);

	r = mkdir(path,S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (r)
	{
		if (errno != EEXIST) //17
			return;
	}
												
	for (i = 0x0101; i <= 0x0106; i++)
	{
		if (	(i == 0x0105 && wd_status.config->use_nv == 1) ||
				(i == 0x0101 && wd_status.config->cert_nv == 1 && wd_status.config->dev_cert_null == 0) ||
				(i != 0x0101 && i != 0x0105 && wd_status.config->cert_nv == 1 && wd_status.config->ca_cert_null == 0)	)
		{
			unsigned char data[MAX_PACKET_LEN];
			int data_len;
			data_len = get_nv_data(i, data);
			if (data_len != 0)
			{
			
			switch (i) {
				case 0x0101:
				{
					free(wd_status.config->client_cert);
					free(wd_status.config->private_key);
					wd_status.config->client_cert = NULL;
					wd_status.config->private_key = NULL;
					wd_status.config->client_cert = (unsigned char *)malloc(strlen(path)+13);
					sprintf((char *)wd_status.config->client_cert,"%s/client_cert.pem",path);
					wd_status.config->private_key = (unsigned char *)strdup((char *)wd_status.config->client_cert);
					r = extract_cert(data, data_len, (char *)wd_status.config->client_cert);
					if(r) {
						free(wd_status.config->client_cert);
						free(wd_status.config->private_key);
						wd_status.config->client_cert = NULL;
						wd_status.config->private_key = NULL;						
					}	
					break;
				}				
					break;
				case 0x0102:
				case 0x0103:
				case 0x0104:
				case 0x0106:
				{
					free(wd_status.config->ca_cert);
					wd_status.config->ca_cert = NULL;
					wd_status.config->ca_cert = (unsigned char *)malloc(strlen(path)+13);
					sprintf((char *)wd_status.config->ca_cert,"%s/root_ca.pem",path);
					r = extract_cert(data, data_len, (char *)wd_status.config->ca_cert);
					if(r) {
						free(wd_status.config->ca_cert);
						wd_status.config->ca_cert = NULL;					
					}	
					break;
				}
				case 0x0105:
				{
					data_len = decrypt_data(data, data, data_len);
					if(data_len) {
						int j;
						for (j=0; j < data_len;)
						{
							int id, id_len;
							id = data[j];
							id_len = (data[j+1] << 8) + data[j+2];
							if(id_len)
							{
								char str[id_len+1];
								str[id_len] = '\0';
								memcpy(str, data+j+3, id_len);
//							wmlog_msg(2, "id: %d String: %s", id, str);							
								switch (id) {
									case 0x01:
										free(wd_status.config->anonymous_identity);
										wd_status.config->anonymous_identity = NULL;
										wd_status.config->anonymous_identity = (unsigned char *)strdup(str);
										wd_status.config->anonymous_identity_len = id_len;
										break;
									case 0x02:
										if (wd_status.config->cert_nv == 1 && 
												wd_status.config->dev_cert_null == 0) {
											free(wd_status.config->private_key_passwd);
											wd_status.config->private_key_passwd = NULL;
											wd_status.config->private_key_passwd = (unsigned char *)strdup(str);
										}
										break;							
									case 0x03:
										free(wd_status.config->identity);
										wd_status.config->identity = NULL;
										wd_status.config->identity = (unsigned char *)strdup(str);
										wd_status.config->identity_len = id_len;
										break;
									case 0x04:
										free(wd_status.config->password);
										wd_status.config->password = NULL;
										wd_status.config->password = (unsigned char *)strdup(str);
										wd_status.config->password_len = id_len;									
										break;
									default:
										wmlog_msg(2, "Unknown EAP param from NV # %02x", id);
										break;
								}
							}
							j += id_len + 3;
						}
					}
					break;
				}
			}
			
			}
		}
	}
}


static int init(void)
{
	unsigned char req_data[MAX_PACKET_LEN];
	int len;
	int r;

	alloc_transfers();

	wmlog_msg(2, "Continuous async read start...");
	CHECK_DISCONNECTED(libusb_submit_transfer(req_transfer));

	len = fill_rf_off_req(req_data);
	set_data(req_data, len);
	
	process_events_by_mask(2000, WDS_RF_STATE);
	
	len = fill_debug_req(req_data);  //mode normal or test
	set_data(req_data, len);
		
	len = fill_string_info_req(req_data);
	set_data(req_data, len);

	process_events_by_mask(500, WDS_CHIP | WDS_FIRMWARE);
	
	wmlog_msg(1, "Chip info: %s", wd_status.chip);
	wmlog_msg(1, "Firmware info: %s", wd_status.firmware);
	
	if(wd_status.nspid == 0) {
		len = fill_get_nspid_req(req_data);
		set_data(req_data, len);
		process_events_by_mask(500, WDS_NSPID);
	}	
	wmlog_msg(1, "H-NSPID: 0x%06x", wd_status.nspid);
	
	len = fill_mac_req(req_data);
	set_data(req_data, len);

	process_events_by_mask(500, WDS_MAC);

	wmlog_msg(1, "MAC: %02x:%02x:%02x:%02x:%02x:%02x", wd_status.mac[0], wd_status.mac[1], wd_status.mac[2], wd_status.mac[3], wd_status.mac[4], wd_status.mac[5]);
	
	if (dbus_use) dbus_dev_info_send();
	
	len = fill_rf_on_req(req_data);
	set_data(req_data, len);

	process_events_by_mask(20000, WDS_RF_STATE);

	wmlog_msg(2, "RF ON.");

	get_cert();
	
	return 0;
}

static int scan_loop(void)
{
	unsigned char req_data[MAX_PACKET_LEN];
	int len;
	int r;
	
	while (1)
	{
		if (wd_status.link_status == 0) {

			if_down();

			if (wd_status.config->use_pkm == 1){
				len = fill_auth_on_req(req_data);
				set_data(req_data, len);
			}
			
			if (dbus_use && wd_status.network_status == 0) dbus_conn_info_send(WS_SCAN);
			wmlog_msg(0, "Search network...");		
			len = fill_find_network_req(req_data, &wd_status);
			set_data(req_data, len);

			while (process_events_by_mask(60000, WDS_LINK_STATUS) < 0) {;}
			
			if (wd_status.link_status == 0) {
				if (dbus_use && wd_status.network_status == 0) dbus_conn_info_send(WS_NET_NOT_FOUND);
				wmlog_msg(0, "Network not found.");
				wd_status.network_status--;
				if (wd_status.network_status < 60)
					sleep(30);
				else if (wd_status.network_status < 30)
					sleep(15);
				else if (wd_status.network_status < 10)
					sleep(5);
			} else {
				if (dbus_use) dbus_conn_info_send(WS_NET_FOUND);
				wmlog_msg(0, "Network found.");
					wd_status.link_status = 0;
					wd_status.network_status = 0;			
				len = fill_connect_req(req_data, &wd_status);
				set_data(req_data, len);

				while (process_events_by_mask(100000, WDS_LINK_STATUS) < 0) {;}
				
				if (wd_status.link_status == 0) {
					if (dbus_use) dbus_conn_info_send(WS_CON_ERR);
					wmlog_msg(0, "Connection error.");
				} else {
					wmlog_msg(0, "Connected to Network.");
					
					wd_status.auth_info = 0; //if more then one BS found.
					r = 1;
					while (	1 ){
						process_events_by_mask(5000, WDS_AUTH_STATE);
						wmlog_msg(2, "Auth_state: %06x",wd_status.auth_state);
						
						switch (wd_status.auth_state) {
							case 0x0001ff:
							case 0x0100ff:
							case 0x0101ff:
							case 0x0200ff:
							case 0x0300ff:
							case 0x0301ff:
							case 0x0400ff:
							case 0x0401ff: {
								process_events_by_mask(3000, WDS_LINK_STATUS);
								goto auth_loop_done;
							}
							
							case 0x020000: {
								if (dbus_use) dbus_conn_info_send(WS_START_AUTH);
								wmlog_msg(0, "Start Authentication.");
								if (eap_peer_init(wd_status.config)< 0) {
									wmlog_msg(0, "EAP Peer init error.");
									break;
								}
								r = 1;
								wd_status.info_updated = WDS_NONE;
								while( r > 0)
								{
									if (process_events_by_mask(100000, WDS_EAP))
										r = eap_peer_step();
									else
										r = 0;
								}
								eap_peer_deinit();
								break;
							}
							case 0x0201ff: {
								if (dbus_use) dbus_conn_info_send(WS_AUTH_FAILED);
								if (wd_status.auth_info == 0){
									wmlog_msg(0, "Authentication Failed. Renewing Authentication.");
								}
								else {
									wmlog_msg(0, "Authentication Failed.");
									goto auth_loop_done;
								}
								break;
							}
							case 0x020100: {
								wmlog_msg(0, "Authentication Succeed.");
								break;
							}
							case 0x040100: {
								if_up();
								process_events_by_mask(2000, WDS_LINK_STATUS);	
								process_events_by_mask(2000, WDS_LINK_STATUS);
								goto auth_loop_done;
							}
							default: {
								break;
							}
						}
					}
auth_loop_done:;
				}
			}
				
		} 
		else { 

				len = fill_connection_params_req(req_data);
				set_data(req_data, len);

				process_events_by_mask(500, WDS_RSSI1 | WDS_CINR1 | WDS_RSSI2 | WDS_CINR2 | WDS_TXPWR | WDS_FREQ);

				wmlog_msg(1, "RSSI1: %d   CINR1: %d   TX Power: %d   Frequency: %d", 
								wd_status.rssi1, wd_status.cinr1, wd_status.txpwr, wd_status.freq);
				wmlog_msg(1, "RSSI2: %d   CINR2: %d", 
								wd_status.rssi2, wd_status.cinr2);

				if (dbus_use) dbus_sig_info_send();
			
				process_events_by_mask(5000, WDS_LINK_STATUS);
		}
	}

	return 0;
}

/* print usage information */
void usage(const char *progname)
{
	printf("Usage: %s [options]\n", progname);
	printf("Options:\n");
	printf("  -C, --cfg-file=FILE         path to configuration file\n");
	printf("      --dl-xml=FILE           DL XML Image & exit\n");
	printf("  -D,                         Switch device & exit\n");
	printf("  -v,                         verbose log level (even more -vv)\n");
	printf("  -p, --pid-file=FILE         PID file (only for daemonize mode)\n");
	printf("  -d, --daemonize             daemonize after startup\n");
	printf("  -V, --version               print the version number\n");
#ifdef WITH_DBUS
	printf("      --with-dbus             Run with dbus support\n");
#endif
	printf("  -h, --help                  display this help\n");
}

/* print version */
void version()
{
	printf("%s %s\n", "GCTwimax", get_wimax_version());
}

static void parse_args(int argc, char **argv)
{
	while (1)
	{
		int c;
		/* getopt_long stores the option index here. */
		int option_index = 0;
		static struct option long_options[] =
		{
			{"cfg-file",	required_argument, 	0, 'C'},
			{"dl-xml",		required_argument, 	0, 8},
			{"daemonize",	no_argument,		0, 'd'},
			{"pid-file",	required_argument, 	0, 'p'},
			{"version",		no_argument,		0, 'V'},
#ifdef WITH_DBUS
			{"with-dbus",	no_argument,		0, 7},
#endif /* WITH_DBUS */				
			{"help",		no_argument,		0, 'h'},
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "C:p:vDdVh", long_options, &option_index);
		/* detect the end of the options. */
		if (c == -1)
			break;

		switch (c)
		{
			case 'C': {	
					conf_file = optarg;
					break;				
				}
			case 'p': {	
					pid_file = rel2abs_path(optarg);
					break;				
				}
			case 'v': {
					inc_wmlog_level();
					break;
				}
			case 'D': {	
					int r;
					r = libusb_init(&ctx);
					if (r < 0) {
						wmlog_msg(0, "failed to initialise libusb");
						exit(1);
					}
					switch_wimax_device();
					libusb_exit(ctx);
					exit(1);
					break;				
				}					
			case 'd': {
					daemonize = 1;
					break;
				}
			case 'V': {
					version();
					exit(0);
					break;
				}
			case 'h': {
					usage(argv[0]);
					exit(0);
					break;
				}
			case '?': {
					/* getopt_long already printed an error message. */
					usage(argv[0]);
					exit(1);
					break;
				}
#ifdef WITH_DBUS
			case 7: {
					dbus_use = 1;
					break;
				}				
#endif /* WITH_DBUS */	
			case 8: {
					xml_file = fopen(optarg, "rb");
					if (xml_file == NULL){
						wmlog_msg(0, "bad xml file: %s",strerror(errno));
						exit(1);
					}
					break;	
				}
			default: {
					exit(1);
				}
		}
	}
}

static void exit_release_resources(int code)
{	
//Rewrite this part!!!!!!!
	if(devh != NULL && wd_status.rf_state == 0){
		unsigned char req_data[MAX_PACKET_LEN];
		int len;
		wmlog_msg(2, "RF OFF.");
		len = fill_rf_off_req(req_data);
		set_data(req_data, len);
		process_events_by_mask(2000, WDS_RF_STATE);
	}
	if(wd_status.config != NULL && wd_status.config->cert_nv == 1)
	{
		if(wd_status.config->ca_cert)
			remove((const char *)wd_status.config->ca_cert); //Delete this
		if(wd_status.config->client_cert)
			remove((const char *)wd_status.config->client_cert);
	}
	if(tap_fd >= 0) {
		if_down();
		while (wait(NULL) > 0) {}
		if_release();
		while (wait(NULL) > 0) {}
	}
	if(ctx != NULL) {
		if(req_transfer != NULL) {
			libusb_cancel_transfer(req_transfer);
			libusb_free_transfer(req_transfer);
		}
		libusb_set_pollfd_notifiers(ctx, NULL, NULL, NULL);
		if(fds != NULL) {
			free(fds);
		}
		if(devh != NULL) {
			libusb_release_interface(devh, 0);
			if (kernel_driver_active)
				libusb_attach_kernel_driver(devh, 0);
			libusb_unlock_events(ctx);
			libusb_close(devh);
		}
		libusb_exit(ctx);
	}
	if (pid_file)
	{
		unlink(pid_file);
		free(pid_file);
	}
	if(logfile != NULL) {
		fclose(logfile);
	}
	if (dbus_use) {
#ifdef WITH_DBUS
		DBusMessage *message;
		message = dbus_message_new_signal (	"/ua/org/yarx/Daemon",
											"ua.org.yarx.Daemon", 
											"ErrorAndDisconnect");	
		dbus_message_append_args (	message,
									DBUS_TYPE_INVALID);
		dbus_connection_send (dbus_connection, message, NULL);
		dbus_message_unref (message);
#endif /* WITH_DBUS */	
	}
	if(wd_status.config)
		gct_config_free(wd_status.config);
	exit(code);
}

static void sighandler_exit(int signum) {
	exit_release_resources(0);
}

static void sighandler_wait_child(int signum) {
	int status;
	wait3(&status, WNOHANG, NULL);
	wmlog_msg(2, "Child exited with status %d", status);
}

////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

static void dbus_dev_info_send(void) {
#ifdef WITH_DBUS
	const char *chip = wd_status.chip;
	const char *firmware = wd_status.firmware;
	char _mac[18];

	snprintf(_mac, 18, "%02x:%02x:%02x:%02x:%02x:%02x", wd_status.mac[0], wd_status.mac[1], wd_status.mac[2],
							wd_status.mac[3], wd_status.mac[4], wd_status.mac[5]);
							
	const char * mac = _mac;
	
	DBusMessage *message;
	message = dbus_message_new_signal (	"/ua/org/yarx/Daemon",
										"ua.org.yarx.Daemon", 
										"SendDeviceInfo");	
	dbus_message_append_args (	message,
								DBUS_TYPE_STRING, &chip,
								DBUS_TYPE_STRING, &firmware,
								DBUS_TYPE_STRING, &mac,
								DBUS_TYPE_INVALID);
	dbus_connection_send (dbus_connection, message, NULL);
	dbus_message_unref (message);
#endif /* WITH_DBUS */	
}

static void dbus_sig_info_send(void) {
#ifdef WITH_DBUS
	DBusMessage *message;
	message = dbus_message_new_signal (	"/ua/org/yarx/Daemon",
										"ua.org.yarx.Daemon", 
										"SendSignalInfo");	
	dbus_message_append_args (	message,
								DBUS_TYPE_INT16, &wd_status.rssi1,
								DBUS_TYPE_INT16, &wd_status.rssi2,
								DBUS_TYPE_INT16, &wd_status.cinr1,
								DBUS_TYPE_INT16, &wd_status.cinr2,
								DBUS_TYPE_UINT16, &wd_status.txpwr,
								DBUS_TYPE_UINT32, &wd_status.freq,
								DBUS_TYPE_INVALID);
	dbus_connection_send (dbus_connection, message, NULL);
	dbus_message_unref (message);
#endif /* WITH_DBUS */
}

static void dbus_bsid_info_send(void) {
#ifdef WITH_DBUS
	char _bsid[18];
	snprintf(_bsid, 18, "%02x:%02x:%02x:%02x:%02x:%02x", wd_status.bsid[0], wd_status.bsid[1], wd_status.bsid[2],
							wd_status.bsid[3], wd_status.bsid[4], wd_status.bsid[5]);
							
	const char *bsid = _bsid;
	
	DBusMessage *message;
	message = dbus_message_new_signal (	"/ua/org/yarx/Daemon",
										"ua.org.yarx.Daemon", 
										"SendBsidInfo");	
	dbus_message_append_args (	message,
								DBUS_TYPE_STRING, &bsid,
								DBUS_TYPE_INVALID );
	dbus_connection_send (dbus_connection, message, NULL);
	dbus_message_unref (message);
#endif /* WITH_DBUS */
}

static void dbus_conn_info_send(enum wimax_state state) {
#ifdef WITH_DBUS
	DBusMessage *message;
	message = dbus_message_new_signal (	"/ua/org/yarx/Daemon",
										"ua.org.yarx.Daemon", 
										"SendConnectionInfo");

	const char *msg_state = wimax_state_to_name(state);	
	dbus_message_append_args (	message,
								DBUS_TYPE_INT16, &state,
								DBUS_TYPE_STRING, &msg_state,
								DBUS_TYPE_INVALID );
	dbus_connection_send (dbus_connection, message, NULL);

	dbus_message_unref (message);
#endif /* WITH_DBUS */
}


static void dbus_percent_info_send(short code, short percent) {
#ifdef WITH_DBUS
	DBusMessage *message;
	message = dbus_message_new_signal (	"/ua/org/yarx/Daemon",
										"ua.org.yarx.Daemon", 
										"SendPercentInfo");
	dbus_message_append_args (	message,
								DBUS_TYPE_INT16, &code,
								DBUS_TYPE_INT16, &percent,
								DBUS_TYPE_INVALID );										
	dbus_connection_send (dbus_connection, message, NULL);

	dbus_message_unref (message);
#endif /* WITH_DBUS */
}
////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

static
void gct_config_complete()
{
	struct gct_config *config = wd_status.config;
	
	if(wimax_log_level < config->wimax_verbose_level)
		set_wmlog_level(config->wimax_verbose_level);
		
	if (strlen(config->log_file))
		logfile = fopen(config->log_file, "a");
	
	if (strlen(config->event_script))
		event_script = config->event_script;
	
	wd_status.nspid = config->nspid;

	if(config->use_pkm)
	{
		if(config->ca_cert_null)
		{
			free(config->ca_cert);
			config->ca_cert = NULL;
		}
		
		if(config->dev_cert_null)
		{
			free(config->client_cert);
			free(config->private_key);
			free(config->private_key_passwd);
			config->client_cert = NULL;
			config->private_key = NULL;
			config->private_key_passwd = NULL;			
		}
		
		switch(config->eap_type)
		{
			case -1:
				/* Use settings only from config file*/
				break;
			case 3:
				free(config->eap_methods);
				free(config->identity);
				free(config->password);
				free(config->phase1);
				free(config->phase2);
				config->identity = NULL;
				config->password = NULL;
				config->identity_len = 0;
				config->password_len = 0;	
				config->eap_methods = calloc(2, sizeof(*config->eap_methods));
				config->eap_methods[0].vendor = EAP_VENDOR_IETF;
				config->eap_methods[0].method = EAP_TYPE_TLS;
				config->phase1 = strdup("include_tls_length=1");
				config->phase2 = NULL;
				break;			
			case 4:
				free(config->eap_methods);
				free(config->phase1);
				free(config->phase2);
				config->eap_methods = calloc(2, sizeof(*config->eap_methods));
				config->eap_methods[0].vendor = EAP_VENDOR_IETF;
				config->eap_methods[0].method = EAP_TYPE_TTLS;
				config->phase1 = NULL;
				config->phase2 = strdup("auth=MD5");
				break;			
			case 5:
				free(config->eap_methods);
				free(config->phase1);
				free(config->phase2);
				config->eap_methods = calloc(2, sizeof(*config->eap_methods));
				config->eap_methods[0].vendor = EAP_VENDOR_IETF;
				config->eap_methods[0].method = EAP_TYPE_TTLS;			
				config->phase1 = NULL;
				config->phase2 = strdup("auth=MSCHAPV2");
				break;
			case 6:
				free(config->eap_methods);
				free(config->phase1);
				free(config->phase2);
				config->eap_methods = calloc(2, sizeof(*config->eap_methods));
				config->eap_methods[0].vendor = EAP_VENDOR_IETF;
				config->eap_methods[0].method = EAP_TYPE_TTLS;	
				config->phase1 = NULL;
				config->phase2 = strdup("auth=CHAP");
				break;			
			default:
				wmlog_msg(0,"E:	EAP Type (%d) not supported yet",
					config->eap_type);
		}		
	}
}
	

int main(int argc, char **argv)
{
	struct sigaction sigact;
	int r = 1;
	
	set_wmlogger(argv[0], WMLOGGER_FILE, stderr);	

	wd_status.nspid = 0;

	parse_args(argc, argv);

	wmlog_msg(2,"Dir  %s",CONF_DIR);	
	
#ifdef WITH_DBUS
	if (dbus_use) {
		dbus_error_init (&dbus_error);
		dbus_connection = dbus_bus_get (DBUS_BUS_SESSION, &dbus_error);

		if (dbus_connection == NULL) {
			wmlog_msg(0, "Failed to connect to the D-BUS: %s\n", dbus_error.message);
			exit(1);
		}
	}
#endif /* WITH_DBUS */	
	
	if (xml_file)
		DL_xml_image();
	
	sigact.sa_handler = sighandler_exit;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);
	sigact.sa_handler = sighandler_wait_child;
	sigaction(SIGCHLD, &sigact, NULL);

	if (daemonize) {
		wmlog_msg(0, "Demon");
//		CHECK_NEGATIVE(daemon(0, 0));		
		if(daemon(0, 0))
			exit(1);
		if (pid_file) {
			FILE *f = fopen(pid_file, "w");
			if (f) {
				fprintf(f, "%u\n", getpid());
				fclose(f);
			}
		}		
	}
		
	wd_status.rf_state = 1;  // RF OFF
	wd_status.network_status = 0;
	wd_status.config = gct_config_read(conf_file); //conf_file
	if (!wd_status.config){
		wmlog_msg(0, "Bad config file");
		exit_release_resources(1);
	}
	
	gct_config_complete();

	if (logfile != NULL) {
		set_wmlogger(argv[0], WMLOGGER_FILE, logfile);
	} else if (daemonize || dbus_use) {
		set_wmlogger(argv[0], WMLOGGER_SYSLOG, NULL);
	} else {
		set_wmlogger(argv[0], WMLOGGER_FILE, stderr);
	}
	
	r = libusb_init(&ctx);
	if (r < 0) {
		if (dbus_use) dbus_conn_info_send(WS_DEV_NOT_FOUND);
		wmlog_msg(0, "failed to initialise libusb");
		exit_release_resources(1);
	}

	get_wimax_device();

	alloc_fds();
	libusb_set_pollfd_notifiers(ctx, cb_add_pollfd, cb_remove_pollfd, NULL);
	
	r = init();
	if (r < 0) {
		if (dbus_use) dbus_conn_info_send(WS_ERR_INIT_DRIVER);
		wmlog_msg(0, "init error %d", r);
		exit_release_resources(1);
	}

	if_create();
	cb_add_pollfd(tap_fd, POLLIN, NULL);

	r = scan_loop();
	if (r < 0) {
		if (dbus_use) dbus_conn_info_send(WS_ERR_INIT_DRIVER);
		wmlog_msg(0, "scan_loop error %d", r);
		exit_release_resources(1);
	}

	exit_release_resources(0);
	return 0;
}


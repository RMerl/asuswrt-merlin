#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <bcmnvram.h>

#include "utils.h"
#include "shutils.h"
#include "shared.h"

#ifdef RTCONFIG_RALINK

// TODO: make it switch model dependent, not product dependent
#ifdef RTCONFIG_DSL
#include "rtl8367r.h"
#else
#include "rtl8367m.h"
#endif	

#endif

static int gpio_values_loaded = 0;

#define GPIO_ACTIVE_LOW 0x1000

int btn_rst_gpio = 0x0ff;
int btn_wps_gpio = 0xff;
int led_pwr_gpio = 0xff;
int led_wps_gpio = 0xff;
int led_usb_gpio = 0xff;
int led_5g_gpio = 0xff;
int led_2g_gpio = 0xff;
int led_lan_gpio = 0xff;
int led_wan_gpio = 0xff;
int wan_port = 0xff;
int fan_gpio = 0xff;
int have_fan_gpio = 0xff;
#ifdef RTCONFIG_WIRELESS_SWITCH
int btn_wifi_sw = 0xff; 
#endif
#ifdef RTCONFIG_SWMODE_SWITCH
int btn_swmode_sw_router = 0xff;
int btn_swmode_sw_repeater = 0xff;
int btn_swmode_sw_ap = 0xff;
#endif

int init_gpio(void)
{

	// TODO: system dependent initialization
	return 0;
}

// this is shared by every process, so, need to get nvram for first time it called per process
int get_gpio_values_once(void)
{
	//int model;
	if (gpio_values_loaded) return;
	
	gpio_values_loaded = 1;
	//model = get_model();

	// TODO : add other models
	
	btn_rst_gpio = nvram_get_int("btn_rst_gpio");
	btn_wps_gpio = nvram_get_int("btn_wps_gpio");
	led_pwr_gpio = nvram_get_int("led_pwr_gpio");
	led_wps_gpio = nvram_get_int("led_wps_gpio");		
	led_2g_gpio = nvram_get_int("led_2g_gpio");
	led_5g_gpio = nvram_get_int("led_5g_gpio");
	led_lan_gpio = nvram_get_int("led_lan_gpio");
	led_wan_gpio = nvram_get_int("led_wan_gpio");
	led_usb_gpio = nvram_get_int("led_usb_gpio");

#ifdef RTCONFIG_SWMODE_SWITCH
	btn_swmode_sw_router = nvram_get_int("btn_swmode1_gpio");
	btn_swmode_sw_repeater = nvram_get_int("btn_swmode2_gpio");
	btn_swmode_sw_ap = nvram_get_int("btn_swmode3_gpio");
#endif

#ifdef RTCONFIG_WIRELESS_SWITCH
	btn_wifi_sw = nvram_get_int("btn_wifi_gpio"); 
#endif
}

int button_pressed(int which)
{
	int use_gpio;
	int gpio_value;

	get_gpio_values_once();
	switch(which) {
		case BTN_RESET:
			use_gpio = btn_rst_gpio;
			break;
		case BTN_WPS:
			use_gpio = btn_wps_gpio;
			break;
		case BTN_FAN:
			use_gpio = fan_gpio;
			break;
		case BTN_HAVE_FAN:
			use_gpio = have_fan_gpio;
			break;
#ifdef RTCONFIG_SWMODE_SWITCH
		case BTN_SWMODE_SW_ROUTER:
			use_gpio = btn_swmode_sw_router;
			break;
		case BTN_SWMODE_SW_REPEATER:
			use_gpio = btn_swmode_sw_repeater;
			break;
		case BTN_SWMODE_SW_AP:
			use_gpio = btn_swmode_sw_ap;
			break;
#endif
#ifdef RTCONFIG_WIRELESS_SWITCH
		case BTN_WIFI_SW:
			use_gpio = btn_wifi_sw;
			break;
#endif
		default:
			use_gpio = 0xff;
			break;
	}

	if((use_gpio&0xff)!=0x0ff) 
	{
		gpio_value = get_gpio(use_gpio&0xff);

//		_dprintf("use_gpio: %x gpio_value: %x\n", use_gpio, gpio_value);

		if((use_gpio&GPIO_ACTIVE_LOW)==0) // active high case
			return gpio_value==0 ? 0 : 1;
		else 
			return gpio_value==0 ? 1 : 0;
	}
	else return 0;
}


int led_control(int which, int mode)
{
	int use_gpio;
//	int gpio_value;
	int enable, disable;

	get_gpio_values_once();
	switch(which) {
		case LED_POWER:
			use_gpio = led_pwr_gpio;
			break;
		case LED_USB:
			use_gpio = led_usb_gpio;
			break;
		case LED_WPS:	
			use_gpio = led_wps_gpio;
			break;
		case LED_2G:
			use_gpio = led_2g_gpio;
			break;
		case LED_5G:
			use_gpio = led_5g_gpio;
			break;
		case LED_LAN:
			use_gpio = led_lan_gpio;
			break;
		case LED_WAN:
			use_gpio = led_wan_gpio;
			break;
		case FAN:
			use_gpio = fan_gpio;
			break;
		case HAVE_FAN:
			use_gpio = have_fan_gpio;
			break;
		default:
			use_gpio = 0xff;
			break;
	}

	if((use_gpio&0xff) != 0xff)
	{
		enable = (use_gpio&GPIO_ACTIVE_LOW)==0 ? 1 : 0;
		disable = (use_gpio&GPIO_ACTIVE_LOW)==0 ? 0: 1;

		switch(mode) {
			case LED_ON:
			case FAN_ON:
			case HAVE_FAN_ON:
				set_gpio((use_gpio&0xff), enable);
				break;
			case LED_OFF:	
			case FAN_OFF:
			case HAVE_FAN_OFF:
				set_gpio((use_gpio&0xff), disable);
				break;
		}
	}
	return 0;
}

int wanport_status(int wan_unit)
{
	char word[100], *next;
	int mask;
	char wan_ports[16];

	memset(wan_ports, 0, 16);
	if(wan_unit == 1)
		strcpy(wan_ports, "wan1ports");
	else
		strcpy(wan_ports, "wanports");

	mask = 0;

	foreach(word, nvram_safe_get(wan_ports), next) {
		mask |= (0x0001<<atoi(word));
	}
#ifdef RTCONFIG_WIRELESSWAN
	// to do for report wireless connection status
	if(is_wirelesswan_enabled())
		return 1;
#endif
	return get_phy_status(mask);
}

int wanport_speed(void)
{
	char word[100], *next;
	int mask;

	mask = 0;

	foreach(word, nvram_safe_get("wanports"), next) {
		mask |= (0x0003<<(atoi(word)*2));
	}

#ifdef RTCONFIG_WIRELESSWAN
	if(is_wirelesswan_enabled())
		return 0x01;
#endif
	return get_phy_speed(mask);
}

int wanport_ctrl(int ctrl)
{
#ifdef RTCONFIG_RALINK
	// TODO? no one use it.
	return 1;
#else
	char word[100], *next;
	int mask;

	mask = 0;

	foreach(word, nvram_safe_get("wanports"), next) {
		mask |= (0x0001<<atoi(word));
	}
#ifdef RTCONFIG_WIRELESSWAN
	// TODO for enable/disable wireless radio
	if(is_wirelesswan_enabled())
		return 0;
#endif
	return set_phy_ctrl(mask, ctrl);
#endif
}

int lanport_status(void)
{
// turn into a general way?
#ifdef RTCONFIG_RALINK

#ifdef RTCONFIG_DSL
	//DSL has no software controlled LAN LED
	return 0;
#else
	return rtl8367m_lanPorts_phyStatus();
#endif	
	
#else
	char word[100], *next;
	int mask;

	mask = 0;

	foreach(word, nvram_safe_get("lanports"), next) {
		mask |= (0x0001<<atoi(word));
	}
	return get_phy_status(mask);
#endif
}

int lanport_speed(void)
{
#ifdef RTCONFIG_RALINK
	return 1;
#else
	char word[100], *next;
	int mask;

	mask = 0;

	foreach(word, nvram_safe_get("lanports"), next) {
		mask |= (0x0003<<(atoi(word)*2));
	}
	return get_phy_speed(mask);
#endif
}

int lanport_ctrl(int ctrl)
{
	// no general way for ralink platform, so individual api for each kind of switch are used
#ifdef RTCONFIG_RALINK

#ifdef RTCONFIG_DSL
	if(ctrl) rtl8367r_LanPort_linkUp();
	else rtl8367r_LanPort_linkDown();
	return 1;
#else
	if(ctrl) rtl8367m_LanPort_linkUp();
	else rtl8367m_LanPort_linkDown();
	return 1;
#endif	

#else
	char word[100], *next;
	int mask;

	mask = 0;

	foreach(word, nvram_safe_get("lanports"), next) {
		mask |= (0x0001<<atoi(word));
	}
	return set_phy_ctrl(mask, ctrl);
#endif
}


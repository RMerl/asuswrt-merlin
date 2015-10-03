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

#if defined(RTCONFIG_BLINK_LED)
#include <bled_defs.h>
#endif

#ifdef RTCONFIG_RALINK
// TODO: make it switch model dependent, not product dependent
#include "rtkswitch.h"
#endif

#ifdef RTCONFIG_EXT_RTL8365MB
#include <rtk_switch.h>
#endif

int led_control(int which, int mode);

static int gpio_values_loaded = 0;

int btn_rst_gpio = 0x0ff;
int btn_wps_gpio = 0xff;

static int led_gpio_table[LED_ID_MAX];

int wan_port = 0xff;
int fan_gpio = 0xff;
int have_fan_gpio = 0xff;
#ifdef RTCONFIG_WIRELESS_SWITCH
int btn_wifi_sw = 0xff; 
#endif
#ifdef RTCONFIG_WIFI_TOG_BTN
int btn_wltog_gpio = 0xff; 
#endif
#ifdef RTCONFIG_TURBO
int btn_turbo_gpio = 0xff;
#endif
#ifdef RTCONFIG_LED_BTN
int btn_led_gpio = 0xff;
#endif
#ifdef RT4GAC55U
int btn_lte_gpio = 0xff;
#endif
#ifdef RTCONFIG_SWMODE_SWITCH
int btn_swmode_sw_router = 0xff;
int btn_swmode_sw_repeater = 0xff;
int btn_swmode_sw_ap = 0xff;
#endif
#ifdef RTCONFIG_QTN
int reset_qtn_gpio = 0xff;
#endif

int extract_gpio_pin(const char *gpio)
{
	char *p;

	if (!gpio || !(p = nvram_get(gpio)))
		return -1;

	return (atoi(p) & GPIO_PIN_MASK);
}

int init_gpio(void)
{
	char *btn_list[] = { "btn_rst_gpio", "btn_wps_gpio", "fan_gpio", "have_fan_gpio"
#ifdef RTCONFIG_WIRELESS_SWITCH
		, "btn_wifi_gpio"
#endif
#ifdef RTCONFIG_WIFI_TOG_BTN
		, "btn_wltog_gpio"
#endif
#ifdef RTCONFIG_SWMODE_SWITCH
		, "btn_swmode1_gpio", "btn_swmode2_gpio", "btn_swmode3_gpio"
#endif
#ifdef RTCONFIG_TURBO
		, "btn_turbo_gpio"
#endif
#ifdef RTCONFIG_LED_BTN
		, "btn_led_gpio"
#endif
#ifdef RT4GAC55U
		, "btn_lte_gpio"
#endif
	};
	char *led_list[] = { "led_turbo_gpio", "led_pwr_gpio", "led_usb_gpio", "led_wps_gpio", "fan_gpio", "have_fan_gpio", "led_lan_gpio", "led_wan_gpio", "led_usb3_gpio", "led_2g_gpio", "led_5g_gpio" 
#ifdef RTCONFIG_LAN4WAN_LED
		, "led_lan1_gpio", "led_lan2_gpio", "led_lan3_gpio", "led_lan4_gpio"
#endif  /* LAN4WAN_LED */
#ifdef RTCONFIG_LED_ALL
		, "led_all_gpio"
#endif
		, "led_wan_red_gpio"
#ifdef RTCONFIG_QTN
		, "reset_qtn_gpio"
#endif
#ifdef RTCONFIG_USBRESET
		, "pwr_usb_gpio"
		, "pwr_usb_gpio2"
#endif
#ifdef RTCONFIG_WIFIPWR
		, "pwr_2g_gpio"
		, "pwr_5g_gpio"
#endif
#ifdef RT4GAC55U
		, "led_lte_gpio", "led_sig1_gpio", "led_sig2_gpio", "led_sig3_gpio"
#endif
#if (defined(PLN12) || defined(PLAC56))
		, "plc_wake_gpio"
		, "led_pwr_red_gpio"
		, "led_2g_green_gpio", "led_2g_orange_gpio", "led_2g_red_gpio"
		, "led_5g_green_gpio", "led_5g_orange_gpio", "led_5g_red_gpio"
#endif
#ifdef RTCONFIG_MMC_LED
		, "led_mmc_gpio"
#endif
#ifdef RTCONFIG_RTAC5300
		, "rpm_fan_gpio"
#endif
#ifdef RTCONFIG_RESET_SWITCH
		, "reset_switch_gpio"
#endif
			   };
	int use_gpio, gpio_pin;
	int enable, disable;
	int i;

#ifdef RT4GAC55U
	void get_gpio_values_once(void);
	get_gpio_values_once();		// for filling data to led_gpio_table[]
#endif	/* RT4GAC55U */

	/* btn input */
	for(i = 0; i < ASIZE(btn_list); i++)
	{
		if (!nvram_get(btn_list[i]))
			continue;
		use_gpio = nvram_get_int(btn_list[i]);
		if((gpio_pin = use_gpio & 0xff) == 0xff)
			continue;
		gpio_dir(gpio_pin, GPIO_DIR_IN);
	}

	/* led output */
	for(i = 0; i < ASIZE(led_list); i++)
	{
		if (!nvram_get(led_list[i]))
			continue;
#if defined(RTCONFIG_ETRON_XHCI_USB3_LED)
		if (!strcmp(led_list[i], "led_usb3_gpio") && nvram_match("led_usb3_gpio", "etron")) {
			led_control(LED_USB3, LED_OFF);
			continue;
		}
#endif
		use_gpio = nvram_get_int(led_list[i]);

		if((gpio_pin = use_gpio & 0xff) == 0xff)
			continue;

#if defined(RTCONFIG_RALINK_MT7620)
		if(gpio_pin == 72)	//skip 2g wifi led NOT to be gpio LED
			continue;
#endif

		disable = (use_gpio&GPIO_ACTIVE_LOW)==0 ? 0: 1;
		gpio_dir(gpio_pin, GPIO_DIR_OUT);

		/* If WAN RED LED is defined, keep it on until Internet connection ready in router mode. */
		if (!strcmp(led_list[i], "led_wan_red_gpio") && nvram_get_int("sw_mode") == SW_MODE_ROUTER)
			disable = !disable;

		set_gpio(gpio_pin, disable);
#ifdef RT4GAC55U	// save setting value
		{ int i; char led[16]; for(i=0; i<LED_ID_MAX; i++) if(gpio_pin == (led_gpio_table[i]&0xff)){snprintf(led, sizeof(led), "led%02d", i); nvram_set_int(led, LED_OFF); break;}}
#endif	/* RT4GAC55U */
	}

#if (defined(PLN12) || defined(PLAC56))
	if((gpio_pin = (use_gpio = nvram_get_int("led_pwr_red_gpio")) & 0xff) != 0xff)
#else
	if((gpio_pin = (use_gpio = nvram_get_int("led_pwr_gpio")) & 0xff) != 0xff)
#endif
	{
		enable = (use_gpio&GPIO_ACTIVE_LOW)==0 ? 1 : 0;
		set_gpio(gpio_pin, enable);
#ifdef RT4GAC55U	// save setting value
		{ int i; char led[16]; for(i=0; i<LED_ID_MAX; i++) if(gpio_pin == (led_gpio_table[i]&0xff)){snprintf(led, sizeof(led), "led%02d", i); nvram_set_int(led, LED_ON); break;}}
#endif	/* RT4GAC55U */
	}

	// Power of USB.
	if((gpio_pin = (use_gpio = nvram_get_int("pwr_usb_gpio")) & 0xff) != 0xff){
		enable = (use_gpio&GPIO_ACTIVE_LOW)==0 ? 1 : 0;
		set_gpio(gpio_pin, enable);
	}
	if((gpio_pin = (use_gpio = nvram_get_int("pwr_usb_gpio2")) & 0xff) != 0xff){
		enable = (use_gpio&GPIO_ACTIVE_LOW)==0 ? 1 : 0;
		set_gpio(gpio_pin, enable);
	}

#ifdef RTAC5300
	// RPM of FAN
	if((gpio_pin = (use_gpio = nvram_get_int("rpm_fan_gpio")) & 0xff) != 0xff){
	enable = (use_gpio&GPIO_ACTIVE_LOW)==0 ? 1 : 0;
	set_gpio(gpio_pin, enable);
	}
#endif

#ifdef PLAC56
	if((gpio_pin = (use_gpio = nvram_get_int("plc_wake_gpio")) & 0xff) != 0xff){
		enable = (use_gpio&GPIO_ACTIVE_LOW)==0 ? 1 : 0;
		set_gpio(gpio_pin, enable);
	}
#endif

	// TODO: system dependent initialization
	return 0;
}

int set_pwr_usb(int boolOn){
	int use_gpio, gpio_pin;

	switch(get_model()) {
		case MODEL_RTAC68U:
			if((nvram_get_int("HW_ver") != 170) &&
			   (atof(nvram_safe_get("HW_ver")) != 1.10) &&
			   (atof(nvram_safe_get("HW_ver")) != 2.10))
				return 0;
			break;
	}

	if((gpio_pin = (use_gpio = nvram_get_int("pwr_usb_gpio"))&0xff) != 0xff){
		if(boolOn)
			set_gpio(gpio_pin, 1);
		else
			set_gpio(gpio_pin, 0);
	}

	if((gpio_pin = (use_gpio = nvram_get_int("pwr_usb_gpio2"))&0xff) != 0xff){
		if(boolOn)
			set_gpio(gpio_pin, 1);
		else
			set_gpio(gpio_pin, 0);
	}

	return 0;
}

/* Return GPIO# of specified nvram variable.
 * @return:
 * 	-1:	nvram variables doesn't not exist.
 */
static int __get_gpio(char *name)
{
	if (!nvram_get(name))
		return -1;

	return nvram_get_int(name);
}

// this is shared by every process, so, need to get nvram for first time it called per process
void get_gpio_values_once(void)
{
	int i;
	//int model;
	if (gpio_values_loaded) return;

	gpio_values_loaded = 1;
	//model = get_model();

	// TODO : add other models

	btn_rst_gpio = __get_gpio("btn_rst_gpio");
	btn_wps_gpio = __get_gpio("btn_wps_gpio");

	for (i = 0; i < ARRAY_SIZE(led_gpio_table); ++i) {
		led_gpio_table[i] = -1;
	}
	led_gpio_table[LED_POWER] = __get_gpio("led_pwr_gpio");
	led_gpio_table[LED_WPS] = __get_gpio("led_wps_gpio");
	led_gpio_table[LED_2G] = __get_gpio("led_2g_gpio");
	led_gpio_table[LED_5G] = __get_gpio("led_5g_gpio");
#ifdef RTCONFIG_LAN4WAN_LED
	led_gpio_table[LED_LAN1] = __get_gpio("led_lan1_gpio");
	led_gpio_table[LED_LAN2] = __get_gpio("led_lan2_gpio");
	led_gpio_table[LED_LAN3] = __get_gpio("led_lan3_gpio");
	led_gpio_table[LED_LAN4] = __get_gpio("led_lan4_gpio");
#else
	led_gpio_table[LED_LAN] = __get_gpio("led_lan_gpio");
#endif	/* LAN4WAN_LED */
	led_gpio_table[LED_WAN] = __get_gpio("led_wan_gpio");
	led_gpio_table[LED_USB] = __get_gpio("led_usb_gpio");
	led_gpio_table[LED_USB3] = __get_gpio("led_usb3_gpio");
#ifdef RTCONFIG_MMC_LED
	led_gpio_table[LED_MMC] = __get_gpio("led_mmc_gpio");
#endif
#ifdef RTCONFIG_RESET_SWITCH
	led_gpio_table[LED_RESET_SWITCH] = __get_gpio("reset_switch_gpio");
#endif
#ifdef RTCONFIG_LED_ALL
	led_gpio_table[LED_ALL] = __get_gpio("led_all_gpio");
#endif
	led_gpio_table[LED_TURBO] = __get_gpio("led_turbo_gpio");
	led_gpio_table[LED_WAN_RED] = __get_gpio("led_wan_red_gpio");
#ifdef RTCONFIG_QTN
	led_gpio_table[BTN_QTN_RESET] = __get_gpio("reset_qtn_gpio");
#endif
#ifdef RT4GAC55U
	led_gpio_table[LED_LTE] = __get_gpio("led_lte_gpio");
	led_gpio_table[LED_SIG1] = __get_gpio("led_sig1_gpio");
	led_gpio_table[LED_SIG2] = __get_gpio("led_sig2_gpio");
	led_gpio_table[LED_SIG3] = __get_gpio("led_sig3_gpio");
#endif

#ifdef RTAC5300
	led_gpio_table[RPM_FAN] = __get_gpio("rpm_fan_gpio");
#endif

#if (defined(PLN12) || defined(PLAC56))
	led_gpio_table[PLC_WAKE] = __get_gpio("plc_wake_gpio");
	led_gpio_table[LED_POWER_RED] = __get_gpio("led_pwr_red_gpio");
	led_gpio_table[LED_2G_GREEN] = __get_gpio("led_2g_green_gpio");
	led_gpio_table[LED_2G_ORANGE] = __get_gpio("led_2g_orange_gpio");
	led_gpio_table[LED_2G_RED] = __get_gpio("led_2g_red_gpio");
	led_gpio_table[LED_5G_GREEN] = __get_gpio("led_5g_green_gpio");
	led_gpio_table[LED_5G_ORANGE] = __get_gpio("led_5g_orange_gpio");
	led_gpio_table[LED_5G_RED] = __get_gpio("led_5g_red_gpio");
#endif

#ifdef RTCONFIG_SWMODE_SWITCH
	btn_swmode_sw_router = __get_gpio("btn_swmode1_gpio");
	btn_swmode_sw_repeater = __get_gpio("btn_swmode2_gpio");
	btn_swmode_sw_ap = __get_gpio("btn_swmode3_gpio");
#endif

#ifdef RTCONFIG_WIRELESS_SWITCH
	btn_wifi_sw = __get_gpio("btn_wifi_gpio");
#endif
#ifdef RTCONFIG_WIFI_TOG_BTN
	btn_wltog_gpio = __get_gpio("btn_wltog_gpio");
#endif
#ifdef RTCONFIG_TURBO
	btn_turbo_gpio = __get_gpio("btn_turbo_gpio");
#endif
#ifdef RTCONFIG_LED_BTN
	btn_led_gpio = __get_gpio("btn_led_gpio");
#endif
#ifdef RTCONFIG_QTN
	reset_qtn_gpio = nvram_get_int("reset_qtn_gpio");
#endif
#ifdef RT4GAC55U
	btn_lte_gpio = __get_gpio("btn_lte_gpio");
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
#ifdef RTCONFIG_WIFI_TOG_BTN
		case BTN_WIFI_TOG:
			use_gpio = btn_wltog_gpio;
			break;
#endif
#ifdef RTCONFIG_TURBO
		case BTN_TURBO:
			use_gpio = btn_turbo_gpio;
			break;
#endif
#ifdef RTCONFIG_LED_BTN
		case BTN_LED:
			use_gpio = btn_led_gpio;
			break;
#endif
#ifdef RT4GAC55U
		case BTN_LTE:
			use_gpio = btn_lte_gpio;
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
#ifdef RT4GAC55U
{ //save value
	char name[16];

	snprintf(name, sizeof(name), "led%02d", which);
	if(nvram_get_int(name) != mode)
		nvram_set_int(name, mode);

	if (nvram_match(LED_CTRL_HIPRIO, "1"))
		return 0;

	int do_led_control(int which, int mode);
	return do_led_control(which, mode);
}

int do_led_control(int which, int mode)
#endif
{
	int use_gpio, gpio_nr;
	int v = (mode == LED_OFF)? 0:1;

	// Did the user disable the leds?
	if ((mode == LED_ON) && (nvram_get_int("led_disable") == 1) && (which != LED_TURBO)
#ifdef RTCONFIG_QTN
		&& (which != BTN_QTN_RESET)
#endif
	)
	{
		return 0;
	}

	if (which < 0 || which >= LED_ID_MAX || mode < 0 || mode >= LED_FAN_MODE_MAX)
		return -1;

	get_gpio_values_once();
	use_gpio = led_gpio_table[which];
	gpio_nr = use_gpio & 0xFF;

#if defined(RTCONFIG_ETRON_XHCI_USB3_LED)
	if (which == LED_USB3 && nvram_match("led_usb3_gpio", "etron")) {
		char *onoff = "2";	/* LED OFF */

		if (mode == LED_ON || mode == FAN_ON || mode == HAVE_FAN_ON)
			onoff = "3";	/* LED ON */

		f_write_string("/sys/bus/pci/drivers/xhci_hcd/port_led", onoff, 0, 0);
		return 0;
	}
#endif

#if defined(RTAC56U) || defined(RTAC56S)
	if (which == LED_5G && nvram_match("5g_fail", "1"))
		return -1;
#endif

	if (use_gpio < 0 || gpio_nr == 0xFF)
		return -1;

	if (use_gpio & GPIO_ACTIVE_LOW)
		v ^= 1;

	if (mode == LED_OFF) {
		stop_bled(use_gpio);
	}

	set_gpio(gpio_nr, v);

	if (mode == LED_ON) {
		start_bled(use_gpio);
	}

	return 0;
}

/* Led control code used by Stealth Mode.  Unlike led_control(), this
 * function can also use other methods of enabling/disabling a LED
 * beside gpio - required for some models
 *
 * Also call led_control(), as it will handle gpio-based leds.
*/

int led_control_atomic(int which, int mode)
{
	int model;

	model = get_model();

	switch(which) {
		case LED_2G:
			if ((model == MODEL_RTN66U) || (model == MODEL_RTAC66U) || (model == MODEL_RTN16)) {
				if (mode == LED_ON)
					eval("wl", "-i", "eth1", "leddc", "0");
				else if (mode == LED_OFF)
					eval("wl", "-i", "eth1", "leddc", "1");
			} else if ((model == MODEL_RTAC56U) || (model == MODEL_RTAC56S)) {
				if (mode == LED_ON)
					eval("wl", "-i", "eth1", "ledbh", "3", "7");
				else if (mode == LED_OFF)
					eval("wl", "-i", "eth1", "ledbh", "3", "0");
			} else if ((model == MODEL_RTAC68U) || (model == MODEL_RTAC87U) || (model == MODEL_RTAC3200)) {
				if (mode == LED_ON)
					eval("wl", "ledbh", "10", "7");
				else if (mode == LED_OFF)
					eval("wl", "ledbh", "10", "0");
			} else if ((model == MODEL_RTAC88U) || (model == MODEL_RTAC3100) || (model == MODEL_RTAC3100)) {
				if (mode == LED_ON)
					eval("wl", "ledbh", "9", "7");
				else if (mode == LED_OFF)
					eval("wl", "ledbh", "9", "0");
			}
			break;
		case LED_5G_FORCED:
			if ((model == MODEL_RTAC68U) || (model == MODEL_RTAC3200)) {
				if (mode == LED_ON) {
					nvram_set("led_5g", "1");
		                        eval("wl", "-i", "eth2", "ledbh", "10", "7");
				} else if (mode == LED_OFF) {
					nvram_set("led_5g", "0");
					eval("wl", "-i", "eth2", "ledbh", "10", "0");
				}
			} else if ((model == MODEL_RTAC88U) || (model == MODEL_RTAC3100) || (model == MODEL_RTAC3100)) {
				if (mode == LED_ON)
					eval("wl", "-i", "eth2", "ledbh", "9", "7");
				else if (mode == LED_OFF)
					eval("wl", "-i", "eth2", "ledbh", "9", "0");
			}
			// Fall through regular LED_5G to handle other models
		case LED_5G:
			if ((model == MODEL_RTN66U) || (model == MODEL_RTN16)) {
                                if (mode == LED_ON)
                                        eval("wl", "-i", "eth2", "leddc", "0");
                                else if (mode == LED_OFF)
                                        eval("wl", "-i", "eth2", "leddc", "1");
			} else if ((model == MODEL_RTAC66U) || (model == MODEL_RTAC56U) || (model == MODEL_RTAC56S)) {
				if (mode == LED_ON)
					nvram_set("led_5g", "1");
				else if (mode == LED_OFF)
					nvram_set("led_5g", "0");
			}
			break;
		case LED_SWITCH:
			if (mode == LED_ON) {
				eval("et", "robowr", "0x00", "0x18", "0x01ff");
				eval("et", "robowr", "0x00", "0x1a", "0x01ff");
			} else if (mode == LED_OFF) {
				eval("et", "robowr", "0x00", "0x18", "0x01e0");
				eval("et", "robowr", "0x00", "0x1a", "0x01e0");
			}
			break;
	}

	return led_control(which, mode);
}

#ifdef RT4GAC55U
void led_control_lte(int percent)
{
	if(percent >= 0)
	{ //high priority led for LTE
		int LTE_LED[] = { LED_SIG1, LED_SIG2, LED_SIG3, LED_USB, LED_LAN, LED_2G, LED_5G, LED_WAN, LED_LTE, LED_POWER };
		int i;
		nvram_set(LED_CTRL_HIPRIO, "1");
		for(i = 0; i < ARRAY_SIZE(LTE_LED); i++)
		{
			if((percent/9) > i)
				do_led_control(LTE_LED[i], LED_ON);
			else
				do_led_control(LTE_LED[i], LED_OFF);
		}
	}
	else if(nvram_match(LED_CTRL_HIPRIO, "1"))
	{ //restore
		int which, mode;
		char name[16];
		char *p;

		nvram_unset(LED_CTRL_HIPRIO);

		for(which = 0; which < LED_ID_MAX; which++)
		{
			sprintf(name, "led%02d", which);
			if ((p = nvram_get(name)) != NULL)
			{
				mode = atoi(p);
				do_led_control(which, mode);
			}
		}
	}
}
#endif	/* RT4GAC55U */


extern uint32_t get_phy_status(uint32_t portmask);
extern uint32_t set_phy_ctrl(uint32_t portmask, int ctrl);

int wanport_status(int wan_unit)
{
#if defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA)
	return rtkswitch_wanPort_phyStatus(wan_unit);
#else
	char word[100], *next;
	int mask;
	char wan_ports[16];

	memset(wan_ports, 0, 16);
#ifndef RTN53
	if(nvram_get_int("sw_mode") == SW_MODE_AP)
		strcpy(wan_ports, "lanports");
	else
#endif 
	if(wan_unit == 1)
		strcpy(wan_ports, "wan1ports");
	else
		strcpy(wan_ports, "wanports");

	mask = 0;

	foreach(word, nvram_safe_get(wan_ports), next) {
		mask |= (0x0001<<atoi(word));
		if(nvram_get_int("sw_mode") == SW_MODE_AP)
			break;
	}
#ifdef RTCONFIG_WIRELESSWAN
	// to do for report wireless connection status
	if(is_wirelesswan_enabled())
		return 1;
#endif
	return get_phy_status(mask);
#endif
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

#ifdef RTCONFIG_DSL
	/* FIXME: Not implemented yet. */
	return 1;
#else
	if(ctrl) rtkswitch_WanPort_linkUp();
	else rtkswitch_WanPort_linkDown();
	return 1;
#endif
	return 1;
#elif defined(RTCONFIG_QCA)
	if(ctrl)
		rtkswitch_WanPort_linkUp();
	else
		rtkswitch_WanPort_linkDown();
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
	return rtkswitch_lanPorts_phyStatus();
#endif	
	
#elif defined(RTCONFIG_QCA)
	return rtkswitch_lanPorts_phyStatus();
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
	return get_phy_speed(0);	/* FIXME */
#elif defined(RTCONFIG_QCA)
	return get_phy_speed(0);	/* FIXME */
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

	if(ctrl) rtkswitch_LanPort_linkUp();
	else rtkswitch_LanPort_linkDown();
	return 1;

#elif defined(RTCONFIG_QCA)

	if(ctrl)
		rtkswitch_LanPort_linkUp();
	else
		rtkswitch_LanPort_linkDown();
	return 1;

#else
	char word[100], *next;
	int mask = 0;

#ifdef RTCONFIG_EXT_RTL8365MB
	if(ctrl)
		rtkswitch_ioctl(POWERUP_LANPORTS, -1, -1);
	else
		rtkswitch_ioctl(POWERDOWN_LANPORTS, -1, -1);
#endif

	foreach(word, nvram_safe_get("lanports"), next) {
		mask |= (0x0001<<atoi(word));
	}
	return set_phy_ctrl(mask, ctrl);
#endif
}


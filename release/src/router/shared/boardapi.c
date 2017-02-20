#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/mii.h>
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

#if defined(RTCONFIG_EXT_RTL8365MB) || defined(RTCONFIG_EXT_RTL8370MB)
#include <rtk_switch.h>
#endif

int led_control(int which, int mode);

static int gpio_values_loaded = 0;

static int btn_gpio_table[BTN_ID_MAX];
int led_gpio_table[LED_ID_MAX];

int wan_port = 0xff;
int fan_gpio = 0xff;
#ifdef RTCONFIG_QTN
int reset_qtn_gpio = 0xff;
#endif

static const struct led_btn_table_s {
	char *nv;
	int *p_val;
} led_btn_table[] = {
	/* button */
	{ "btn_rst_gpio",	&btn_gpio_table[BTN_RESET] },
	{ "btn_wps_gpio",	&btn_gpio_table[BTN_WPS] },
#ifdef RTCONFIG_SWMODE_SWITCH
#if defined(PLAC66U)
	{ "btn_swmode1_gpio",	&btn_gpio_table[BTN_SWMODE_SW_ROUTER] },
#else
	{ "btn_swmode1_gpio",	&btn_gpio_table[BTN_SWMODE_SW_ROUTER] },
	{ "btn_swmode2_gpio",	&btn_gpio_table[BTN_SWMODE_SW_REPEATER] },
	{ "btn_swmode3_gpio",	&btn_gpio_table[BTN_SWMODE_SW_AP] },
#endif	/* Model */
#endif	/* RTCONFIG_SWMODE_SWITCH */

#ifdef RTCONFIG_WIRELESS_SWITCH
	{ "btn_wifi_gpio",	&btn_gpio_table[BTN_WIFI_SW] },
#endif
#ifdef RTCONFIG_WIFI_TOG_BTN
	{ "btn_wltog_gpio",	&btn_gpio_table[BTN_WIFI_TOG] },
#endif
#ifdef RTCONFIG_LED_BTN
	{ "btn_led_gpio",	&btn_gpio_table[BTN_LED] },
#endif
#ifdef RTCONFIG_INTERNAL_GOBI
	{ "btn_lte_gpio",	&btn_gpio_table[BTN_LTE] },
#endif
#ifdef RTCONFIG_EJUSB_BTN
	{ "btn_ejusb1_gpio",	&btn_gpio_table[BTN_EJUSB1] },
	{ "btn_ejusb2_gpio",	&btn_gpio_table[BTN_EJUSB2] },
#endif
	/* LED */
	{ "led_pwr_gpio",	&led_gpio_table[LED_POWER] },
#if defined(RTCONFIG_PWRRED_LED)
	{ "led_pwr_red_gpio",	&led_gpio_table[LED_POWER_RED] },
#endif
	{ "led_wps_gpio",	&led_gpio_table[LED_WPS] },
	{ "led_2g_gpio",	&led_gpio_table[LED_2G] },
	{ "led_5g_gpio",	&led_gpio_table[LED_5G] },
#ifdef RTCONFIG_LAN4WAN_LED
	{ "led_lan1_gpio",	&led_gpio_table[LED_LAN1] },
	{ "led_lan2_gpio",	&led_gpio_table[LED_LAN2] },
	{ "led_lan3_gpio",	&led_gpio_table[LED_LAN3] },
	{ "led_lan4_gpio",	&led_gpio_table[LED_LAN4] },
#else
	{ "led_lan_gpio",	&led_gpio_table[LED_LAN] },
#endif	/* LAN4WAN_LED */
	{ "led_wan_gpio",	&led_gpio_table[LED_WAN] },
#if defined(RTCONFIG_WANPORT2)
	{ "led_wan2_gpio",	&led_gpio_table[LED_WAN2] },
#endif
#if defined(RTCONFIG_FAILOVER_LED)
	{ "led_failover_gpio",	&led_gpio_table[LED_FAILOVER] },
#endif
#if defined(RTCONFIG_M2_SSD)
	{ "led_sata_gpio",	&led_gpio_table[LED_SATA] },
#endif
	{ "led_usb_gpio",	&led_gpio_table[LED_USB] },
	{ "led_usb3_gpio",	&led_gpio_table[LED_USB3] },
#ifdef RTCONFIG_MMC_LED
	{ "led_mmc_gpio",	&led_gpio_table[LED_MMC] },
#endif
#ifdef RTCONFIG_RESET_SWITCH
	{ "reset_switch_gpio",	&led_gpio_table[LED_RESET_SWITCH] },
#endif
#ifdef RTCONFIG_LED_ALL
	{ "led_all_gpio",	&led_gpio_table[LED_ALL] },
#endif
#ifdef RTCONFIG_LOGO_LED
	{ "led_logo_gpio",	&led_gpio_table[LED_LOGO] },
#endif
	{ "led_wan_red_gpio",	&led_gpio_table[LED_WAN_RED] },
#if defined(RTCONFIG_WANPORT2) && defined(RTCONFIG_WANRED_LED)
	{ "led_wan2_red_gpio",	&led_gpio_table[LED_WAN2_RED] },
#endif
#ifdef RTCONFIG_QTN
	{ "reset_qtn_gpio",	&led_gpio_table[BTN_QTN_RESET] },
#endif
#ifdef RTCONFIG_INTERNAL_GOBI
	{ "led_3g_gpio",	&led_gpio_table[LED_3G] },
	{ "led_lte_gpio",	&led_gpio_table[LED_LTE] },
	{ "led_sig1_gpio",	&led_gpio_table[LED_SIG1] },
	{ "led_sig2_gpio",	&led_gpio_table[LED_SIG2] },
	{ "led_sig3_gpio",	&led_gpio_table[LED_SIG3] },
#ifdef RT4GAC68U
	{ "led_sig4_gpio",	&led_gpio_table[LED_SIG4] },
#endif
#endif

#if defined(RTCONFIG_RTAC5300) || defined(RTCONFIG_RTAC5300R)
	{ "rpm_fan_gpio",	&led_gpio_table[RPM_FAN] },
#endif

#if defined(PLN12) || defined(PLAC56)
	{ "plc_wake_gpio",	&led_gpio_table[PLC_WAKE] },
	{ "led_pwr_red_gpio",	&led_gpio_table[LED_POWER_RED] },
	{ "led_2g_green_gpio",	&led_gpio_table[LED_2G_GREEN] },
	{ "led_2g_orange_gpio",	&led_gpio_table[LED_2G_ORANGE] },
	{ "led_2g_red_gpio",	&led_gpio_table[LED_2G_RED] },
	{ "led_5g_green_gpio",	&led_gpio_table[LED_5G_GREEN] },
	{ "led_5g_orange_gpio",	&led_gpio_table[LED_5G_ORANGE] },
	{ "led_5g_red_gpio",	&led_gpio_table[LED_5G_RED] },
#endif

	{ NULL, NULL },
};

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
#if defined(PLAC66U)
		, "btn_swmode1_gpio"
#else
		, "btn_swmode1_gpio", "btn_swmode2_gpio", "btn_swmode3_gpio"
#endif	/* Mode */
#endif	/* RTCONFIG_SWMODE_SWITCH */
#ifdef RTCONFIG_LED_BTN
		, "btn_led_gpio"
#endif
#ifdef RTCONFIG_INTERNAL_GOBI
		, "btn_lte_gpio"
#endif
#ifdef RTCONFIG_EJUSB_BTN
		, "btn_ejusb1_gpio", "btn_ejusb2_gpio"
#endif
	};
	char *led_list[] = { "led_pwr_gpio", "led_usb_gpio", "led_wps_gpio", "fan_gpio", "have_fan_gpio", "led_lan_gpio", "led_wan_gpio", "led_usb3_gpio", "led_2g_gpio", "led_5g_gpio"
#if defined(RTCONFIG_PWRRED_LED)
		, "led_pwr_red_gpio"
#endif
#ifdef RTCONFIG_LOGO_LED
		, "led_logo_gpio"
#endif
#ifdef RTCONFIG_LAN4WAN_LED
		, "led_lan1_gpio", "led_lan2_gpio", "led_lan3_gpio", "led_lan4_gpio"
#endif  /* LAN4WAN_LED */
#ifdef RTCONFIG_LED_ALL
		, "led_all_gpio"
#endif
#if defined(RTCONFIG_WANPORT2)
		, "led_wan2_gpio"
#endif
#if defined(RTCONFIG_WANRED_LED)
		, "led_wan_red_gpio"
#if defined(RTCONFIG_WANPORT2)
		, "led_wan2_red_gpio"
#endif
#endif
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
#ifdef RTCONFIG_INTERNAL_GOBI
		, "led_3g_gpio", "led_lte_gpio", "led_sig1_gpio", "led_sig2_gpio", "led_sig3_gpio", "led_sig4_gpio"
#endif
#if defined(RTCONFIG_PWRRED_LED)
		, "led_pwr_red_gpio"
#endif
#if defined(RTCONFIG_FAILOVER_LED)
		, "led_failover_gpio"
#endif
#if defined(RTCONFIG_M2_SSD)
		, "led_sata_gpio"
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
#if defined(RTCONFIG_RTAC5300) || defined(RTCONFIG_RTAC5300R)
		, "rpm_fan_gpio"
#endif
#ifdef RTCONFIG_RESET_SWITCH
		, "reset_switch_gpio"
#endif
			   };
	int use_gpio, gpio_pin;
	int enable, disable;
	int i;

#ifdef RTCONFIG_INTERNAL_GOBI
	void get_gpio_values_once(int);
	get_gpio_values_once(0);		// for filling data to led_gpio_table[]
#endif	/* RTCONFIG_INTERNAL_GOBI */

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
#if defined(BRTAC828M2)
		if ((!strcmp(led_list[i], "led_wan_gpio") && nvram_match("led_wan_gpio", "qca8033")) ||
		    (!strcmp(led_list[i], "led_wan2_gpio") && nvram_match("led_wan2_gpio", "qca8033"))) {
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

#if defined(RTCONFIG_WANPORT2)
		/* Turn on WAN RED LED at system start-up if and only if coresponding WAN unit is enabled. */
		if (nvram_get_int("sw_mode") == SW_MODE_ROUTER) {
			if ((!strcmp(led_list[i], "led_wan_red_gpio") && get_dualwan_by_unit(0) != WANS_DUALWAN_IF_NONE) ||
			    (!strcmp(led_list[i], "led_wan2_red_gpio") && get_dualwan_by_unit(1) != WANS_DUALWAN_IF_NONE))
				disable = !disable;
		}
#else
#if defined(RTCONFIG_WANRED_LED)
		/* If WAN RED LED is defined, keep it on until Internet connection ready in router mode. */
		if (!strcmp(led_list[i], "led_wan_red_gpio") && nvram_get_int("sw_mode") == SW_MODE_ROUTER)
			disable = !disable;
#endif
#endif

		set_gpio(gpio_pin, disable);
#ifdef RTCONFIG_INTERNAL_GOBI	// save setting value
		{ int i; char led[16]; for(i=0; i<LED_ID_MAX; i++) if(gpio_pin == (led_gpio_table[i]&0xff)){snprintf(led, sizeof(led), "led%02d", i); nvram_set_int(led, LED_OFF); break;}}
#endif	/* RTCONFIG_INTERNAL_GOBI */
	}

#if (defined(PLN12) || defined(PLAC56))
	if((gpio_pin = (use_gpio = nvram_get_int("led_pwr_red_gpio")) & 0xff) != 0xff)
#else
	if((gpio_pin = (use_gpio = nvram_get_int("led_pwr_gpio")) & 0xff) != 0xff)
#endif
	{
		enable = (use_gpio&GPIO_ACTIVE_LOW)==0 ? 1 : 0;
		set_gpio(gpio_pin, enable);
#ifdef RTCONFIG_INTERNAL_GOBI	// save setting value
		{ int i; char led[16]; for(i=0; i<LED_ID_MAX; i++) if(gpio_pin == (led_gpio_table[i]&0xff)){snprintf(led, sizeof(led), "led%02d", i); nvram_set_int(led, LED_ON); break;}}
#endif	/* RTCONFIG_INTERNAL_GOBI */
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

#if defined(RTCONFIG_RTAC5300) || defined(RTCONFIG_RTAC5300R)
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

#ifdef RTCONFIG_BCMARM
int set_pwr_usb(int boolOn) {
	int use_gpio, gpio_pin;

#ifdef RTAC68U
	switch(get_model()) {
		case MODEL_RTAC68U:
			if ((nvram_get_int("HW_ver") != 170) &&
			    (nvram_get_double("HW_ver") != 1.10) &&
			    (nvram_get_double("HW_ver") != 1.85) &&
			    (nvram_get_double("HW_ver") != 1.90) &&
			    (nvram_get_double("HW_ver") != 1.95) &&
			    (nvram_get_double("HW_ver") != 2.10) &&
			    (nvram_get_double("HW_ver") != 2.20) &&
			    !is_ac66u_v2_series() &&
			    !nvram_match("cpurev", "c0"))
				return 0;
			break;
	}
#endif

	if ((gpio_pin = (use_gpio = nvram_get_int("pwr_usb_gpio"))&0xff) != 0xff) {
		if (boolOn)
			set_gpio(gpio_pin, 1);
		else
			set_gpio(gpio_pin, 0);
	}

	if ((gpio_pin = (use_gpio = nvram_get_int("pwr_usb_gpio2"))&0xff) != 0xff) {
		if (boolOn)
			set_gpio(gpio_pin, 1);
		else
			set_gpio(gpio_pin, 0);
	}

	return 0;
}
#else
int set_pwr_usb(int boolOn) {
	return 0;
}
#endif

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
void get_gpio_values_once(int force)
{
	int i;
	const struct led_btn_table_s *p;

	if (gpio_values_loaded && !force) return;

	gpio_values_loaded = 1;
	// TODO : add other models
	for (i = 0; i < ARRAY_SIZE(led_gpio_table); ++i) {
		led_gpio_table[i] = -1;
	}
	for (p = &led_btn_table[0]; p->p_val; ++p)
		*(p->p_val) = __get_gpio(p->nv);
}

int button_pressed(int which)
{
	int use_gpio;
	int gpio_value;

	if (which < 0 || which >= BTN_ID_MAX)
		return -1;

	get_gpio_values_once(0);
	use_gpio = btn_gpio_table[which];
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
#ifdef RTCONFIG_INTERNAL_GOBI
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
	if ((mode == LED_ON) && (nvram_get_int("led_disable") == 1)
#ifdef RTCONFIG_QTN
		&& (which != BTN_QTN_RESET)
#endif
	)
	{
		return 0;
	}

	if (which < 0 || which >= LED_ID_MAX || mode < 0 || mode >= LED_FAN_MODE_MAX)
		return -1;

	get_gpio_values_once(0);
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
			} else if ((model == MODEL_RTAC88U) || (model == MODEL_RTAC3100) || (model == MODEL_RTAC5300)) {
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
			} else if ((model == MODEL_RTAC88U) || (model == MODEL_RTAC3100) || (model == MODEL_RTAC5300)) {
				if (mode == LED_ON)
					eval("wl", "-i", "eth2", "ledbh", "9", "7");
				else if (mode == LED_OFF)
					eval("wl", "-i", "eth2", "ledbh", "9", "0");
			}
			// Second 5 GHz radio
			if (model == MODEL_RTAC5300) {
				if (mode == LED_ON)
					eval("wl", "-i", "eth3", "ledbh", "9", "7");
				else if (mode == LED_OFF)
					eval("wl", "-i", "eth3", "ledbh", "9", "0");
			} else if (model == MODEL_RTAC3200) {
				if (mode == LED_ON)
					eval("wl", "-i", "eth3", "ledbh", "10", "7");
				else if (mode == LED_OFF)
					eval("wl", "-i", "eth3", "ledbh", "10", "0");
			}
			which = LED_5G;	// Fall through regular LED_5G to handle other models
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

#if defined(RTCONFIG_EXT_RTL8365MB) || defined(RTCONFIG_EXT_RTL8370MB)
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


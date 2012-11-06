#!/bin/sh
nvram set led_disable=1
nvram commit
wl leddc 1
led_ctrl 0 0
led_ctrl 1 0
led_ctrl 20 0

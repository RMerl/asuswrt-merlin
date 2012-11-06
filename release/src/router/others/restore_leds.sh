#!/bin/sh
nvram set led_disable=0
nvram commit
wl leddc 0
led_ctrl 0 1
led_ctrl 1 1
led_ctrl 20 1

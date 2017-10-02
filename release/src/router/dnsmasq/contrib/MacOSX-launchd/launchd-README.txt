This is a launchd item for Mac OS X and Mac OS X Server.
For more information about launchd, the
"System wide and per-user daemon/agent manager", see the launchd
man page, or the wikipedia page: http://en.wikipedia.org/wiki/Launchd

This launchd item uses the following flags:
--keep-in-foreground - this is crucial for use with launchd
--log-queries - this is optional and you can remove it
--log-facility=/var/log/dnsmasq.log - again optional instead of system.log

To use this launchd item for dnsmasq:

If you don't already have a folder /Library/LaunchDaemons, then create one:
sudo mkdir /Library/LaunchDaemons
sudo chown root:admin /Library/LaunchDaemons
sudo chmod 775 /Library/LaunchDaemons 

Copy uk.org.thekelleys.dnsmasq.plist there and then set ownership/permissions:
sudo cp uk.org.thekelleys.dnsmasq.plist /Library/LaunchDaemons/
sudo chown root:admin /Library/LaunchDaemons/uk.org.thekelleys.dnsmasq.plist
sudo chmod 644 /Library/LaunchDaemons/uk.org.thekelleys.dnsmasq.plist

Optionally, edit your dnsmasq configuration file to your liking.

To start the launchd job, which starts dnsmasq, reboot or use the command:
sudo launchctl load /Library/LaunchDaemons/uk.org.thekelleys.dnsmasq.plist

To stop the launchd job, which stops dnsmasq, use the command:
sudo launchctl unload /Library/LaunchDaemons/uk.org.thekelleys.dnsmasq.plist

If you want to permanently stop the launchd job, so it doesn't start the job even after a reboot, use the following command:
sudo launchctl unload -w /Library/LaunchDaemons/uk.org.thekelleys.dnsmasq.plist

If you make a change to the configuration file, you should relaunch dnsmasq;
to do this unload and then load again:

sudo launchctl unload /Library/LaunchDaemons/uk.org.thekelleys.dnsmasq.plist
sudo launchctl load /Library/LaunchDaemons/uk.org.thekelleys.dnsmasq.plist

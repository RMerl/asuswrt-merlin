<?xml version="1.0" ?>
<qos_default_list>
<qos_rule><desc>Popular Service</desc><port>0000</port><proto></proto><rate></rate><prio></prio></qos_rule>

<qos_rule><desc>DNS, Time, NTP, RSVP</desc><port>53,37,123,3455</port><proto>TCP/UDP</proto><rate>0~10</rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>FaceTime(TCP)</desc><port>443,5223</port><proto>TCP</proto><rate></rate><prio>High</prio></qos_rule>
<qos_rule><desc>FaceTime(UDP)</desc><port>3478:3497, 16384:16387, 16393:16402</port><proto>UDP</proto><rate></rate><prio>High</prio></qos_rule>
<qos_rule><desc>FTP,SFTP,WLM,File/WebCam</desc><port>20:23, 6571, 681:6901</port><proto>TCP/UDP</proto><rate></rate><prio>Low</prio></qos_rule>
<qos_rule><desc>HTTP, SSL, File transfers</desc><port>80,443,8080</port><proto>TCP/UDP</proto><rate>~512</rate><prio>Medium</prio></qos_rule>
<qos_rule><desc>Netmeeting(TCP)</desc><port>389,522,1503,1720,1731</port><proto>TCP</proto><rate></rate><prio>High</prio></qos_rule>
<qos_rule><desc>PPTV</desc><port>4004,8008</port><proto>TCP/UDP</proto><rate></rate><prio>High</prio></qos_rule>
<qos_rule><desc>SMTP, POP3, IMAP, NNTP</desc><port>25,465,563,587,110,119,143,220,993,995</port><proto>TCP/UDP</proto><rate></rate><prio>Medium</prio></qos_rule>
<qos_rule><desc>Windows Live</desc><port>1493,1502,1503,1542,1863,1963,3389,506,5190:5193,7001</port><proto></proto><rate></rate><prio>Medium</prio></qos_rule>
<qos_rule><desc>WWW, SSL, HTTP Proxy</desc><port>80,443,8080</port><proto>TCP</proto><rate>0~512</rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Web Surf</desc><port>80</port><proto>TCP</proto><rate>0~512</rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>HTTPS</desc><port>443</port><proto>TCP</proto><rate>0~512</rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>File Transfer</desc><port>80,443</port><proto>TCP</proto><rate>512~</rate><prio>Highest</prio></qos_rule>

<qos_rule><desc>Online Game</desc><port>0000</port><proto></proto><rate></rate><prio></prio></qos_rule>

<qos_rule><desc>AIKA SEA</desc><port>8090,8831,8822</port><proto>TCP/UDP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Audition</desc><port>9110,15555,18505:18506,18510:18513,19000:19001</port><proto>TCP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>A.V.A(TCP)</desc><port>16384:18431,28004</port><proto>TCP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>A.V.A(UDP)</desc><port>28672:37267</port><proto>UDP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Blackshot(TCP)</desc><port>12000,30001</port><proto>TCP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Blackshot(UDP)</desc><port>10002:10005</port><proto>UDP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Cabal SEA(TCP)</desc><port>38111:38125</port><proto>TCP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Cabal SEA(UDP)</desc><port>6800:6899</port><proto>UDP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Counter Strike 1.6(TCP)</desc><port>27030:27039</port><proto>TCP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Counter Strike 1.6(UDP)</desc><port>1200,27000:27015</port><proto>UDP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Counter Strike –Source(TCP)</desc><port>27014:27050</port><proto>TCP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Counter Strike –Source(UDP)</desc><port>1200,3478,4379:4380,27000:27030</port><proto>UDP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Diablo 3</desc><port>1119,6881~6999</port><proto>TCP/UDP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Dragon Nest(TCP)</desc><port>14300,14400:14406,14500:14600</port><proto>TCP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Dragon Nest(UDP)</desc><port>15100:15200</port><proto>UDP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Entropia Universe</desc><port>554,30583,30584</port><proto>TCP/UDP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>FIFA Online 2(TCP)</desc><port>3658,6215,6316</port><proto>TCP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>FIFA Online 2(UDP)</desc><port>7299,10000,32768:65535</port><proto>UDP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Granada Espada</desc><port>7000,7040,13579</port><proto>TCP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>League of Legends(TCP)</desc><port>2099,5222:5223,8080,8393:8400</port><proto>TCP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>League of Legends(UDP)</desc><port>5000:5500</port><proto>UDP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Left4Dead</desc><port>4380,27000:27050</port><proto>TCP/UDP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Left4Dead 2</desc><port>4380,27000:27050</port><proto>TCP/UDP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Halo</desc><port>2302,2303</port><proto>UDP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Halo3 (2)</desc><port>3074</port><proto>TCP/UDP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Halo2(UDP)</desc><port>80</port><proto>UDP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Halo3(UDP)</desc><port>88</port><proto>UDP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Heroes of Newerth(TCP)</desc><port>11031</port><proto>TCP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Heroes of Newerth(UDP)</desc><port>11235:11335</port><proto>UDP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Maple SEA</desc><port>8484,8585:8586,8787:8789</port><proto>TCP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Playstation network</desc><port>10070:10080</port><proto>TCP/UDP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Rift</desc><port>6520:6540,6900:6929</port><proto>TCP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Runescape</desc><port>43594,43595</port><proto>TCP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Starcraft</desc><port>6112:6119, 4000</port><proto>TCP/UDP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Sudden Strike</desc><port>2300:2400,47626</port><proto>TCP/UDP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Sudden Strike(TCP)</desc><port>6073,28800:28900</port><proto>TCP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Team Fortress 2(TCP)</desc><port>27014:27050</port><proto>TCP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Team Fortress 2(UDP)</desc><port>2478:4380,27000:27030</port><proto>UDP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>World of Warcraft</desc><port>3724, 6112,6881:6999</port><proto>TCP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Xbox Live</desc><port>88, 3074</port><proto>TCP/UDP</proto><rate></rate><prio>Highest</prio></qos_rule>


<qos_rule><desc>P2P</desc><port>0000</port><proto></proto><rate></rate><prio></prio></qos_rule>

<qos_rule><desc>BitTorrent</desc><port>6881:6889</port><proto>TCP</proto><rate></rate><prio>Lowest</prio></qos_rule>
<qos_rule><desc>eDonkey</desc><port>4661:4662,4665</port><proto>TCP/UDP</proto><rate></rate><prio>Lowest</prio></qos_rule>
<qos_rule><desc>eMule</desc><port>4661:4662,4665,4672,4711</port><proto>TCP/UDP</proto><rate></rate><prio>Lowest</prio></qos_rule>
<qos_rule><desc>Shareaza</desc><port>6346</port><proto>TCP/UDP</proto><rate></rate><prio>Lowest</prio></qos_rule>
</qos_default_list>
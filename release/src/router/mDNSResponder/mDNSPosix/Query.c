#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>              // For errno, EINTR
#include <netinet/in.h>         // For INADDR_NONE
#include <arpa/inet.h>          // For inet_addr()
#include <netdb.h>              // For gethostbyname()

#include "mDNSEmbeddedAPI.h"	// Defines the interface to the mDNS core code
#include "mDNSPosix.h"    	// Defines the specific types needed to run mDNS on this platform
//#include "mDNSType.h" 

#include "dns_sd.h"
#include "../mDNSShared/dnssd_ipc.c"
#include "../mDNSShared/dnssd_clientlib.c"
#include "../mDNSShared/dnssd_clientstub.c"

/* Transfer Char to ASCII */
int char_to_ascii_safe(const char *output, const char *input, int outsize)
{
        char *src = (char *)input;
        char *dst = (char *)output;
        char *end = (char *)output + outsize - 1;
        char *escape = "[]"; // shouldn't be more?

        if (src == NULL || dst == NULL || outsize <= 0)
                return 0;

        for ( ; *src && dst < end; src++) {
                if ((*src >='0' && *src <='9') ||
                    (*src >='A' && *src <='Z') ||
                    (*src >='a' && *src <='z')) {
                        *dst++ = *src;
                } else if (strchr(escape, *src)) {
                        if (dst + 2 > end)
                                break;
                        *dst++ = '\\';
                        *dst++ = *src;
                } else {
                        if (dst + 3 > end)
                                break;
                        dst += sprintf(dst, "%%%.02X", *src);
                }
        }
        if (dst <= end)
                *dst = '\0';

        return dst - output;
}

void char_to_ascii(const char *output, const char *input)
{
        int outlen = strlen(input)*3 + 1;
        char_to_ascii_safe(output, input, outlen);
}

// Globals
static mDNS mDNSStorage;       			// mDNS core uses this to store its globals
static mDNS_PlatformSupport PlatformStorage;  	// Stores this platform's globals
#define RR_CACHE_SIZE 500
static CacheEntity gRRCache[RR_CACHE_SIZE];

mDNSexport const char ProgramName[] = "mDNSQuery";

static const char *gProgramName = ProgramName;
static volatile mDNSBool StopNow;

struct Support_Service {
        char name[24];
        struct Support_Service *next;
};

struct Device {
	char name[64];
	char MACAddr[18];
	char IPv4Addr[16];
	struct Support_Service *sup_service;
	struct Device *next;
};

struct Queried_Service {
	char service[24];
	char type[8];
	struct Device *sup_device;
	struct Queried_Service *next;
};

#if 1
struct mDNS_service_handler {
        char *service;
        char * description;
};

/* Service Type    Description */
struct mDNS_service_handler service_handler[] = {
{"1password", "1Password Password Manager data sharing and synchronization protocol"},
{"a-d-sync", "Altos Design Synchronization protocol"},
{"abi-instrument", "Applied Biosystems Universal Instrument Framework"},
{"accessdata-f2d", "FTK2 Database Discovery Service"},
{"accessdata-f2w", "FTK2 Backend Processing Agent Service"},
{"accessone", "Strix Systems 5S/AccessOne protocol"},
{"accountedge", "MYOB AccountEdge"},
{"acrobatsrv", "Adobe Acrobat"},
{"actionitems", "ActionItems"},
{"activeraid", "Active Storage Proprietary Device Management Protocol"},
{"activeraid-ssl", "Encrypted transport of Active Storage Proprietary Device Management Protocol"},
{"addressbook", "Address-O-Matic"},
{"adobe-vc", "Adobe Version Cue"},
{"adisk", "Automatic Disk Discovery"},
{"adpro-setup", "ADPRO Security Device Setup"},
{"aecoretech", "Apple Application Engineering Services"},
{"aeroflex", "Aeroflex instrumentation and software"},
{"afpovertcp", "Apple File Sharing"},
{"airport", "AirPort Base Station"},
{"airprojector", "AirProjector"},
{"airsharing", "Air Sharing"},
{"airsharingpro", "Air Sharing Pro"},
{"amba-cam", "Ambarella Cameras"},
{"amiphd-p2p", "P2PTapWar Sample Application from 'iPhone SDK Development' Book"},
{"animolmd", "Animo License Manager"},
{"animobserver", "Animo Batch Server"},
{"anquetsync", "Anquet map synchronization between desktop and handheld devices"},
{"appelezvous", "Appelezvous"},
{"apple-ausend", "Apple Audio Units"},
{"apple-midi", "Apple MIDI"},
{"apple-mobdev", "Apple mobile device"},
{"apple-sasl", "Apple Password Server"},
{"applerdbg", "Apple Remote Debug Services (OpenGL Profiler)"},
{"appletv", "Apple TV"},
{"appletv-itunes", "Apple TV discovery of iTunes"},
{"appletv-pair", "Apple TV Pairing"},
{"aquamon", "AquaMon"},
{"asr", "Apple Software Restore"},
{"astnotify", "Asterisk Caller-ID Notification Service"},
{"astralite", "Astralite"},
{"async", "address-o-sync"},
{"atlassianapp", "Atlassian Application (JIRA, Confluence, Fisheye, Crucible, Crowd, Bamboo) discovery service"},
{"av", "Allen Vanguard Hardware Service"},
{"axis-video", "Axis Video Cameras"},
{"auth", "Authentication Service"},
{"b3d-convince", "3M Unitek Digital Orthodontic System"},
{"babyphone", "BabyPhone"},
{"bdsk", "BibDesk Sharing"},
{"beacon", "Beacon Remote Service"},
{"beamer", "Beamer Data Sharing Protocol"},
{"beatpack", "BeatPack Synchronization Server for BeatMaker"},
{"beep", "Xgrid Technology Preview"},
{"bfagent", "BuildForge Agent"},
{"bigbangchess", "Big Bang Chess"},
{"bigbangmancala", "Big Bang Mancala"},
{"bittorrent", "BitTorrent Zeroconf Peer Discovery Protocol"},
{"blackbook", "Little Black Book Information Exchange Protocol"},
{"bluevertise", "BlueVertise Network Protocol (BNP)"},
{"bookworm", "Bookworm Client Discovery"},
{"bootps", "Bootstrap Protocol Server"},
{"boundaryscan", "Proprietary"},
{"bousg", "Bag Of Unusual Strategy Games"},
{"bri", "RFID Reader Basic Reader Interface"},
{"bsqdea", "Backup Simplicity"},
{"busycal", "BusySync Calendar Synchronization Protocol"},
{"caltalk", "CalTalk"},
{"cardsend", "Card Send Protocol"},
{"cctv", "IP and Closed-Circuit Television for Securitiy applications"},
{"cheat", "The Cheat"},
{"chess", "Project Gridlock"},
{"chfts", "Fluid Theme Server"},
{"chili", "The CHILI Radiology System"},
{"cip4discovery", "Discovery of JDF (CIP4 Job Definition Format) enabled devices"},
{"clipboard", "Clipboard Sharing"},
{"clique", "Clique Link-Local Multicast Chat Room"},
{"clscts", "Oracle CLS Cluster Topology Service"},
{"collection", "Published Collection Object"},
{"com-ocs-es-mcc", "ElectraStar media centre control protocol"},
{"contactserver", "Now Contact"},
{"corroboree", "Corroboree Server"},
{"cpnotebook2", "NoteBook 2"},
{"cvspserver", "CVS PServer"},
{"cw-codetap", "CodeWarrior HTI Xscale PowerTAP"},
{"cw-dpitap", "CodeWarrior HTI DPI PowerTAP"},
{"cw-oncetap", "CodeWarrior HTI OnCE PowerTAP"},
{"cw-powertap", "CodeWarrior HTI COP PowerTAP"},
{"cytv", "CyTV"},
{"daap", "Digital Audio Access Protocol (iTunes)"},
{"dacp", "Digital Audio Control Protocol (iTunes)"},
{"dancepartner", "Dance partner application for iPhone"},
{"dataturbine", "Open Source DataTurbine Streaming Data Middleware"},
{"device-info", "Device Info"},
{"difi", "EyeHome"},
{"disconnect", "DisConnect Peer to Peer Game Protocol"},
{"dist-opencl", "Distributed OpenCL discovery protocol"},
{"distcc", "Distributed Compiler"},
{"ditrios", "Ditrios SOA Framework Protocol"},
{"divelogsync", "Dive Log Data Sharing and Synchronization Protocol"},
{"dltimesync", "Local Area Dynamic Time Synchronisation Protocol"},
{"dns-llq", "DNS Long-Lived Queries"},
{"dns-sd", "DNS Service Discovery"},
{"dns-update", "DNS Dynamic Update Service"},
{"domain", "Domain Name Server"},
{"dop", "Roar (Death of Productivity)"},
{"dossier", "Vortimac Dossier Protocol"},
{"dpap", "Digital Photo Access Protocol (iPhoto)"},
{"dropcopy", "DropCopy"},
{"dsl-sync", "Data Synchronization Protocol for Discovery Software products"},
{"dtrmtdesktop", "Desktop Transporter Remote Desktop Protocol"},
{"dvbservdsc", "DVB Service Discovery"},
{"dxtgsync", "Documents To Go Desktop Sync Protocol"},
{"ea-dttx-poker", "Protocol for EA Downtown Texas Hold 'em"},
{"earphoria", "Earphoria"},
{"eb-amuzi", "Amuzi peer-to-peer session synchronization protocol"},
{"ebms", "ebXML Messaging"},
{"ecms", "Northrup Grumman/Mission Systems/ESL Data Flow Protocol"},
{"ebreg", "ebXML Registry"},
{"ecbyesfsgksc", "Net Monitor Anti-Piracy Service"},
{"edcp", "LaCie Ethernet Disk Configuration Protocol"},
{"egistix", "Egistix Auto-Discovery"},
{"eheap", "Interactive Room Software Infrastructure (Event Sharing)"},
{"embrace", "DataEnvoy"},
{"ep", "Endpoint Protocol (EP) for use in Home Automation systems"},
{"eppc", "Remote AppleEvents"},
{"erp-scale", "Pocket Programs ERP-Scale Server Discovery protocol"},
{"esp", "Extensis Server Protocol"},
{"eucalyptus", "Eucalyptus Discovery"},
{"eventserver", "Now Up-to-Date"},
{"evs-notif", "EVS Notification Center Protocol"},
{"ewalletsync", "Synchronization Protocol for Ilium Software's eWallet"},
{"example", "Example Service Type"},
{"exb", "Exbiblio Cascading Service Protocol"},
{"exec", "Remote Process Execution"},
{"extensissn", "Extensis Serial Number"},
{"eyetvsn", "EyeTV Sharing"},
{"facespan", "FaceSpan"},
{"fairview", "Fairview Device Identification"},
{"faxstfx", "FAXstf"},
{"feed-sharing", "NetNewsWire 2.0"},
{"firetask", "Firetask task sharing and synchronization protocol"},
{"fish", "Fish"},
{"fix", "Financial Information Exchange (FIX) Protocol"},
{"fjork", "Fjork"},
{"fl-purr", "FilmLight Cluster Power Control Service"},
{"fmpro-internal", "FileMaker Pro"},
{"fmserver-admin", "FileMaker Server Administration Communication Service"},
{"fontagentnode", "FontAgent Pro"},
{"foxtrot-serv", "FoxTrot Search Server Discovery Service"},
{"foxtrot-start", "FoxTrot Professional Search Discovery Service"},
{"frameforge-lic", "FrameForge License"},
{"freehand", "FreeHand MusicPad Pro Interface Protocol"},
{"frog", "Frog Navigation Systems"},
{"ftp", "File Transfer"},
{"ftpcroco", "Crocodile FTP Server"},
{"fv-cert", "Fairview Certificate"},
{"fv-key", "Fairview Key"},
{"fv-time", "Fairview Time/Date"},
{"garagepad", "Entrackment Client Service"},
{"gbs-smp", "SnapMail"},
{"gbs-stp", "SnapTalk"},
{"gforce-ssmp", "G-Force Control via SoundSpectrum's SSMP TCP Protocol"},
{"glasspad", "GlassPad Data Exchange Protocol"},
{"glasspadserver", "GlassPadServer Data Exchange Protocol"},
{"glrdrvmon", "OpenGL Driver Monitor"},
{"gpnp", "Grid Plug and Play"},
{"grillezvous", "Roxio ToastAnywhere(tm) Recorder Sharing"},
{"growl", "Growl"},
{"guid", "Special service type for resolving by GUID (Globally Unique Identifier)"},
{"h323", "H.323 Real-time audio, video and data communication call setup protocol"},
{"helix", "MultiUser Helix Server"},
{"help", "HELP command"},
{"hg", "Mercurial web-based repository access"},
{"hinz", "HINZMobil Synchronization protocol"},
{"hmcp", "Home Media Control Protocol"},
{"home-sharing", "iTunes Home Sharing"},
{"homeauto", "iDo Technology Home Automation Protocol"},
{"hotwayd", "Hotwayd"},
{"howdy", "Howdy messaging and notification protocol"},
{"hpr-bldlnx", "HP Remote Build System for Linux-based Systems"},
{"hpr-bldwin", "HP Remote Build System for Microsoft Windows Systems"},
{"hpr-db", "Identifies systems that house databases for the Remote Build System and Remote Test System"},
{"hpr-rep", "HP Remote Repository for Build and Test Results"},
{"hpr-toollnx", "HP Remote System that houses compilers and tools for Linux-based Systems"},
{"hpr-toolwin", "HP Remote System that houses compilers and tools for Microsoft Windows Systems"},
{"hpr-tstlnx", "HP Remote Test System for Linux-based Systems"},
{"hpr-tstwin", "HP Remote Test System for Microsoft Windows Systems"},
{"hs-off", "Hobbyist Software Off Discovery"},
{"htsp", "Home Tv Streaming Protocol"},
{"http", "World Wide Web HTML-over-HTTP"},
{"https", "HTTP over SSL/TLS"},
{"hydra", "SubEthaEdit"},
{"hyperstream", "Atempo HyperStream deduplication server"},
{"iax", "Inter Asterisk eXchange, ease-of-use NAT friendly open VoIP protocol"},
{"ibiz", "iBiz Server"},
{"ica-networking", "Image Capture Networking"},
{"ican", "Northrup Grumman/TASC/ICAN Protocol"},
{"ichalkboard", "iChalk"},
{"ichat", "iChat 1.0"},
{"iconquer", "iConquer"},
{"idata", "Generic Data Acquisition and Control Protocol"},
{"idsync", "SplashID Synchronization Service"},
{"ifolder", "Published iFolder"},
{"ihouse", "Idle Hands iHouse Protocol"},
{"ii-drills", "Instant Interactive Drills"},
{"ii-konane", "Instant Interactive Konane"},
{"ilynx", "iLynX"},
{"imap", "Internet Message Access Protocol"},
{"imidi", "iMidi"},
{"indigo-dvr", "Indigo Security Digital Video Recorders"},
{"inova-ontrack", "Inova Solutions OnTrack Display Protocol"},
{"idcws", "Intermec Device Configuration Web Services"},
{"ipbroadcaster", "IP Broadcaster"},
{"ipp", "IPP (Internet Printing Protocol)"},
{"ipspeaker", "IP Speaker Control Protocol"},
{"irelay", "iRelay application discovery service"},
{"irmc", "Intego Remote Management Console"},
{"iscsi", "Internet Small Computer Systems Interface (iSCSI)"},
{"isparx", "iSparx"},
{"ispq-vc", "iSpQ VideoChat"},
{"ishare", "iShare"},
{"isticky", "iSticky"},
{"istorm", "iStorm"},
{"itis-device", "IT-IS International Ltd. Device"},
{"itsrc", "iTunes Socket Remote Control"},
{"ivef", "Inter VTS Exchange Format"},
{"iwork", "iWork Server"},
{"jcan", "Northrup Grumman/TASC/JCAN Protocol"},
{"jeditx", "Jedit X"},
{"jini", "Jini Service Discovery"},
{"jtag", "Proprietary"},
{"kerberos", "Kerberos"},
{"kerberos-adm", "Kerberos Administration"},
{"ktp", "Kabira Transaction Platform"},
{"labyrinth", "Labyrinth local multiplayer protocol"},
{"lan2p", "Lan2P Peer-to-Peer Network Protocol"},
{"lapse", "Gawker"},
{"lanrevagent", "LANrev Agent"},
{"lanrevserver", "LANrev Server"},
{"ldap", "Lightweight Directory Access Protocol"},
{"leaf", "Lua Embedded Application Framework"},
{"lexicon", "Lexicon Vocabulary Sharing"},
{"liaison", "Liaison"},
{"library", "Delicious Library 2 Collection Data Sharing Protocol"},
{"llrp", "RFID reader Low Level Reader Protocol"},
{"llrp-secure", "RFID reader Low Level Reader Protocol over SSL/TLS"},
{"lobby", "Gobby"},
{"logicnode", "Logic Pro Distributed Audio"},
{"login", "Remote Login a la Telnet"},
{"lonbridge", "Echelon LonBridge Server"},
{"lontalk", "LonTalk over IP (ANSI 852)"},
{"lonworks", "Echelon LNS Remote Client"},
{"lsys-appserver", "Linksys One Application Server API"},
{"lsys-camera", "Linksys One Camera API"},
{"lsys-ezcfg", "LinkSys EZ Configuration"},
{"lsys-oamp", "LinkSys Operations, Administration, Management, and Provisioning"},
{"lux-dtp", "Lux Solis Data Transport Protocol"},
{"lxi", "LXI"},
{"lyrics", "iPod Lyrics Service"},
{"macfoh", "MacFOH"},
{"macfoh-admin", "MacFOH admin services"},
{"macfoh-audio", "MacFOH audio stream"},
{"macfoh-events", "MacFOH show control events"},
{"macfoh-data", "MacFOH realtime data"},
{"macfoh-db", "MacFOH database"},
{"macfoh-remote", "MacFOH Remote"},
{"macminder", "Mac Minder"},
{"maestro", "Maestro Music Sharing Service"},
{"magicdice", "Magic Dice Game Protocol"},
{"mandos", "Mandos Password Server"},
{"matrix", "MATRIX Remote AV Switching"},
{"mbconsumer", "MediaBroker++ Consumer"},
{"mbproducer", "MediaBroker++ Producer"},
{"mbserver", "MediaBroker++ Server"},
{"mconnect", "ClairMail Connect"},
{"mcrcp", "MediaCentral"},
{"mediaboard1", "MediaBoardONE Asset and Information Manager data sharing and synchronization protocol"},
{"mesamis", "Mes Amis"},
{"mimer", "Mimer SQL Engine"},
{"mi-raysat", "Mental Ray for Maya"},
{"modolansrv", "modo LAN Services"},
{"moneysync", "SplashMoney Synchronization Service"},
{"moneyworks", "MoneyWorks Gold and MoneyWorks Datacentre network service"},
{"moodring", "Bonjour Mood Ring tutorial program"},
{"mother", "Mother script server protocol"},
{"movieslate", "MovieSlate digital clapperboard"},
{"mp3sushi", "MP3 Sushi"},
{"mqtt", "IBM MQ Telemetry Transport Broker"},
{"mslingshot", "Martian SlingShot"},
{"mumble", "Mumble VoIP communication protocol"},
{"musicmachine", "Protocol for a distributed music playing service"},
{"mysync", "MySync Protocol"},
{"mttp", "MenuTunes Sharing"},
{"mxim-art2", "Maxim Integrated Products Automated Roadtest Mk II"},
{"mxim-ice", "Maxim Integrated Products In-circuit Emulator"},
{"mxs", "MatrixStore"},
{"ncbroadcast", "Network Clipboard Broadcasts"},
{"ncdirect", "Network Clipboard Direct Transfers"},
{"ncsyncserver", "Network Clipboard Sync Server"},
{"neoriders", "NeoRiders Client Discovery Protocol"},
{"net-assistant", "Apple Remote Desktop"},
{"net2display", "Vesa Net2Display"},
{"netrestore", "NetRestore"},
{"newton-dock", "Escale"},
{"nfs", "Network File System - Sun Microsystems"},
{"nssocketport", "DO over NSSocketPort"},
{"ntlx-arch", "American Dynamics Intellex Archive Management Service"},
{"ntlx-ent", "American Dynamics Intellex Enterprise Management Service"},
{"ntlx-video", "American Dynamics Intellex Video Service"},
{"ntp", "Network Time Protocol"},
{"ntx", "Tenasys"},
{"obf", "Observations Framework"},
{"objective", "Means for clients to locate servers in an Objective (http://www.objective.com) instance."},
{"oce", "Oce Common Exchange Protocol"},
{"od-master", "OpenDirectory Master"},
{"odabsharing", "OD4Contact"},
{"odisk", "Optical Disk Sharing"},
{"officetime-sync", "OfficeTime Synchronization Protocol"},
{"ofocus-conf", "OmniFocus setting configuration"},
{"ofocus-sync", "OmniFocus document synchronization"},
{"olpc-activity1", "One Laptop per Child activity"},
{"oma-bcast-sg", "OMA BCAST Service Guide Discovery Service"},
{"omni-bookmark", "OmniWeb"},
{"omni-live", "Service for remote control of Omnisphere virtual instrument"},
{"openbase", "OpenBase SQL"},
{"opencu", "Conferencing Protocol"},
{"oprofile", "oprofile server protocol"},
{"oscit", "Open Sound Control Interface Transfer"},
{"ovready", "ObjectVideo OV Ready Protocol"},
{"owhttpd", "OWFS (1-wire file system) web server"},
{"owserver", "OWFS (1-wire file system) server"},
{"parentcontrol", "Remote Parental Controls"},
{"passwordwallet", "PasswordWallet Data Synchronization Protocol"},
{"pcast", "Mac OS X Podcast Producer Server"},
{"p2pchat", "Peer-to-Peer Chat (Sample Java Bonjour application)"},
{"panoply", "Panoply multimedia composite transfer protocol"},
{"parabay-p2p", "Parabay P2P protocol"},
{"parliant", "PhoneValet Anywhere"},
{"pdl-datastream", "Printer Page Description Language Data Stream (vendor-specific)"},
{"pgpkey-hkp", "Horowitz Key Protocol (HKP)"},
{"pgpkey-http", "PGP Keyserver using HTTP/1.1"},
{"pgpkey-https", "PGP Keyserver using HTTPS"},
{"pgpkey-ldap", "PGP Keyserver using LDAP"},
{"pgpkey-mailto", "PGP Key submission using SMTP"},
{"photoparata", "Photo Parata Event Photography Software"},
{"pictua", "Pictua Intercommunication Protocol"},
{"piesync", "pieSync Computer to Computer Synchronization"},
{"piu", "Pedestal Interface Unit by RPM-PSI"},
{"poch", "Parallel OperatiOn and Control Heuristic (Pooch)"},
{"pokeeye", "Communication channel for 'Poke Eye' Elgato EyeTV remote controller"},
{"pop3", "Post Office Protocol - Version 3"},
{"postgresql", "PostgreSQL Server"},
{"powereasy-erp", "PowerEasy ERP"},
{"powereasy-pos", "PowerEasy Point of Sale"},
{"pplayer-ctrl", "Piano Player Remote Control"},
{"presence", "Peer-to-peer messaging / Link-Local Messaging"},
{"print-caps", "Retrieve a description of a device's print capabilities"},
{"printer", "Spooler (more commonly known as 'LPR printing' or 'LPD printing)'"},
{"profilemac", "Profile for Mac medical practice management software"},
{"prolog", "Prolog"},
{"protonet", "Protonet node and service discovery protocol"},
{"psap", "Progal Service Advertising Protocol"},
{"psia", "Physical Security Interoperability Alliance Protocol"},
{"ptnetprosrv2", "PTNetPro Service"},
{"ptp", "Picture Transfer Protocol"},
{"ptp-req", "PTP Initiation Request Protocol"},
{"puzzle", "Protocol used for puzzle games"},
{"qbox", "QBox Appliance Locator"},
{"qttp", "QuickTime Transfer Protocol"},
{"quinn", "Quinn Game Server"},
{"rakket", "Rakket Client Protocol"},
{"radiotag", "RadioTAG: Event tagging for radio services"},
{"radiovis", "RadioVIS: Visualisation for radio services"},
{"radioepg", "RadioEPG: Electronic Programme Guide for radio services"},
{"raop", "Remote Audio Output Protocol (AirTunes)"},
{"rbr", "RBR Instrument Communication"},
{"rce", "PowerCard"},
{"rdp", "Windows Remote Desktop Protocol"},
{"realplayfavs", "RealPlayer Shared Favorites"},
{"recipe", "Recipe Sharing Protocol"},
{"remote", "Remote Device Control Protocol"},
{"remoteburn", "LaCie Remote Burn"},
{"renderpipe", "ARTvps RenderDrive/PURE Renderer Protocol"},
{"rendezvouspong", "RendezvousPong"},
{"renkara-sync", "Renkara synchronization protocol"},
{"resacommunity", "Community Service"},
{"resol-vbus", "RESOL VBus"},
{"retrospect", "Retrospect backup and restore service"},
{"rfb", "Remote Frame Buffer (used by VNC)"},
{"rfbc", "Remote Frame Buffer Client (Used by VNC viewers in listen-mode)"},
{"rfid", "RFID Reader Mach1(tm) Protocol"},
{"riousbprint", "Remote I/O USB Printer Protocol"},
{"roku-rcp", "Roku Control Protocol"},
{"rql", "RemoteQuickLaunch"},
{"rsmp-server", "Remote System Management Protocol (Server Instance)"},
{"rsync", "Rsync"},
{"rtsp", "Real Time Streaming Protocol"},
{"rubygems", "RubyGems GemServer"},
{"safarimenu", "Safari Menu"},
{"sallingbridge", "Salling Clicker Sharing"},
{"sallingclicker", "Salling Clicker Service"},
{"salutafugijms", "Salutafugi Peer-To-Peer Java Message Service Implementation"},
{"sandvox", "Sandvox"},
{"sc-golf", "StrawberryCat Golf Protocol"},
{"scanner", "Bonjour Scanning"},
{"schick", "Schick"},
{"scone", "Scone"},
{"scpi-raw", "IEEE 488.2 (SCPI) Socket"},
{"scpi-telnet", "IEEE 488.2 (SCPI) Telnet"},
{"sdsharing", "Speed Download"},
{"see", "SubEthaEdit 2"},
{"seeCard", "seeCard"},
{"senteo-http", "Senteo Assessment Software Protocol"},
{"sentillion-vlc", "Sentillion Vault System"},
{"sentillion-vlt", "Sentillion Vault Systems Cluster"},
{"sepvsync", "SEPV Application Data Synchronization Protocol"},
{"serendipd", "serendiPd Shared Patches for Pure Data"},
{"servereye", "ServerEye AgentContainer Communication Protocol"},
{"servermgr", "Mac OS X Server Admin"},
{"services", "DNS Service Discovery"},
{"sessionfs", "Session File Sharing"},
{"sflow", "sFlow traffic monitoring"},
{"sftp-ssh", "Secure File Transfer Protocol over SSH"},
{"shell", "like exec, but automatic authentication is performed as for login server."},
{"shifter", "Window Shifter server protocol"},
{"shipsgm", "Swift Office Ships"},
{"shipsinvit", "Swift Office Ships"},
{"shoppersync", "SplashShopper Synchronization Service"},
{"shoutcast", "Nicecast"},
{"simmon", "Medical simulation patient monitor syncronisation protocol"},
{"simusoftpong", "simusoftpong iPhone game protocol"},
{"sip", "Session Initiation Protocol, signalling protocol for VoIP"},
{"sipuri", "Session Initiation Protocol Uniform Resource Identifier"},
{"sironaxray", "Sirona Xray Protocol"},
{"skype", "Skype"},
{"sleep-proxy", "Sleep Proxy Server"},
{"slimcli", "SliMP3 Server Command-Line Interface"},
{"slimhttp", "SliMP3 Server Web Interface"},
{"smartenergy", "Smart Energy Profile"},
{"smb", "Server Message Block over TCP/IP"},
{"sms", "Short Text Message Sending and Delivery Status Service"},
{"soap", "Simple Object Access Protocol"},
{"socketcloud", "Socketcloud distributed application framework"},
{"sox", "Simple Object eXchange"},
{"sparechange", "SpareChange data sharing protocol"},
{"spearcat", "sPearCat Host Discovery"},
{"spike", "Shared Clipboard Protocol"},
{"spincrisis", "Spin Crisis"},
{"spl-itunes", "launchTunes"},
{"spr-itunes", "netTunes"},
{"splashsync", "SplashData Synchronization Service"},
{"ssh", "SSH Remote Login Protocol"},
{"ssscreenshare", "Screen Sharing"},
{"strateges", "Strateges"},
{"sge-exec", "Sun Grid Engine (Execution Host)"},
{"sge-qmaster", "Sun Grid Engine (Master)"},
{"souschef", "SousChef Recipe Sharing Protocol"},
{"sparql", "SPARQL Protocol and RDF Query Language"},
{"stanza", "Lexcycle Stanza service for discovering shared books"},
{"stickynotes", "Sticky Notes"},
{"submission", "Message Submission"},
{"supple", "Supple Service protocol"},
{"surveillus", "Surveillus Networks Discovery Protocol"},
{"svn", "Subversion"},
{"swcards", "Signwave Card Sharing Protocol"},
{"switcher", "Wireless home control remote control protocol"},
{"swordfish", "Swordfish Protocol for Input/Output"},
{"sxqdea", "Synchronize! Pro X"},
{"sybase-tds", "Sybase Server"},
{"syncopation", "Syncopation Synchronization Protocol by Sonzea"},
{"syncqdea", "Synchronize! X Plus 2.0"},
{"synergy", "Synergy Peer Discovery"},
{"synksharing", "SynkSharing synchronization protocol"},
{"taccounting", "Data Transmission and Synchronization"},
{"tango", "Tango Remote Control Protocol"},
{"tapinoma-ecs", "Tapinoma Easycontact receiver"},
{"taskcoachsync", "Task Coach Two-way Synchronization Protocol for iPhone"},
{"tbricks", "tbricks internal protocol"},
{"tcode", "Time Code"},
{"tcu", "Tracking Control Unit by RPM-PSI"},
{"te-faxserver", "TE-SYSTEMS GmbH Fax Server Daemon"},
{"teamlist", "ARTIS Team Task"},
{"teleport", "teleport"},
{"telnet", "Telnet"},
{"tera-fsmgr", "Terascala Filesystem Manager Protocol"},
{"tera-mp", "Terascala Maintenance Protocol"},
{"tf-redeye", "ThinkFlood RedEye IR bridge"},
{"tftp", "Trivial File Transfer"},
{"thumbwrestling", "tinkerbuilt Thumb Wrestling game"},
{"ticonnectmgr", "TI Connect Manager Discovery Service"},
{"timbuktu", "Timbuktu"},
{"tinavigator", "TI Navigator Hub 1.0 Discovery Service"},
{"tivo-hme", "TiVo Home Media Engine Protocol"},
{"tivo-music", "TiVo Music Protocol"},
{"tivo-photos", "TiVo Photos Protocol"},
{"tivo-remote", "TiVo Remote Protocol"},
{"tivo-videos", "TiVo Videos Protocol"},
{"todogwa", "2Do Sync Helper Tool for Mac OS X and PCs"},
{"tomboy", "Tomboy"},
{"toothpicserver", "ToothPics Dental Office Support Server"},
{"touch-able", "iPhone and iPod touch Remote Controllable"},
{"touch-remote", "iPhone and iPod touch Remote Pairing"},
{"tri-vis-client", "triCerat Simplify Visibility Client"},
{"tri-vis-server", "triCerat Simplify Visibility Server"},
{"tryst", "Tryst"},
{"tt4inarow", "Trivial Technology's 4 in a Row"},
{"ttcheckers", "Trivial Technology's Checkers"},
{"ttp4daemon", "TechTool Pro 4 Anti-Piracy Service"},
{"tunage", "Tunage Media Control Service"},
{"tuneranger", "TuneRanger"},
{"ubertragen", "Ubertragen"},
{"uddi", "Universal Description, Discovery and Integration"},
{"uddi-inq", "Universal Description, Discovery and Integration Inquiry"},
{"uddi-pub", "Universal Description, Discovery and Integration Publishing"},
{"uddi-sub", "Universal Description, Discovery and Integration Subscription"},
{"uddi-sec", "Universal Description, Discovery and Integration Security"},
{"upnp", "Universal Plug and Play"},
{"urlbookmark", "URL Advertising"},
{"uswi", "Universal Switching Corporation products"},
{"utest", "uTest"},
{"uwsgi", "Unbit Web Server Gateway Interface"},
{"ve-decoder", "American Dynamics VideoEdge Decoder Control Service"},
{"ve-encoder", "American Dynamics VideoEdge Encoder Control Service"},
{"ve-recorder", "American Dynamics VideoEdge Recorder Control Service"},
{"visel", "visel Q-System services"},
{"volley", "Volley"},
{"vos", "Virtual Object System (using VOP/TCP)"},
{"vue4rendercow", "VueProRenderCow"},
{"vxi-11", "VXI-11 TCP/IP Instrument Protocol"},
{"walkietalkie", "Walkie Talkie"},
{"we-jell", "Proprietary collaborative messaging protocol"},
{"webdav", "World Wide Web Distributed Authoring and Versioning (WebDAV)"},
{"webdavs", "WebDAV over SSL/TLS"},
{"webissync", "WebIS Sync Protocol"},
{"wedraw", "weDraw document sharing protocol"},
{"whamb", "Whamb"},
{"whistler", "Honeywell Video Systems"},
{"wired", "Wired Server"},
{"witap", "WiTap Sample Game Protocol"},
{"witapvoice", "witapvoice"},
{"wkgrpsvr", "Workgroup Server Discovery"},
{"workstation", "Workgroup Manager"},
{"wormhole", "Roku Cascade Wormhole Protocol"},
{"workgroup", "Novell collaboration workgroup"},
{"writietalkie", "Writie Talkie Data Sharing"},
{"ws", "Web Services"},
{"wtc-heleos", "Wyatt Technology Corporation HELEOS"},
{"wtc-qels", "Wyatt Technology Corporation QELS"},
{"wtc-rex", "Wyatt Technology Corporation Optilab rEX"},
{"wtc-viscostar", "Wyatt Technology Corporation ViscoStar"},
{"wtc-wpr", "Wyatt Technology Corporation DynaPro Plate Reader"},
{"wwdcpic", "PictureSharing sample code"},
{"x-on", "x-on services synchronisation protocol"},
{"x-plane9", "x-plane9"},
{"xcodedistcc", "Xcode Distributed Compiler"},
{"xgate-rmi", "xGate Remote Management Interface"},
{"xgrid", "Xgrid"},
{"xmms2", "XMMS2 IPC Protocol"},
{"xmp", "Xperientia Mobile Protocol"},
{"xmpp-client", "XMPP Client Connection"},
{"xmpp-server", "XMPP Server Connection"},
{"xsanclient", "Xsan Client"},
{"xsanserver", "Xsan Server"},
{"xsansystem", "Xsan System"},
{"xserveraid", "XServe Raid"},
{"xsync", "Xserve RAID Synchronization"},
{"xtimelicence", "xTime License"},
{"xtshapro", "xTime Project"},
{"xul-http", "XUL (XML User Interface Language) transported over HTTP"},
{"yakumo", "Yakumo iPhone OS Device Control Protocol"},
{NULL, NULL}
};
#endif

struct Queried_Service *Service_list = NULL;
struct Queried_Service *Service_tmp = NULL;
struct Queried_Service *Service_cur = NULL;

struct Device *Device_list = NULL;
struct Device *Device_tmp = NULL;
struct Device *Device_cur = NULL;
struct Device *Device_found = NULL;
struct Device *Device_cmp = NULL;
struct Support_Service *Sup_service_tmp = NULL;
struct Support_Service *Sup_service_cur = NULL;
struct Device *DeviceIP_tmp = NULL;
struct Device *DeviceIP_cur = NULL;

char Query_type[32]; 
int callback = 0;
int event = 0;
int device_count = 0;
int service_count = 0;
static void *QueriedNameParse(char *type, char *query_name, struct Device *device)
{
        char ParseName[64];
        char *c;

        memset(ParseName, 0, 64);
	memset(device->IPv4Addr, 0, 16);

        //Execption
        if(strstr(query_name, "QNAP")) {
                c = strchr(query_name, '(');
                if(c) *c = NULL;
                strcpy(device->name, query_name);
        }
        else if(!strcmp(type, "_workstation")) {
                c = strstr(query_name, " [");
                if( c && (*(c+4)==':') && (*(c+19)==']')) {
                        *c = NULL;
                        strcpy(device->name, query_name);
/*
printf("c= %s\n", c);
                        c+2;
printf("c= %s\n", c);
                        *(c+17) = NULL;
                        printf("!!!!!!!! MAC=%s=\n", c);
                        strcpy(device->MACAddr, c);
*/
                }
        }
	else
		strcpy(device->name, query_name);

}

static void *HostnameParse(char *name)
{
        char ParseName[64];
        char *c;
        int i = 0, Get = 0;

        memset(ParseName, 0, 64);

        //Execption
        if(strstr(name, "QNAP")) {
                c = strchr(name, '(');
                if(c) *c = NULL;
        }

        //Parse hostname
        c = name;
        while(*c!=NULL) {
                //printf("%c ", *c);
                if( (*c>='0' && *c<='9') || (*c>='a' && *c<='z') || (*c>='A' && *c<='Z') ) {
                        ParseName[i] = *c;
                        Get = 0;
                        i++;
                }
                else if( (*c==' ') || (*c=='-') ||(*c=='_') ) {
                        if(Get==0) {
                                ParseName[i] = '-';
                                i++;
                        }
                        Get = 1;
                }
                else Get = 0; //Others

                c++;
        }
        strcpy(name, ParseName);
}

static void BrowseCallback(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer, QC_result AddRecord)
    // A callback from the core mDNS code that indicates that we've received a 
    // response to our query.  Note that this code runs on the main thread 
    // (in fact, there is only one thread!), so we can safely printf the results.
{
return;
    domainlabel name;
    domainname  type;
    domainname  domain;
    char nameC  [MAX_DOMAIN_LABEL+1];	// Unescaped name: up to 63 bytes plus C-string terminating NULL.
    char typeC  [MAX_ESCAPED_DOMAIN_NAME];
    char domainC[MAX_ESCAPED_DOMAIN_NAME];
    const char *state;
    (void)m;        // Unused
    (void)question; // Unused

    assert(answer->rrtype == kDNSType_PTR);

    DeconstructServiceName(&answer->rdata->u.name, &name, &type, &domain);

    ConvertDomainLabelToCString_unescaped(&name, nameC);
    ConvertDomainNameToCString(&type, typeC);
    ConvertDomainNameToCString(&domain, domainC);
    callback++;
    // If the TTL has hit 0, the service is no longer available.
    if (AddRecord) {
    	fprintf(stderr, "*** Found name = '%s', type = '%s', domain = '%s'\n", nameC, typeC, domainC);
	//fprintf(stderr, "=== call back count: %d ===\n", callback);
	if(Service_list == NULL) {
		Service_list = (struct Queried_Service *)malloc(sizeof(struct Queried_Service));
		strcpy(Service_list->service, nameC);
		strcpy(Service_list->type, typeC);
		Service_list->sup_device = NULL;
		Service_cur = Service_list;
		Service_cur->next = NULL;
	}
	else {
                Service_tmp = (struct Queried_Service *)malloc(sizeof(struct Queried_Service));
                strcpy(Service_tmp->service, nameC);
                strcpy(Service_tmp->type, typeC);
		Service_tmp->sup_device = NULL;
		Service_tmp->next = NULL;
                Service_cur->next = Service_tmp;
                Service_cur = Service_tmp;
	}
        callback = 0;
        event = 0;
    }
}

static void QueryCallback(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer, QC_result AddRecord)
{
    domainlabel name;
    domainname  type;
    domainname  domain;
    char nameC  [MAX_DOMAIN_LABEL+1];   // Unescaped name: up to 63 bytes plus C-string terminating NULL.
    char typeC  [MAX_ESCAPED_DOMAIN_NAME];
    char domainC[MAX_ESCAPED_DOMAIN_NAME];
    const char *state;
    (void)m;        // Unused
    (void)question; // Unused

    assert(answer->rrtype == kDNSType_PTR);

    DeconstructServiceName(&answer->rdata->u.name, &name, &type, &domain);

    ConvertDomainLabelToCString_unescaped(&name, nameC);
    ConvertDomainNameToCString(&type, typeC);
    ConvertDomainNameToCString(&domain, domainC);
    callback++;
    // If the TTL has hit 0, the service is no longer available.
    if (AddRecord) {
        fprintf(stderr, "*** Found device = '%s', service = '%s', domain = '%s'\n", nameC, typeC, domainC);
        //fprintf(stderr, "=== call back count: %d ===\n", callback);
        if(Device_list == NULL) {
                Device_list = (struct Device *)malloc(sizeof(struct Device));
                QueriedNameParse(Service_tmp->service, nameC, Device_list);
printf("1.    Get %s, %s\n", Device_list->name, Device_list->MACAddr);
		Device_list->sup_service = (struct Support_Service *)malloc(sizeof(struct Support_Service));
		strcpy(Device_list->sup_service->name, Service_tmp->service);
		Device_list->sup_service->next = NULL;
                Device_cur = Device_list;
                Device_cur->next = NULL;
		service_count++;
        }
        else {
		Device_found = (struct Device *)malloc(sizeof(struct Device));
		QueriedNameParse(Service_tmp->service, nameC, Device_found);
printf("2.    Get %s, %s\n", Device_found->name, Device_found->MACAddr);

		Device_cmp = Device_list;
		while(Device_cmp != NULL) {
printf("%s ? %s\n",Device_found->name, Device_cmp->name);
			if(!strcmp(Device_found->name, Device_cmp->name)) { //the device already exist, add service to list.
printf("Device name the same!\n");
				Sup_service_cur = Device_cmp->sup_service;
				while(Sup_service_cur->next!=NULL)
					Sup_service_cur = Sup_service_cur->next;

				Sup_service_tmp = (struct Support_Service *)malloc(sizeof(struct Support_Service));
				strcpy(Sup_service_tmp->name, Service_tmp->service);
				Sup_service_tmp->next = NULL;
				Sup_service_cur->next = Sup_service_tmp;
				Sup_service_cur = Sup_service_tmp;				
				service_count++;

				return;
			}
			Device_cmp = Device_cmp->next;
		}

printf("New Device name!\n");
		Device_found->sup_service = (struct Support_Service *)malloc(sizeof(struct Support_Service));
		strcpy(Device_found->sup_service->name, Service_tmp->service);
		Device_found->sup_service->next = NULL;
                Device_found->next = NULL;
       	        Device_cur->next = Device_found;
               	Device_cur = Device_found;
		Device_cur->next = NULL;
        }

printf("device= %s\n", Device_cur->name);
	device_count++;
        callback = 0;
        event = 0;
    }
}

static void IPInfoCallback(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer, QC_result AddRecord)
{
    	(void)m;        // Unused
    	(void)question; // Unused
	const unsigned char *b;
    
printf("IPInfoCallBack...\n");
    // If the TTL has hit 0, the service is no longer available.
    if (AddRecord) {

	if (answer->rrtype == kDNSType_A && answer->rdlength == sizeof(mDNSv4Addr)) {
		b = &answer->rdata->u.ipv4;
		printf("IP:%d.%d.%d.%d\n", b[0], b[1], b[2], b[3]);
		sprintf(DeviceIP_cur->IPv4Addr, "%d.%d.%d.%d", b[0], b[1], b[2], b[3]);
		printf("IP= %s\n", DeviceIP_cur->IPv4Addr);
        	callback = 0;
        	event = 0;
		StopNow = 1;//Break EventLoop
	}
    }
}

mDNSexport void EventLoop(mDNS *const m)
{
//printf("*** Call EventLoop ***\n");
        while (!StopNow)
        {
                int nfds = 0;
                fd_set readfds;
                struct timeval timeout;
                int result;

                // 1. Set up the fd_set as usual here.
                FD_ZERO(&readfds);

                // 2. Set up the timeout.
                timeout.tv_sec = 0x3FFFFFFF;
                timeout.tv_usec = 0;

                // 3. Give the mDNSPosix layer a chance to add its information to the fd_set and timeout
                mDNSPosixGetFDSet(m, &nfds, &readfds, &timeout);

                // 4. Call select as normal
if(StopNow) {
//printf("BREAK!!!\n");
break;
}
		//printf("select(%d, %d.%06d)\n", nfds, timeout.tv_sec, timeout.tv_usec);
                result = select(nfds, &readfds, NULL, NULL, &timeout);

                if (result < 0)
                {
                        printf("select() returned %d errno %d", result, errno);
                        if (errno != EINTR) StopNow = mDNStrue;
                }
                else
                {
                        // 5. Call mDNSPosixProcessFDSet to let the mDNSPosix layer do its work
                        mDNSPosixProcessFDSet(m, &readfds);

                        event++;
                        //fprintf(stderr, "=== Event count: %d ===\n", event);
                        if( (event - callback) > 5 )
                                StopNow = 1;
                }
        }
}

static const char kDefaultServiceType[] = "_services._dns-sd._udp";
static const char kDefaultDomain[]      = "local.";
static const char *gServiceType      = kDefaultServiceType;
static const char *gServiceDomain    = kDefaultDomain;

#if 0
int main(int argc, char **argv)
{
        char hostname[64];
        memset(hostname, 0, 64);

        printf("argv[1]=%s=\n", argv[1]);
        strcpy(hostname, argv[1]);

        HostnameParse(hostname);
        printf("PARSED name=%s=\n", hostname);

        return 0;
}
#endif
int main(void)
    // The program's main entry point.  The program does a trivial 
    // mDNS query, looking for mDNS service & device in the local domain.
{
    	int     result;
    	mStatus     status;
    	DNSQuestion question;
    	domainname  type;
    	domainname  domain;
	char DeviceName[64];

    	// Initialise the mDNS core.
	status = mDNS_Init(&mDNSStorage, &PlatformStorage,
    	gRRCache, RR_CACHE_SIZE,
    	mDNS_Init_DontAdvertiseLocalAddresses,
    	mDNS_Init_NoInitCallback, mDNS_Init_NoInitCallbackContext);

    if (status == mStatus_NoError) {
#if 1 /* Query service type */
        // Construct and start the query.
        MakeDomainNameFromDNSNameString(&type, gServiceType);
        MakeDomainNameFromDNSNameString(&domain, gServiceDomain);

        status = mDNS_StartBrowse(&mDNSStorage, &question, &type, &domain, mDNSInterface_Any, mDNSfalse, BrowseCallback, NULL);
    
	//return;
        // Run the platform main event loop. 
        // The BrowseCallback routine is responsible for printing 
        // any results that we find.
        
        if (status == mStatus_NoError) {
            	fprintf(stderr, "* Query: %s\n", gServiceType);
        	EventLoop(&mDNSStorage);
            	mDNS_StopQuery(&mDNSStorage, &question);
                //Parse all service type and query detail
                //fprintf(stderr, "===== Parse all service type and query detail...\n");
                callback = 0;
                event = 0;
		StopNow = 0;
	}
	return 0;
#endif

#if 0 /* Query device name */
		Service_tmp = Service_list;
		while(Service_tmp!=NULL) {
			printf("\n======= Queried Service: %s.%s\n", Service_tmp->service, Service_tmp->type);
			sprintf(Query_type, "%s.%s", Service_tmp->service, Service_tmp->type);
			//Device_found = Service_tmp->sup_device;
			gServiceType = Query_type;
			MakeDomainNameFromDNSNameString(&type, gServiceType);
			status = mDNS_StartBrowse(&mDNSStorage, &question, &type, &domain, mDNSInterface_Any, mDNSfalse, QueryCallback, NULL);
			if (status == mStatus_NoError) {
		                EventLoop(&mDNSStorage);
        		        mDNS_StopQuery(&mDNSStorage, &question);
			}
                        callback = 0;
                        event = 0;
                        StopNow = 0;
                        Service_tmp = Service_tmp->next;
                }
		printf("===============\n\n");
#endif

#if 0 /* Query IP of device */
		DeviceIP_cur = Device_list;
		printf("%d,%d,%d  ", callback, event, StopNow);
		printf("QUERY DEVICE: %s/%s\n", Device_cur->name, Device_cur->IPv4Addr);
		char *cc = Device_cur->IPv4Addr;
		int x =0;
		for(; x<16; x++) {
			printf("%",*cc);
			cc++;
		}
		printf("\n");
		//if(DeviceIP_cur->IPv4Addr == NULL)
		if(!strcmp(DeviceIP_cur->IPv4Addr, ""))
			printf("    DeviceIP_cur->IPv4Addr == NULL\n");

		while(DeviceIP_cur && (!strcmp(DeviceIP_cur->IPv4Addr, ""))) {
			memset(DeviceName, 0, 64);
			strcpy(DeviceName, DeviceIP_cur->name);
			HostnameParse(DeviceName);
			gServiceType = DeviceName;
        		printf("Query IP of %s\n", gServiceType);
		        // Construct and start the query.
		       	MakeDomainNameFromDNSNameString(&type, gServiceType);
		        status = mDNS_GetAddrInfo(&mDNSStorage, &question, &type, &domain, mDNSInterface_Any, mDNSfalse, IPInfoCallback, NULL);
		        if (status == mStatus_NoError) {
		       	        EventLoop(&mDNSStorage);
                		mDNS_StopQuery(&mDNSStorage, &question);
		        }
			else {
				printf("Error= %ld\n", status);
			}
			DeviceIP_cur = DeviceIP_cur->next;
        	               callback = 0;
                        event = 0;
                       	StopNow = 0;
		}

		printf("===============\n");
#endif
int nvram_size, i = 0;
char *nvram_info;
char device_name_ascii[256], service_type_list[512];
struct mDNS_service_handler *handler;

nvram_size = device_count*64 + service_count*4;
nvram_info = malloc(nvram_size);
memset(nvram_info, 0, nvram_size);
printf("nvram_size= %d\n", nvram_size);

    	//Print all info
    	Service_tmp = Service_list;
    	while(Service_tmp!=NULL) {
		printf("\n=== %s.%s ===\n", Service_tmp->service, Service_tmp->type);
		Service_tmp = Service_tmp->next;
    	}

printf("\n===== Print device ========\n");
        Device_tmp = Device_list;
        while(Device_tmp!=NULL) {
                //printf("    %s:%s->", Device_tmp->name, Device_tmp->IPv4Addr);
		memset(device_name_ascii, 0, 256);
		char_to_ascii(device_name_ascii, Device_tmp->name);
		strcat(nvram_info, "<");
		sprintf(nvram_info, "%s%s>%s>", nvram_info, Device_tmp->IPv4Addr, device_name_ascii);
printf("    %s\n", nvram_info);
		Sup_service_tmp = Device_tmp->sup_service;
		memset(service_type_list, 0, 512);
		while(Sup_service_tmp!=NULL) {
			for(handler = &service_handler[0], i=0; handler->service; handler++, i++) {
				if(strstr(Sup_service_tmp->name, handler->service)) {
					sprintf(service_type_list, "%s#%d", service_type_list, i);
					break;
				}	
			}

			printf("%s ", Sup_service_tmp->name);
			Sup_service_tmp = Sup_service_tmp->next;
		}
		printf("\n");
		printf("Service_type= %s\n", service_type_list);
		sprintf(nvram_info, "%s%s", nvram_info, service_type_list);
printf("-> %s\n", nvram_info);
                Device_tmp = Device_tmp->next;
        }
	printf("\n==> %s\n", nvram_info);

	free(nvram_info);
	mDNS_Close(&mDNSStorage);
    }//Endof Initialise the mDNS core.
    
    if (status == mStatus_NoError) {
        result = 0;
    } else {
        result = 2;
    }
    if ( (result != 0) || (gMDNSPlatformPosixVerboseLevel > 0) ) {
        fprintf(stderr, "%s: Finished with status %d, result %d\n", gProgramName, (int)status, result);
    }

    return 0;
}


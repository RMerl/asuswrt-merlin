/*
 * swinst.c : hrSWInstalledTable data access
 */
/*
 * Copyright (C) 2007 Apple, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/library/container.h>
#include <net-snmp/library/dir_utils.h>
#include <net-snmp/library/snmp_debug.h>
#include <net-snmp/data_access/swinst.h>

#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#define __APPLE_API_EVOLVING 1
#include <sys/acl.h> /* or else CoreFoundation.h barfs */
#undef __APPLE_API_EVOLVING 

#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>

netsnmp_feature_require(container_directory)
netsnmp_feature_require(date_n_time)

/* ---------------------------------------------------------------------
 */
static int _add_applications_in_dir(netsnmp_container *, const char* path);
static int32_t _index;
static int _check_bundled_app(CFURLRef currentURL, CFStringRef *name,
                              CFStringRef *info, const char* path);
static int _check_classic_app(CFURLRef currentURL, CFStringRef *name,
                              CFStringRef *info, const char* path);
static netsnmp_container *dirs = NULL;

/* ---------------------------------------------------------------------
 */
void
netsnmp_swinst_arch_init( void )
{
    struct stat stat_buf;
    const char *default_dirs[] = {
        "/Applications",
        "/Applications (Mac OS 9)",
        "/System/Library/CoreServices",
        "/System/Library/Extensions",
        "/System/Library/Services"
#ifdef TEST
        , "/Developer/Applications"
        , "/Volumes/audX/Applications (Mac OS 9)"
#endif
    };
    int i, count = sizeof(default_dirs)/sizeof(default_dirs[0]);

    /*
     * create the container, if needed
     */
    if (NULL == dirs) {
        dirs = netsnmp_container_find("directory_container:cstring");
        if (NULL == dirs) {
            snmp_log(LOG_ERR, "couldn't allocate container for dir list\n");
            return;
        }
        dirs->container_name = strdup("directory search list");
        netsnmp_binary_array_options_set(dirs, 1, CONTAINER_KEY_UNSORTED);
    }

    /*
     * add dirs
     */
    for(i = 0; i < count; ++i) {
        char *      tmp;
        /** xxx: get/save the last mod date? */
        if(-1 == stat(default_dirs[i], &stat_buf)) {
            DEBUGMSGTL(("swinst:arch:darwin", "skipping dir %s\n",
                        default_dirs[i]));
            continue;
        }
        DEBUGMSGTL(("swinst:arch:darwin", "adding dir %s\n",
                        default_dirs[i]));
        tmp = strdup(default_dirs[i]);
        if (NULL == tmp) {
            snmp_log(LOG_ERR,"strdup failed\n");
            break;
        }
        CONTAINER_INSERT(dirs, tmp);
    }
}

void
netsnmp_swinst_arch_shutdown( void )
{
    netsnmp_directory_container_free(dirs);
}

/* ---------------------------------------------------------------------
 */

int
netsnmp_swinst_arch_load( netsnmp_container *container, u_int flags )
{
    netsnmp_iterator   *it;
    const char         *dir;
    int                 rc;

    DEBUGMSGTL(("swinst:arch:darwin", "load\n"));

    if (NULL == dirs) {
        DEBUGMSGTL(("swinst:arch:darwin", "no dirs to scan!\n"));
        return -1;
    }

    _index = 0;
    
    it = CONTAINER_ITERATOR(dirs);
    for (dir = ITERATOR_FIRST(it); dir; dir = ITERATOR_NEXT(it)) {
        rc = _add_applications_in_dir(container, dir);
    }
    ITERATOR_RELEASE(it);
    DEBUGMSGTL(("swinst:arch:darwin", "loaded %d apps\n",_index));

    return 0;
}

void  _dump_flags(u_long flags)
{
    static struct {
        const char*name;
        u_long bits;
    } names[] = {
        { "kLSItemInfoIsPlainFile", 0x00000001 },
        { "kLSItemInfoIsPackage", 0x00000002 },
        { "kLSItemInfoIsApplication", 0x00000004 },
        { "kLSItemInfoIsContainer", 0x00000008 },
        { "kLSItemInfoIsAliasFile", 0x00000010 },
        { "kLSItemInfoIsSymlink", 0x00000020 },
        { "kLSItemInfoIsInvisible", 0x00000040 },
        { "kLSItemInfoIsNativeApp", 0x00000080 },
        { "kLSItemInfoIsClassicApp", 0x00000100 },
        { "kLSItemInfoAppPrefersNative", 0x00000200 },
        { "kLSItemInfoAppPrefersClassic", 0x00000400 },
        { "kLSItemInfoAppIsScriptable", 0x00000800 },
        { "kLSItemInfoIsVolume", 0x00001000 },
        { "kLSItemInfoExtensionIsHidden", 0x00100000 }
    };
    int i, count = sizeof(names)/sizeof(names[0]);

    for(i = 0; i < count; ++i) {
        if (flags & names[i].bits) {
            DEBUGMSGTL(("swinst:arch:darwin:flags", "\t%s\n",
                       names[i].name));
        }
    }
}

static int
_add_applications_in_dir(netsnmp_container *container, const char* path)
{
    netsnmp_container  *files;
    netsnmp_iterator   *it;
    const char         *file;
    netsnmp_swinst_entry *entry = NULL;
    struct stat	        stat_buf;
    size_t              date_len;
    u_char             *date_buf;
    int                 rc = 0;

    CFStringRef         currentPath = NULL;
    CFURLRef            currentURL = NULL;
    LSItemInfoRecord    itemInfoRecord;
    CFStringRef         prodName = NULL;
    CFStringRef         version = NULL;
    
    DEBUGMSGTL(("swinst:arch:darwin", " adding files from %s\n", path));
    files = netsnmp_directory_container_read(NULL, path, 0);
    if (NULL == files) {
        snmp_log(LOG_ERR, "swinst: could not read directory %s\n", path);
        return -1;
    }

    it = CONTAINER_ITERATOR(files);
    if (NULL == it) {
        snmp_log(LOG_ERR, "could not get iterator\n");
        netsnmp_directory_container_free(files);
        return -1;
    }
    for (file = ITERATOR_FIRST(it);
         file;
         file = ITERATOR_NEXT(it),
             CFRelease(currentPath),
             CFRelease(currentURL)) {

        int                 rc2 = 0;
        
        currentPath =
            CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, file,
                                            kCFStringEncodingUTF8,
                                            kCFAllocatorNull);
        currentURL =
            CFURLCreateWithFileSystemPath(kCFAllocatorDefault, currentPath,
                                          kCFURLPOSIXPathStyle, true); 
        LSCopyItemInfoForURL(currentURL,
                             kLSRequestBasicFlagsOnly|kLSRequestAppTypeFlags,
                             &itemInfoRecord); 
        if((0 == itemInfoRecord.flags) ||
           (kLSItemInfoIsPlainFile == itemInfoRecord.flags) ||
           (itemInfoRecord.flags & kLSItemInfoIsInvisible) ||
           (itemInfoRecord.flags & kLSItemInfoIsAliasFile)) {
            continue;
        }
        /** recurse on non-application containers (i.e. directory) */
        if ((itemInfoRecord.flags & kLSItemInfoIsContainer) &&
            (!(itemInfoRecord.flags & kLSItemInfoIsApplication))) {
            netsnmp_directory_container_read(files, file, 0);
            continue;
       }

        /** skip any other non-application files */
        if (!(itemInfoRecord.flags & kLSItemInfoIsApplication)) {
            continue;
       }

        if ((itemInfoRecord.flags & kLSItemInfoIsPackage) ||           
            (itemInfoRecord.flags & kLSItemInfoIsContainer)) {
            rc2 = _check_bundled_app(currentURL, &prodName, &version, file);
        } 
        else if ((itemInfoRecord.flags & kLSItemInfoIsClassicApp) ||
                 (itemInfoRecord.flags & kLSItemInfoIsPlainFile)) {
            rc2 = _check_classic_app(currentURL, &prodName, &version, file);
        } else {
            snmp_log(LOG_ERR,"swinst shouldn't get here: %s\n", file);
            _dump_flags(itemInfoRecord.flags);
            continue;
        }
        if (rc2) { /* not an app. if directory, recurse; else continue */
            _dump_flags(itemInfoRecord.flags);
            if (1 == rc2)
                netsnmp_directory_container_read(files, file, 0);
            continue;
        }
        
        /*
         * allocate entry
         */
        entry = netsnmp_swinst_entry_create(++_index);
        if (NULL == entry) {
            snmp_log(LOG_ERR, "error creating swinst entry\n");
            rc = -1;
            SNMP_CFRelease(prodName);
            SNMP_CFRelease(version);
            break;
        }

        entry->swName_len =
            snprintf(entry->swName, sizeof(entry->swName),
                     "%s %s", CFStringGetCStringPtr(prodName,0),
                     CFStringGetCStringPtr(version,0));
	if (entry->swName_len >= sizeof(entry->swName))
	    entry->swName_len = sizeof(entry->swName)-1;

        DEBUGMSGTL(("swinst:arch:darwin", "\t%s %s\n", file, entry->swName));

        /** get the last mod date */
        if(stat(file, &stat_buf) != -1) {
            date_buf = date_n_time(&stat_buf.st_mtime, &date_len);
            entry->swDate_len = date_len;
            memcpy(entry->swDate, date_buf, entry->swDate_len);
        }
        
        CONTAINER_INSERT(container, entry);
        entry = NULL;
        SNMP_CFRelease(prodName);
        SNMP_CFRelease(version);
    }
    ITERATOR_RELEASE(it);
    netsnmp_directory_container_free(files);

    return rc;
}

int
_check_bundled_app(CFURLRef currentURL, CFStringRef *prodName,
                   CFStringRef *version, const char* file)
{
    CFBundleRef         theBundle = NULL;
    CFDictionaryRef     infoDict = NULL;
            
    if ((NULL == prodName) || (NULL == version))
       return -1;

    theBundle = CFBundleCreate (kCFAllocatorDefault, currentURL);
    if(theBundle == NULL)
        return -1; /* not a bundle */

    infoDict = CFBundleGetInfoDictionary(theBundle);
    if(0 == CFDictionaryGetCount(infoDict)) {
        SNMP_CFRelease(theBundle);
        return 1; /* directory */
    }

    *prodName = (CFStringRef)
        CFDictionaryGetValue (infoDict, CFSTR("CFBundleName"));
    if (NULL == *prodName) {
        *prodName = (CFStringRef)
            CFDictionaryGetValue (infoDict, CFSTR("CFBundleDisplayName"));
        if (NULL == *prodName) {
            *prodName = (CFStringRef) CFDictionaryGetValue (infoDict,
                                      CFSTR("CFBundleExecutable"));
        }
    }
    if(NULL == *prodName) {
        DEBUGMSGTL(("swinst:arch:darwin", "\tmissing name: %s\n",file));
        /*CFShow(infoDict);*/
        SNMP_CFRelease(theBundle);
        return -1;
    }

    *version = (CFStringRef)
        CFDictionaryGetValue (infoDict, CFSTR("CFBundleShortVersionString"));
    if(NULL == *version) {
        *version = (CFStringRef)
            CFDictionaryGetValue (infoDict, CFSTR("CFBundleVersion"));
        if (*version == NULL) 
            *version = (CFStringRef) CFDictionaryGetValue (infoDict,
                                      CFSTR("CFBundleGetInfoString"));
    }
    if(NULL == *version) {
        DEBUGMSGTL(("swinst:arch:darwin", "\tmissing version: %s\n",file));
        /*CFShow(infoDict);*/
        SNMP_CFRelease(theBundle);
        return -1;
    }
    
    if(theBundle != NULL) {
        CFRetain(*prodName);
        CFRetain(*version);
        SNMP_CFRelease(theBundle);
    }
    return 0;
}

static int
_check_classic_app(CFURLRef currentURL, CFStringRef *prodName,
                   CFStringRef *version, const char* file)
{
    /*
     * get info for classic or single-file apps
     */
    FSRef theFSRef;
    int theResFile;

    if ((NULL == prodName) || (NULL == version))
       return -1;

    *prodName = CFURLCopyLastPathComponent(currentURL);
    if (! CFURLGetFSRef(currentURL, &theFSRef)) {
        DEBUGMSGTL(("swinst:arch:darwin", "GetFSRef failed: %s\n", file));
        SNMP_CFRelease(*prodName);
        return -1;
    }
    theResFile = FSOpenResFile(&theFSRef, fsRdPerm);
    if (ResError() != noErr) {
        DEBUGMSGTL(("swinst:arch:darwin", "FSOpenResFile failed: %s\n", file));
        SNMP_CFRelease(*prodName);
        return -1;
    }
    VersRecHndl versHandle = (VersRecHndl)Get1IndResource('vers', 1);
    if (versHandle != NULL) {
        *version = CFStringCreateWithPascalString(kCFAllocatorDefault,
                       (**versHandle).shortVersion, kCFStringEncodingMacRoman);
        if (*version == NULL) {
            StringPtr longVersionPtr = (**versHandle).shortVersion;
            longVersionPtr = (StringPtr)(((Ptr) longVersionPtr) +
                              1 + ((unsigned char) *longVersionPtr));
            *version = CFStringCreateWithPascalString(kCFAllocatorDefault,
                          longVersionPtr,  kCFStringEncodingMacRoman);
        }
        ReleaseResource((Handle)versHandle);
    }
    CloseResFile(theResFile);
    if(*version == NULL) {
        DEBUGMSGTL(("swinst:arch:darwin",
                    "\tmissing classic/file version: %s\n", file));
        SNMP_CFRelease(*prodName);
        return -1;
    }

    return 0;
}

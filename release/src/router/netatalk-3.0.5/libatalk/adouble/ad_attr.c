#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <arpa/inet.h>
#include <atalk/adouble.h>
#include <atalk/logger.h>

#define FILEIOFF_ATTR 14
#define AFPFILEIOFF_ATTR 2

/* 
   Note:
   the "shared" and "invisible" attributes are opaque and stored and
   retrieved from the FinderFlags. This fixes Bug #2802236:
   <https://sourceforge.net/tracker/?func=detail&aid=2802236&group_id=8642&atid=108642>
 */
int ad_getattr(const struct adouble *ad, uint16_t *attr)
{
    uint16_t fflags;
    *attr = 0;

    if (ad_getentryoff(ad, ADEID_AFPFILEI)) {
        memcpy(attr, ad_entry(ad, ADEID_AFPFILEI) + AFPFILEIOFF_ATTR, 2);

        /* Now get opaque flags from FinderInfo */
        memcpy(&fflags, ad_entry(ad, ADEID_FINDERI) + FINDERINFO_FRFLAGOFF, 2);
        if (fflags & htons(FINDERINFO_INVISIBLE))
            *attr |= htons(ATTRBIT_INVISIBLE);
        else
            *attr &= htons(~ATTRBIT_INVISIBLE);
        /*
         * This one is tricky, I actually got it wrong the first time:
         * for directories bit 1<<1 is ATTRBIT_EXPFLDR and is NOT opaque !
         */
        if ( ! (ad->ad_adflags & ADFLAGS_DIR)) {
            if (fflags & htons(FINDERINFO_ISHARED))
                *attr |= htons(ATTRBIT_MULTIUSER);
            else
                *attr &= htons(~ATTRBIT_MULTIUSER);
        }
    }

    *attr |= htons(ad->ad_open_forks);

    return 0;
}

/* ----------------- */
int ad_setattr(const struct adouble *ad, const uint16_t attribute)
{
    uint16_t fflags;

    /* we don't save open forks indicator */
    uint16_t attr = attribute & ~htons(ATTRBIT_DOPEN | ATTRBIT_ROPEN);

    /* Proactively (10.4 does indeed try to set ATTRBIT_MULTIUSER (=ATTRBIT_EXPFLDR)
       for dirs with SetFile -a M <dir> ) disable all flags not defined for dirs. */
    if (ad->ad_adflags & ADFLAGS_DIR)
        attr &= ~(ATTRBIT_MULTIUSER | ATTRBIT_NOWRITE | ATTRBIT_NOCOPY);

    if (ad_getentryoff(ad, ADEID_AFPFILEI) && ad_getentryoff(ad, ADEID_FINDERI)) {
        memcpy(ad_entry(ad, ADEID_AFPFILEI) + AFPFILEIOFF_ATTR, &attr, sizeof(attr));
            
        /* Now set opaque flags in FinderInfo too */
        memcpy(&fflags, ad_entry(ad, ADEID_FINDERI) + FINDERINFO_FRFLAGOFF, 2);
        if (attr & htons(ATTRBIT_INVISIBLE))
            fflags |= htons(FINDERINFO_INVISIBLE);
        else
            fflags &= htons(~FINDERINFO_INVISIBLE);

        /* See above comment in ad_getattr() */
        if (attr & htons(ATTRBIT_MULTIUSER)) {
            if ( ! (ad->ad_adflags & ADFLAGS_DIR) )
                fflags |= htons(FINDERINFO_ISHARED);
        } else
            fflags &= htons(~FINDERINFO_ISHARED);

        memcpy(ad_entry(ad, ADEID_FINDERI) + FINDERINFO_FRFLAGOFF, &fflags, 2);
    }

    return 0;
}

/* --------------
 * save file/folder ID in AppleDoubleV2 netatalk private parameters
 * return 1 if resource fork has been modified
 */
int ad_setid (struct adouble *adp, const dev_t dev, const ino_t ino , const uint32_t id, const cnid_t did, const void *stamp)
{
    uint32_t tmp;

    ad_setentrylen( adp, ADEID_PRIVID, sizeof(id));
    tmp = id;
    if (adp->ad_vers == AD_VERSION_EA)
        tmp = htonl(tmp);
    memcpy(ad_entry( adp, ADEID_PRIVID ), &tmp, sizeof(tmp));

    ad_setentrylen( adp, ADEID_PRIVDEV, sizeof(dev_t));
    if ((adp->ad_options & ADVOL_NODEV)) {
        memset(ad_entry( adp, ADEID_PRIVDEV ), 0, sizeof(dev_t));
    } else {
        memcpy(ad_entry( adp, ADEID_PRIVDEV ), &dev, sizeof(dev_t));
    }

    ad_setentrylen( adp, ADEID_PRIVINO, sizeof(ino_t));
    memcpy(ad_entry( adp, ADEID_PRIVINO ), &ino, sizeof(ino_t));

    ad_setentrylen( adp, ADEID_DID, sizeof(did));
    memcpy(ad_entry( adp, ADEID_DID ), &did, sizeof(did));

    ad_setentrylen( adp, ADEID_PRIVSYN, ADEDLEN_PRIVSYN);
    memcpy(ad_entry( adp, ADEID_PRIVSYN ), stamp, ADEDLEN_PRIVSYN);

    return 1;
}

/* ----------------------------- */
uint32_t ad_getid (struct adouble *adp, const dev_t st_dev, const ino_t st_ino , const cnid_t did, const void *stamp _U_)
{
    uint32_t aint = 0;
    dev_t  dev;
    ino_t  ino;
    cnid_t a_did;

    if (adp) {
        if (sizeof(dev_t) == ad_getentrylen(adp, ADEID_PRIVDEV)) {
            memcpy(&dev, ad_entry(adp, ADEID_PRIVDEV), sizeof(dev_t));
            memcpy(&ino, ad_entry(adp, ADEID_PRIVINO), sizeof(ino_t));
            memcpy(&a_did, ad_entry(adp, ADEID_DID), sizeof(cnid_t));

            if (((adp->ad_options & ADVOL_NODEV) || (dev == st_dev))
                && ino == st_ino
                && (!did || a_did == did) ) {
                memcpy(&aint, ad_entry(adp, ADEID_PRIVID), sizeof(aint));
                if (adp->ad_vers == AD_VERSION2)
                    return aint;
                else
                    return ntohl(aint);
            }
        }
    }
    return 0;
}

/* ----------------------------- */
uint32_t ad_forcegetid (struct adouble *adp)
{
    uint32_t aint = 0;

    if (adp) {
        memcpy(&aint, ad_entry(adp, ADEID_PRIVID), sizeof(aint));
        if (adp->ad_vers == AD_VERSION2)
            return aint;
        else
            return ntohl(aint);
    }
    return 0;
}

/* -----------------
 * set resource fork filename attribute.
 */
int ad_setname(struct adouble *ad, const char *path)
{
    int len;
    if ((len = strlen(path)) > ADEDLEN_NAME)
        len = ADEDLEN_NAME;
    if (path && ad_getentryoff(ad, ADEID_NAME)) {
        ad_setentrylen( ad, ADEID_NAME, len);
        memcpy(ad_entry( ad, ADEID_NAME ), path, len);
        return 1;
    }
    return 0;
}

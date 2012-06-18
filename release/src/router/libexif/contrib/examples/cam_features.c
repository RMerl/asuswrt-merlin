/***************************************************************************
 *            cam_features.c
 *
 *  Wed Jul 27 11:25:09 2005
 *  Copyright  2005  User: Naysawn Naderi
 *  Email: ndn at xiphos dot ca
 *
 * Uses libdc1394 and libraw1394
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/times.h>
#include <errno.h>

#include <libraw1394/raw1394.h>
#include <libdc1394/dc1394_control.h>
#include <libdc1394/dc1394_register.h>

//EXIF includes
#include <libexif/exif-data.h>
#include <libexif/exif-ifd.h>
#include <libexif/exif-loader.h>

// Part of the exif command-line source package
#include "libjpeg/jpeg-data.h"


#define FILENAME "test.jpg"


static int createEXIF(dc1394featureset_t *xFeatures, ExifData ** pParentEd);


int main(int argc, char *argv[])
{   dc1394camera_t *pCamera, **pCameras=NULL;
    int iNumCameras;
    dc1394featureset_t xFeatures;
    int i;
    int err=dc1394_find_cameras(&pCameras, &iNumCameras);

    //EXIF STUFF
    JPEGData *pData;
    //float fOnefloat;
    ExifData * pEd;


    if (err!=DC1394_SUCCESS) {
        fprintf( stderr, "Unable to look for cameras\n\n"
            "Please check \n"
            "  - if the kernel modules `ieee1394',`raw1394' and `ohci1394' are loaded \n"
            "  - if you have read/write access to /dev/raw1394\n\n");
        exit(1);
    }


    /*-----------------------------------------------------------------------
     *  Initialize the camera
     *-----------------------------------------------------------------------*/
    if (iNumCameras<1) {
        fprintf(stderr, "no cameras found :(\n");
        exit(1);
    }
    pCamera=pCameras[0];
    for (i=1;i<iNumCameras;i++)
        dc1394_free_camera(pCameras[i]);
    free(pCameras);

    if(dc1394_get_camera_feature_set(pCamera, &xFeatures)!=DC1394_SUCCESS)
            fprintf(stdout, "unable to get feature set\n");
    else
            printf("camera's feature set retrieved\n");

    createEXIF(&xFeatures, &pEd);  //tag the file with the settings of the camera

    //exif_data_dump (pEd);

    //write the Exif data to a jpeg file
    pData = jpeg_data_new_from_file (FILENAME);  //input data
    if (!pData) {
        printf ("Could not load '%s'!\n", FILENAME);
        return (-1);
    }

    printf("Saving EXIF data to jpeg file\n");
    jpeg_data_set_exif_data (pData, pEd);
    printf("Set the data\n");
    jpeg_data_save_file(pData, "foobar2.jpg");

    return 0;

}


int createEXIF(dc1394featureset_t *xFeatures, ExifData ** pParentEd)
{
    ExifEntry *pE;
    ExifData * pEd;
    int i = !xFeatures->feature[DC1394_FEATURE_WHITE_BALANCE - DC1394_FEATURE_MIN].auto_active;

    ExifSRational xR = {xFeatures->feature[DC1394_FEATURE_BRIGHTNESS - DC1394_FEATURE_MIN].value, xFeatures->feature[DC1394_FEATURE_BRIGHTNESS - DC1394_FEATURE_MIN].max};;

    printf ("Creating EXIF data...\n");
    pEd = exif_data_new ();

    /*

    Things to tag:

    EXIF_TAG_MAKE               = 0x010f,
    EXIF_TAG_MODEL              = 0x0110,
    EXIF_TAG_EXPOSURE_TIME      = 0x829a,
    EXIF_TAG_BRIGHTNESS_VALUE   = 0x9203,
    EXIF_TAG_WHITE_BALANCE      = 0xa403,
    EXIF_TAG_GAIN_CONTROL       = 0xa407,
    EXIF_TAG_CONTRAST           = 0xa408,
    EXIF_TAG_SATURATION         = 0xa409,
    EXIF_TAG_SHARPNESS          = 0xa40a,
    EXIF_TAG_USER_COMMENT
    */

    printf ("Adding a Make reference\n");
    pE = exif_entry_new ();
    exif_content_add_entry (pEd->ifd[EXIF_IFD_0], pE);
    exif_entry_initialize (pE, EXIF_TAG_MAKE);
    pE->data="AVT";
    exif_entry_unref (pE);

    printf ("Adding a Model reference\n");
    pE = exif_entry_new ();
    exif_content_add_entry (pEd->ifd[EXIF_IFD_0], pE);
    exif_entry_initialize (pE, EXIF_TAG_MODEL);
    pE->data="510c";
    exif_entry_unref (pE);

    printf ("Adding a Tag to reference # samples per pixel\n");
    pE = exif_entry_new ();
    exif_content_add_entry (pEd->ifd[EXIF_IFD_0], pE);
    exif_entry_initialize (pE, EXIF_TAG_SAMPLES_PER_PIXEL); //by default is 3
    exif_entry_unref (pE);

    printf ("Adding a White Balance Reference\n");
    pE = exif_entry_new ();
    exif_content_add_entry (pEd->ifd[EXIF_IFD_0], pE);
    exif_entry_initialize (pE, EXIF_TAG_WHITE_BALANCE);
    exif_set_short(pE->data, exif_data_get_byte_order (pEd), i);  //0=auto white balance, 1 = manual white balance
    exif_entry_unref (pE);

    //need to create logic according to the value of the sharpness
    printf ("Adding a Sharpness Reference\n");
    pE = exif_entry_new ();
    exif_content_add_entry (pEd->ifd[EXIF_IFD_0], pE);
    exif_entry_initialize (pE, EXIF_TAG_SHARPNESS);
    exif_set_short(pE->data, exif_data_get_byte_order (pEd), 0);
    exif_entry_unref (pE);

    printf ("Adding a Brightness reference\n");

    //try to get brightness
    //printf("Float Value: %i\n",xFeatures->feature[DC1394_FEATURE_BRIGHTNESS - DC1394_FEATURE_MIN].value);

    pE = exif_entry_new ();
    exif_content_add_entry (pEd->ifd[EXIF_IFD_0], pE);
    exif_entry_initialize (pE, EXIF_TAG_BRIGHTNESS_VALUE);
    exif_set_srational (pE->data, exif_data_get_byte_order (pEd), xR);


    //exif_data_dump (ed);
    //exif_data_dump (pEd);
    *pParentEd = pEd;
    printf("Done!\n");

    return 0;
}

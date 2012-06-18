/*
 * libexif example program to extract an EXIF thumbnail from an image
 * and save it into a new file.
 *
 * Placed into the public domain by Dan Fandrich
 */

#include <stdio.h>
#include <libexif/exif-loader.h>

int main(int argc, char **argv)
{
    int rc = 1;
    ExifLoader *l;

    if (argc < 2) {
        printf("Usage: %s image.jpg\n", argv[0]);
        printf("Extracts a thumbnail from the given EXIF image.\n");
        return rc;
    }

    /* Create an ExifLoader object to manage the EXIF loading process */
    l = exif_loader_new();
    if (l) {
        ExifData *ed;

        /* Load the EXIF data from the image file */
        exif_loader_write_file(l, argv[1]);

        /* Get a pointer to the EXIF data */
        ed = exif_loader_get_data(l);

	/* The loader is no longer needed--free it */
        exif_loader_unref(l);
	l = NULL;
        if (ed) {
	    /* Make sure the image had a thumbnail before trying to write it */
            if (ed->data && ed->size) {
                FILE *thumb;
                char thumb_name[1024];

		/* Try to create a unique name for the thumbnail file */
                snprintf(thumb_name, sizeof(thumb_name),
                         "%s_thumb.jpg", argv[1]);

                thumb = fopen(thumb_name, "wb");
                if (thumb) {
		    /* Write the thumbnail image to the file */
                    fwrite(ed->data, 1, ed->size, thumb);
                    fclose(thumb);
                    printf("Wrote thumbnail to %s\n", thumb_name);
                    rc = 0;
                } else {
                    printf("Could not create file %s\n", thumb_name);
                    rc = 2;
                }
            } else {
                printf("No EXIF thumbnail in file %s\n", argv[1]);
                rc = 1;
            }
	    /* Free the EXIF data */
            exif_data_unref(ed);
        }
    }
    return rc;
}

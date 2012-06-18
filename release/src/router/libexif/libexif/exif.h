/*! \file exif.h exif/exif.h
 * \brief Dummy header file for documentation purposes
 * \date 2007
 * \author Hans Ulrich Niedermann, et. al.
 *
 * \mainpage The libexif library
 *
 * \section general_notes General Notes
 *
 * This documentation is work in progress, as is the code itself.
 *
 * \section using_libexif Using libexif
 *
 * \#include <libexif/exif-data.h>
 *
 * libexif provides a libexif.pc file for use with pkgconfig on the
 * libexif installation. If you are using libtool to build your
 * package, you can also make use of libexif-uninstalled.pc.
 *
 * An application using libexif would typically first create an #ExifLoader to
 * load EXIF data into memory. From there, it would extract that data as
 * an #ExifData to start manipulating it. Each IFD is represented by its own
 * #ExifContent within that #ExifData, which contains all the tag data in
 * #ExifEntry form.  If the MakerNote data is required, an #ExifMnoteData
 * can be extracted from the #ExifData and manipulated with the MakerNote
 * functions.
 *
 * libexif is written in C using an object-based style that defines
 * sets of functions that operate on each data structure.
 *
 * \section data_structures Primary Data Structures
 *
 * #ExifLoader
 * State maintained by the loader interface while importing EXIF data
 * from an external file or memory
 *
 * #ExifData
 * The entirety of EXIF data found in an image
 *
 * #ExifContent
 * All EXIF tags in a single IFD
 *
 * #ExifEntry
 * Data found in a single EXIF tag
 *
 * #ExifMnoteData
 * All data found in the MakerNote tag
 *
 * #ExifLog
 * State maintained by the logging interface
 *
 * \section string_conventions String Conventions
 *
 * Strings are 8 bit characters ("char*"). When libexif is compiled with
 * NLS, character set and encoding are as set in the current locale,
 * except for strings that come directly from the data in EXIF
 * tags which are generally returned in raw form.  Most EXIF strings are
 * defined to be plain 7-bit ASCII so this raw form should be acceptable in
 * any UNIX locale, but some software ignores the specification and
 * writes 8-bit characters.  It is up to the application to detect this
 * and deal with it intelligently.
 *
 * \section memory_management Memory Management Patterns
 *
 * For pointers to data objects, libexif uses reference counting. The
 * pattern is to use the foo_new() function to create a data object,
 * foo_ref() to increase the reference counter, and foo_unref() to
 * decrease the reference counter and possibly free(3)ing the memory.
 *
 * Libexif by default relies on the calloc(3), realloc(3), and free(3)
 * functions, but the libexif user can tell libexif to use their
 * special memory management functions at runtime.
 * 
 * \section thread_safety Thread Safety
 * 
 * libexif is thread safe when the underlying C library is also thread safe.
 * Some C libraries may require defining a special macro (like _REENTRANT)
 * to ensure this, or may require linking to a special thread-safe version of
 * the library.
 *
 * The programmer must ensure that each object allocated by libexif is only
 * used in a single thread at once. For example, an ExifData* allocated
 * in one thread can't be used in a second thread if there is any chance
 * that the first thread could use it at the same time. Multiple threads
 * can use libexif without issues if they never share handles.
 *
 */

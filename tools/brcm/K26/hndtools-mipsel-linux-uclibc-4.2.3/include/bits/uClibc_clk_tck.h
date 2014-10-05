/* Use a default of 100 for CLK_TCK to implement sysconf() and clock().
 * Override this by supplying an arch-specific version of this header file.
 *
 * WARNING: It is assumed that this is a constant integer value usable in
 * preprocessor conditionals!!!
 */

#define __UCLIBC_CLK_TCK_CONST		100

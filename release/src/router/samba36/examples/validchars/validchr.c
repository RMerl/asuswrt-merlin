/* by tino@augsburg.net
 */

#include <stdio.h>
#include <string.h>

#include <dirent.h>

unsigned char
test(void)
{
  DIR		*dir;
  struct dirent	*dp;
  unsigned char	c;

  if ((dir=opendir("."))==0)
    {
      perror("open .");
      return 0;
    }
  c	= 0;
  while ((dp=readdir(dir))!=0)
    {
      size_t len;

      len	= strlen(dp->d_name);
      if (len<4)
	continue;
      if (strcmp(dp->d_name+len-4, ".TST"))
	continue;
      if (len!=5)
	{
	  fprintf(stderr, "warning: %s\n", dp->d_name);
	  printf(" length");
	  continue;
	}
      if (c)
	printf(" double%d\n", c);
      c	= dp->d_name[0];
    }
  if (closedir(dir))
    perror("close .");
  return c;
}

int
main(void)
{
  char		name[256];
  unsigned char	map[256], upper[256], lower[256];
  int		i, j, c;
  FILE		*fd;

  if (test())
    {
      printf("There are *.TST files, please remove\n");
      return 0;
    }
  for (i=0; ++i<256; )
    {
      lower[i]	= i;
      upper[i]	= 0;
    }
  for (i=256; --i; )
    {
      map[i]	= i;
      strcpy(name, "..TST");
      name[0]	= i;
      printf("%d:", i);
      if ((fd=fopen(name, "w"))==0)
	printf(" open");
      else
	fclose(fd);
      c	= test();
      if (unlink(name))
	printf(" unlink");
      if (c==i)
	printf(" ok");
      else
	printf(" %d", c);
      printf("\n");
      if (c!=i)
	{
	  upper[c]++;
	  lower[c]	= i;
        }
      map[i]	= c;
    }

  /* Uppercase characters are detected above on:
   * The character is mapped to itself and there is a
   * character which maps to it.
   * Lowercase characters are the lowest character pointing to another one.
   * Else it is a one way character.
   *
   * For this reason we have to process the list
   * 1) for 'one way' characters
   *	'one way' is something which is no upper and no lower character.
   *	This is an awful, crude and ugly hack due to missing Samba support.
   * 2) for true uppercase/lowercase characters
   * 3) for standalone characters
   * Note that there might be characters which do not fall into 1 to 3.
   */
  printf("\n   valid chars =");
  for (i=0; ++i<256; )
    if (map[i] && map[i]!=i && lower[map[i]]!=i)
      {
	if (!upper[i])
	  printf(" %d:%d %d:%d %d:%d",					/*1*/
		 map[i], i, i, map[i], map[i], map[i]);
	else
	  fprintf(stderr, "ignoring map %d->%d because of %d->%d\n",
		  lower[i], i, i, map[i]);
      }
  for (i=0; ++i<256; )
    if (map[i] && map[i]==i)
      if (upper[i])
	printf(" %d:%d", lower[i], i);					/*2*/
      else
	printf(" %d", i);						/*3*/
  printf("\n");
  return 0;
}

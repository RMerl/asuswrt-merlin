/* Decomposition and composition of Unicode strings.
   Copyright (C) 2009-2017 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2009.

   This program is free software: you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published
   by the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

UNIT *
FUNC (uninorm_t nf, const UNIT *s, size_t n,
      UNIT *resultbuf, size_t *lengthp)
{
  int (*decomposer) (ucs4_t uc, ucs4_t *decomposition) = nf->decomposer;
  ucs4_t (*composer) (ucs4_t uc1, ucs4_t uc2) = nf->composer;

  /* The result being accumulated.  */
  UNIT *result;
  size_t length;
  size_t allocated;
  /* The buffer for sorting.  */
  #define SORTBUF_PREALLOCATED 64
  struct ucs4_with_ccc sortbuf_preallocated[2 * SORTBUF_PREALLOCATED];
  struct ucs4_with_ccc *sortbuf; /* array of size 2 * sortbuf_allocated */
  size_t sortbuf_allocated;
  size_t sortbuf_count;

  /* Initialize the accumulator.  */
  if (resultbuf == NULL)
    {
      result = NULL;
      allocated = 0;
    }
  else
    {
      result = resultbuf;
      allocated = *lengthp;
    }
  length = 0;

  /* Initialize the buffer for sorting.  */
  sortbuf = sortbuf_preallocated;
  sortbuf_allocated = SORTBUF_PREALLOCATED;
  sortbuf_count = 0;

  {
    const UNIT *s_end = s + n;

    for (;;)
      {
        int count;
        ucs4_t decomposed[UC_DECOMPOSITION_MAX_LENGTH];
        int decomposed_count;
        int i;

        if (s < s_end)
          {
            /* Fetch the next character.  */
            count = U_MBTOUC_UNSAFE (&decomposed[0], s, s_end - s);
            decomposed_count = 1;

            /* Decompose it, recursively.
               It would be possible to precompute the recursive decomposition
               and store it in a table.  But this would significantly increase
               the size of the decomposition tables, because for example for
               U+1FC1 the recursive canonical decomposition and the recursive
               compatibility decomposition are different.  */
            {
              int curr;

              for (curr = 0; curr < decomposed_count; )
                {
                  /* Invariant: decomposed[0..curr-1] is fully decomposed, i.e.
                     all elements are atomic.  */
                  ucs4_t curr_decomposed[UC_DECOMPOSITION_MAX_LENGTH];
                  int curr_decomposed_count;

                  curr_decomposed_count = decomposer (decomposed[curr], curr_decomposed);
                  if (curr_decomposed_count >= 0)
                    {
                      /* Move curr_decomposed[0..curr_decomposed_count-1] over
                         decomposed[curr], making room.  It's not worth using
                         memcpy() here, since the counts are so small.  */
                      int shift = curr_decomposed_count - 1;

                      if (shift < 0)
                        abort ();
                      if (shift > 0)
                        {
                          int j;

                          decomposed_count += shift;
                          if (decomposed_count > UC_DECOMPOSITION_MAX_LENGTH)
                            abort ();
                          for (j = decomposed_count - 1 - shift; j > curr; j--)
                            decomposed[j + shift] = decomposed[j];
                        }
                      for (; shift >= 0; shift--)
                        decomposed[curr + shift] = curr_decomposed[shift];
                    }
                  else
                    {
                      /* decomposed[curr] is atomic.  */
                      curr++;
                    }
                }
            }
          }
        else
          {
            count = 0;
            decomposed_count = 0;
          }

        i = 0;
        for (;;)
          {
            ucs4_t uc;
            int ccc;

            if (s < s_end)
              {
                /* Fetch the next character from the decomposition.  */
                if (i == decomposed_count)
                  break;
                uc = decomposed[i];
                ccc = uc_combining_class (uc);
              }
            else
              {
                /* End of string reached.  */
                uc = 0;
                ccc = 0;
              }

            if (ccc == 0)
              {
                size_t j;

                /* Apply the canonical ordering algorithm to the accumulated
                   sequence of characters.  */
                if (sortbuf_count > 1)
                  gl_uninorm_decompose_merge_sort_inplace (sortbuf, sortbuf_count,
                                                           sortbuf + sortbuf_count);

                if (composer != NULL)
                  {
                    /* Attempt to combine decomposed characters, as specified
                       in the Unicode Standard Annex #15 "Unicode Normalization
                       Forms".  We need to check
                         1. whether the first accumulated character is a
                            "starter" (i.e. has ccc = 0).  This is usually the
                            case.  But when the string starts with a
                            non-starter, the sortbuf also starts with a
                            non-starter.  Btw, this check could also be
                            omitted, because the composition table has only
                            entries (code1, code2) for which code1 is a
                            starter; if the first accumulated character is not
                            a starter, no lookup will succeed.
                         2. If the sortbuf has more than one character, check
                            for each of these characters that are not "blocked"
                            from the starter (i.e. have a ccc that is higher
                            than the ccc of the previous character) whether it
                            can be combined with the first character.
                         3. If only one character is left in sortbuf, check
                            whether it can be combined with the next character
                            (also a starter).  */
                    if (sortbuf_count > 0 && sortbuf[0].ccc == 0)
                      {
                        for (j = 1; j < sortbuf_count; )
                          {
                            if (sortbuf[j].ccc > sortbuf[j - 1].ccc)
                              {
                                ucs4_t combined =
                                  composer (sortbuf[0].code, sortbuf[j].code);
                                if (combined)
                                  {
                                    size_t k;

                                    sortbuf[0].code = combined;
                                    /* sortbuf[0].ccc = 0, still valid.  */
                                    for (k = j + 1; k < sortbuf_count; k++)
                                      sortbuf[k - 1] = sortbuf[k];
                                    sortbuf_count--;
                                    continue;
                                  }
                              }
                            j++;
                          }
                        if (s < s_end && sortbuf_count == 1)
                          {
                            ucs4_t combined =
                              composer (sortbuf[0].code, uc);
                            if (combined)
                              {
                                uc = combined;
                                ccc = 0;
                                /* uc could be further combined with subsequent
                                   characters.  So don't put it into sortbuf[0] in
                                   this round, only in the next round.  */
                                sortbuf_count = 0;
                              }
                          }
                      }
                  }

                for (j = 0; j < sortbuf_count; j++)
                  {
                    ucs4_t muc = sortbuf[j].code;

                    /* Append muc to the result accumulator.  */
                    if (length < allocated)
                      {
                        int ret =
                          U_UCTOMB (result + length, muc, allocated - length);
                        if (ret == -1)
                          {
                            errno = EINVAL;
                            goto fail;
                          }
                        if (ret >= 0)
                          {
                            length += ret;
                            goto done_appending;
                          }
                      }
                    {
                      size_t old_allocated = allocated;
                      size_t new_allocated = 2 * old_allocated;
                      if (new_allocated < 64)
                        new_allocated = 64;
                      if (new_allocated < old_allocated) /* integer overflow? */
                        abort ();
                      {
                        UNIT *larger_result;
                        if (result == NULL)
                          {
                            larger_result =
                              (UNIT *) malloc (new_allocated * sizeof (UNIT));
                            if (larger_result == NULL)
                              {
                                errno = ENOMEM;
                                goto fail;
                              }
                          }
                        else if (result == resultbuf)
                          {
                            larger_result =
                              (UNIT *) malloc (new_allocated * sizeof (UNIT));
                            if (larger_result == NULL)
                              {
                                errno = ENOMEM;
                                goto fail;
                              }
                            U_CPY (larger_result, resultbuf, length);
                          }
                        else
                          {
                            larger_result =
                              (UNIT *) realloc (result, new_allocated * sizeof (UNIT));
                            if (larger_result == NULL)
                              {
                                errno = ENOMEM;
                                goto fail;
                              }
                          }
                        result = larger_result;
                        allocated = new_allocated;
                        {
                          int ret =
                            U_UCTOMB (result + length, muc, allocated - length);
                          if (ret == -1)
                            {
                              errno = EINVAL;
                              goto fail;
                            }
                          if (ret < 0)
                            abort ();
                          length += ret;
                          goto done_appending;
                        }
                      }
                    }
                   done_appending: ;
                  }

                /* sortbuf is now empty.  */
                sortbuf_count = 0;
              }

            if (!(s < s_end))
              /* End of string reached.  */
              break;

            /* Append (uc, ccc) to sortbuf.  */
            if (sortbuf_count == sortbuf_allocated)
              {
                struct ucs4_with_ccc *new_sortbuf;

                sortbuf_allocated = 2 * sortbuf_allocated;
                if (sortbuf_allocated < sortbuf_count) /* integer overflow? */
                  abort ();
                new_sortbuf =
                  (struct ucs4_with_ccc *) malloc (2 * sortbuf_allocated * sizeof (struct ucs4_with_ccc));
                if (new_sortbuf == NULL)
                  {
                    errno = ENOMEM;
                    goto fail;
                  }
                memcpy (new_sortbuf, sortbuf,
                        sortbuf_count * sizeof (struct ucs4_with_ccc));
                if (sortbuf != sortbuf_preallocated)
                  free (sortbuf);
                sortbuf = new_sortbuf;
              }
            sortbuf[sortbuf_count].code = uc;
            sortbuf[sortbuf_count].ccc = ccc;
            sortbuf_count++;

            i++;
          }

        if (!(s < s_end))
          /* End of string reached.  */
          break;

        s += count;
      }
  }

  if (length == 0)
    {
      if (result == NULL)
        {
          /* Return a non-NULL value.  NULL means error.  */
          result = (UNIT *) malloc (1);
          if (result == NULL)
            {
              errno = ENOMEM;
              goto fail;
            }
        }
    }
  else if (result != resultbuf && length < allocated)
    {
      /* Shrink the allocated memory if possible.  */
      UNIT *memory;

      memory = (UNIT *) realloc (result, length * sizeof (UNIT));
      if (memory != NULL)
        result = memory;
    }

  if (sortbuf_count > 0)
    abort ();
  if (sortbuf != sortbuf_preallocated)
    free (sortbuf);

  *lengthp = length;
  return result;

 fail:
  {
    int saved_errno = errno;
    if (sortbuf != sortbuf_preallocated)
      free (sortbuf);
    if (result != resultbuf)
      free (result);
    errno = saved_errno;
  }
  return NULL;
}

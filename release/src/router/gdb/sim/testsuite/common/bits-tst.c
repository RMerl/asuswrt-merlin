# 2 "bits-tst.c"

/* Drive the bit test routines */


long long
calc (const char *call,
      long long val,
      int row,
      int col)
{
  if (strcmp (call, "MASK") == 0)
    return MASKED (val, row, col);
  if (strcmp (call, "MASK8") == 0)
    return MASKED8 (val, row, col);
  if (strcmp (call, "MASK16") == 0)
    return MASKED16 (val, row, col);
  if (strcmp (call, "MASK32") == 0)
    return MASKED32 (val, row, col);
  if (strcmp (call, "MASK64") == 0)
    return MASKED64 (val, row, col);

  if (strcmp (call, "EXTRACT") == 0)
    return EXTRACTED (val, row, col);
  if (strcmp (call, "EXTRACT8") == 0)
    return EXTRACTED8 (val, row, col);
  if (strcmp (call, "EXTRACT16") == 0)
    return EXTRACTED16 (val, row, col);
  if (strcmp (call, "EXTRACT32") == 0)
    return EXTRACTED32 (val, row, col);
  if (strcmp (call, "EXTRACT64") == 0)
    return EXTRACTED64 (val, row, col);

  if (strcmp (call, "LSEXTRACT") == 0)
    return LSEXTRACTED (val, row, col);
  if (strcmp (call, "LSEXTRACT8") == 0)
    return LSEXTRACTED8 (val, row, col);
  if (strcmp (call, "LSEXTRACT16") == 0)
    return LSEXTRACTED16 (val, row, col);
  if (strcmp (call, "LSEXTRACT32") == 0)
    return LSEXTRACTED32 (val, row, col);
  if (strcmp (call, "LSEXTRACT64") == 0)
    return LSEXTRACTED64 (val, row, col);

  if (strcmp (call, "MSEXTRACT") == 0)
    return MSEXTRACTED (val, row, col);
  if (strcmp (call, "MSEXTRACT8") == 0)
    return MSEXTRACTED8 (val, row, col);
  if (strcmp (call, "MSEXTRACT16") == 0)
    return MSEXTRACTED16 (val, row, col);
  if (strcmp (call, "MSEXTRACT32") == 0)
    return MSEXTRACTED32 (val, row, col);
  if (strcmp (call, "MSEXTRACT64") == 0)
    return MSEXTRACTED64 (val, row, col);

  if (strcmp (call, "INSERT") == 0)
    return INSERTED (val, row, col);
  if (strcmp (call, "INSERT8") == 0)
    return INSERTED8 (val, row, col);
  if (strcmp (call, "INSERT16") == 0)
    return INSERTED16 (val, row, col);
  if (strcmp (call, "INSERT32") == 0)
    return INSERTED32 (val, row, col);
  if (strcmp (call, "INSERT64") == 0)
    return INSERTED64 (val, row, col);

  if (strcmp (call, "LSINSERT") == 0)
    return LSINSERTED (val, row, col);
  if (strcmp (call, "LSINSERT8") == 0)
    return LSINSERTED8 (val, row, col);
  if (strcmp (call, "LSINSERT16") == 0)
    return LSINSERTED16 (val, row, col);
  if (strcmp (call, "LSINSERT32") == 0)
    return LSINSERTED32 (val, row, col);
  if (strcmp (call, "LSINSERT64") == 0)
    return LSINSERTED64 (val, row, col);

  if (strcmp (call, "MSINSERT") == 0)
    return MSINSERTED (val, row, col);
  if (strcmp (call, "MSINSERT8") == 0)
    return MSINSERTED8 (val, row, col);
  if (strcmp (call, "MSINSERT16") == 0)
    return MSINSERTED16 (val, row, col);
  if (strcmp (call, "MSINSERT32") == 0)
    return MSINSERTED32 (val, row, col);
  if (strcmp (call, "MSINSERT64") == 0)
    return MSINSERTED64 (val, row, col);

  if (strcmp (call, "MSMASK") == 0)
    return MSMASKED (val, row, col);
  if (strcmp (call, "MSMASK8") == 0)
    return MSMASKED8 (val, row, col);
  if (strcmp (call, "MSMASK16") == 0)
    return MSMASKED16 (val, row, col);
  if (strcmp (call, "MSMASK32") == 0)
    return MSMASKED32 (val, row, col);
  if (strcmp (call, "MSMASK64") == 0)
    return MSMASKED64 (val, row, col);

  if (strcmp (call, "LSMASK") == 0)
    return LSMASKED (val, row, col);
  if (strcmp (call, "LSMASK8") == 0)
    return LSMASKED8 (val, row, col);
  if (strcmp (call, "LSMASK16") == 0)
    return LSMASKED16 (val, row, col);
  if (strcmp (call, "LSMASK32") == 0)
    return LSMASKED32 (val, row, col);
  if (strcmp (call, "LSMASK64") == 0)
    return LSMASKED64 (val, row, col);

  if (strcmp (call, "ROT64") == 0)
    return ROT64 (val, col);
  if (strcmp (call, "ROT8") == 0)
    return ROT8 (val, col);
  if (strcmp (call, "ROT16") == 0)
    return ROT16 (val, col);
  if (strcmp (call, "ROT32") == 0)
    return ROT32 (val, col);

  if (strcmp (call, "SEXT") == 0)
    return SEXT (val, col);
  if (strcmp (call, "SEXT8") == 0)
    return SEXT8 (val, col);
  if (strcmp (call, "SEXT16") == 0)
    return SEXT16 (val, col);
  if (strcmp (call, "SEXT32") == 0)
    return SEXT32 (val, col);
  if (strcmp (call, "SEXT64") == 0)
    return SEXT64 (val, col);

  if (strcmp (call, "LSSEXT") == 0)
    return LSSEXT (val, col);
  if (strcmp (call, "LSSEXT8") == 0)
    return LSSEXT8 (val, col);
  if (strcmp (call, "LSSEXT16") == 0)
    return LSSEXT16 (val, col);
  if (strcmp (call, "LSSEXT32") == 0)
    return LSSEXT32 (val, col);
  if (strcmp (call, "LSSEXT64") == 0)
    return LSSEXT64 (val, col);

  if (strcmp (call, "MSSEXT8") == 0)
    return MSSEXT8 (val, col);
  if (strcmp (call, "MSSEXT16") == 0)
    return MSSEXT16 (val, col);
  if (strcmp (call, "MSSEXT32") == 0)
    return MSSEXT32 (val, col);
  if (strcmp (call, "MSSEXT64") == 0)
    return MSSEXT64 (val, col);
  if (strcmp (call, "MSSEXT") == 0)
    return MSSEXT (val, col);

  else
    {
      fprintf (stderr,
	       "Unknown call passed to calc (%s, 0x%08lx%08lx, %d, %d)\n",
	       call, (long)(val >> 32), (long)val, row, col);
      abort ();
      return val;
    }
}


int
check_sext (int nr_bits,
	    int msb_nr,
	    const char *sexted,
	    const char *masked,
	    const char *msmasked)
{
  int errors = 0;
  int col;
  for (col = 0; col < nr_bits; col ++)
    {
      long long mask = calc (masked, -1, col, col);
      long long msmask = calc (msmasked, -1,
			       0, (msb_nr ? nr_bits - col - 1 : col));
      long long sext = calc (sexted, mask, -1, col);
      long long mask_1 = mask >> 1;
      long long sext_1 = calc (sexted, mask_1, -1, col);
      long long mask_0 = (mask << 1) | mask_1;
      long long sext_0 = calc (sexted, mask_0, -1, col);
      if (sext_0 != mask_1)
	{
	  fprintf (stderr,
		   "%s:%d: ", __FILE__, __LINE__);
	  fprintf (stderr,
		   " %s(0x%08lx%08lx,%d) == 0x%08lx%08lx wrong, != 0x%08lx%08lx\n",
		   sexted, (long)(mask_0 >> 32), (long)mask_0, col,
		   (long)(sext_0 >> 32), (long)sext_0,
		   (long)(mask_1 >> 32), (long)mask_1);
	  errors ++;
	}
      if (sext_1 != mask_1)
	{
	  fprintf (stderr,
		   "%s:%d: ", __FILE__, __LINE__);
	  fprintf (stderr,
		   " %s(0x%08lx%08lx,%d) == 0x%08lx%08lx wrong, != 0x%08lx%08lx\n",
		   sexted, (long)(mask_1 >> 32), (long)mask_1, col,
		   (long)(sext_1 >> 32), (long)sext_1,
		   (long)(mask_1 >> 32), (long)mask_1);
	  errors ++;
	}
      if (sext != msmask)
	{
	  fprintf (stderr,
		   "%s:%d: ", __FILE__, __LINE__);
	  fprintf (stderr,
		   " %s(0x%08lx%08lx,%d) == 0x%08lx%08lx wrong, != 0x%08lx%08lx (%s(%d,%d))\n",
		   sexted, (long)(mask >> 32), (long)mask, col,
		   (long)(sext >> 32), (long)sext,
		   (long)(msmask >> 32), (long)msmask,
		   msmasked, 0, (msb_nr ? nr_bits - col - 1 : col));
	  errors ++;
	}

    }
  return errors;
}


int
check_rot (int nr_bits,
	   const char *roted,
	   const char *masked)
{
  int errors = 0;
  int row;
  int col;
  for (row = 0; row < nr_bits; row++)
    for (col = 0; col < nr_bits; col++)
      if ((WITH_TARGET_WORD_MSB == 0 && row <= col)
	  || (WITH_TARGET_WORD_MSB != 0 && row >= col))
	{
	  long long mask = calc (masked, -1, row, col);
	  int shift;
	  for (shift = -nr_bits + 1; shift < nr_bits; shift ++)
	    {
	      long long rot = calc (roted, mask, -1, shift);
	      long long urot = calc (roted, rot, -1, -shift);
	      if (mask != urot
		  || (shift == 0 && rot != mask)
		  || (shift != 0 && rot == mask && abs(row - col) != (nr_bits - 1)))
		{
		  fprintf (stderr, "%s:%d: ", __FILE__, __LINE__);
		  fprintf (stderr, " %s(%s(0x%08lx%08lx,%d) == 0x%08lx%08lx, %d) failed\n",
			   roted, roted,
			   (long)(mask >> 32), (long)mask, shift,
			   (long)(urot >> 32), (long)urot, -shift);
		  errors ++;
		}
	    }
	}
  return errors;
}


int
check_extract (int nr_bits,
	       const char *extracted,
	       const char *inserted,
	       const char *masked)
{
  int errors = 0;
  int row;
  int col;
  for (row = 0; row < nr_bits; row++)
    for (col = 0; col < nr_bits; col ++)
      if ((WITH_TARGET_WORD_MSB == 0 && row <= col)
	  || (WITH_TARGET_WORD_MSB != 0 && row >= col))
	{
	  long long mask = calc (masked, -1, row, col);
	  long long extr = calc (extracted, mask, row, col);
	  long long inst = calc (inserted, extr, row, col);
	  if (mask != inst)
	    {
	      fprintf (stderr, "%s:%d: ", __FILE__, __LINE__);
	      fprintf (stderr, " %s(%d,%d)=0x%08lx%08lx -> %s=0x%08lx%08lx -> %s=0x%08lx%08lx failed\n",
		       masked, row, col, (long)(mask >> 32), (long)mask,
		       extracted, (long)(extr >> 32), (long)extr,
		       inserted, (long)(inst >> 32), (long)inst);
	      errors ++;
	    }
	}
  return errors;
}


int
check_bits (int call,
	    test_spec **tests)
{
  int r;
  int c;
  int errors = 0;
  while (*tests != NULL)
    {
      int nr_rows = (*tests)->nr_rows;
      int nr_cols = (*tests)->nr_cols;
      test_tuples *tuples = (*tests)->tuples;
      for (r = 0; r < nr_rows; r++)
	for (c = 0; c < nr_cols; c++)
	  {
	    int i = r * nr_rows + c;
	    test_tuples *tuple = &tuples[i];
	    if (tuple->col >= 0)
	      {
		long long val = (!call ? tuple->val : calc ((*tests)->macro, -1,
							    tuple->row, tuple->col));
		long long check = tuple->check;
		if (val != check)
		  {
		    fprintf (stderr, "%s:%d:", (*tests)->file, tuple->line);
		    fprintf (stderr, " %s", (*tests)->macro);
		    if (tuple->row >= 0)
		      fprintf (stderr, " (%d, %d)", tuple->row, tuple->col);
		    else
		      fprintf (stderr, " (%d)", tuple->col);
		    fprintf (stderr, " == 0x%08lx%08lx wrong, != 0x%08lx%08lx\n",
			     (long) (val >> 32), (long) val,
			     (long) (check >> 32), (long) check);
		    errors ++;
		  }
	      }
	  }
      tests ++;
    }
  return errors;
}     


int
main (argc, argv)
     int argc;
     char **argv;
{
  int errors = 0;


#if defined (DO_BIT_TESTS)
  printf ("Checking BIT*\n");
  errors += check_bits (0, bit_tests);
#endif


#if defined (DO_MASK_TESTS)
  printf ("Checking MASK*\n");
  errors += check_bits (0, mask_tests);

  printf ("Checking MASKED*\n");
  errors += check_bits (1, mask_tests);
#endif


#if defined (DO_LSMASK_TESTS)
  printf ("Checking LSMASK*\n");
  errors += check_bits (0, lsmask_tests);

  printf ("Checking LSMASKED*\n");
  errors += check_bits (1, lsmask_tests);
#endif


#if defined (DO_MSMASK_TESTS)
  printf ("Checking MSMASK*\n");
  errors += check_bits (0, msmask_tests);

  printf ("Checking MSMASKED*\n");
  errors += check_bits (1, msmask_tests);
#endif


  printf ("Checking EXTRACTED*\n");
  errors += check_extract ( 8, "EXTRACT8",  "INSERT8",  "MASK8");
  errors += check_extract (16, "EXTRACT16", "INSERT16", "MASK16");
  errors += check_extract (32, "EXTRACT32", "INSERT32", "MASK32");
  errors += check_extract (64, "EXTRACT64", "INSERT64", "MASK64");
  errors += check_extract (64, "EXTRACT",   "INSERT",   "MASK");

  printf ("Checking SEXT*\n");
  errors += check_sext ( 8, WITH_TARGET_WORD_MSB, "SEXT8",  "MASK8",  "MSMASK8");
  errors += check_sext (16, WITH_TARGET_WORD_MSB, "SEXT16", "MASK16", "MSMASK16");
  errors += check_sext (32, WITH_TARGET_WORD_MSB, "SEXT32", "MASK32", "MSMASK32");
  errors += check_sext (64, WITH_TARGET_WORD_MSB, "SEXT64", "MASK64", "MSMASK64");
  errors += check_sext (64, WITH_TARGET_WORD_MSB, "SEXT",   "MASK",   "MSMASK");
  
  printf ("Checking LSSEXT*\n");
  errors += check_sext ( 8,  8 - 1, "LSSEXT8",  "LSMASK8",  "MSMASK8");
  errors += check_sext (16, 16 - 1, "LSSEXT16", "LSMASK16", "MSMASK16");
  errors += check_sext (32, 32 - 1, "LSSEXT32", "LSMASK32", "MSMASK32");
  errors += check_sext (64, 64 - 1, "LSSEXT64", "LSMASK64", "MSMASK64");
  errors += check_sext (64, WITH_TARGET_WORD_BITSIZE - 1, "LSSEXT",   "LSMASK",   "MSMASK");
  
  printf ("Checking MSSEXT*\n");
  errors += check_sext (8,   0, "MSSEXT8",  "MSMASK8",  "MSMASK8");
  errors += check_sext (16,  0, "MSSEXT16", "MSMASK16", "MSMASK16");
  errors += check_sext (32,  0, "MSSEXT32", "MSMASK32", "MSMASK32");
  errors += check_sext (64,  0, "MSSEXT64", "MSMASK64", "MSMASK64");
  errors += check_sext (64,  0, "MSSEXT",   "MSMASK",   "MSMASK");
  
  printf ("Checking ROT*\n");
  errors += check_rot (16, "ROT16", "MASK16");
  errors += check_rot (32, "ROT32", "MASK32");
  errors += check_rot (64, "ROT64", "MASK64");

  return errors != 0;
}

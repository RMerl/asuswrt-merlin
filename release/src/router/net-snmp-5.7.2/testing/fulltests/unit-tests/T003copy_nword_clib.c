/* HEADER Testing copy_nword */

#define ASSERT1(cond)                                           \
  do {                                                          \
    ++__test_counter;                                           \
    if ((cond))                                                 \
      printf("ok %d\n", __test_counter);                        \
    else {                                                      \
      printf("not ok %d - %d: %s failed\n",                     \
             __test_counter, __LINE__, #cond);                  \
    }                                                           \
  } while (0)

#define ASSERT2(cond, on_error)                                 \
  do {                                                          \
    ++__test_counter;                                           \
    if ((cond))                                                 \
      printf("ok %d\n", __test_counter);                        \
    else {                                                      \
      printf("not ok %d - %d: %s failed, ",                     \
             __test_counter, __LINE__, #cond);                  \
      printf on_error ;                                         \
    }                                                           \
  } while (0)

{
  /* A quoted string */
  char input[] = "\"The red rose\"";
  char output[sizeof(input)] = "";
  char* run = copy_nword(input, output, sizeof(output));
  ASSERT2(strcmp(output, "The red rose") == 0,
          ("output = >%s<\n", output));
  ASSERT1(run == NULL);
}

{
  /* Escaped quotes */
  char input[] = "\\\"The red rose\\\"";
  char output[sizeof(input)] = "";
  char* run = copy_nword(input, output, sizeof(output));
  ASSERT2(strcmp(output, "\"The") == 0, ("output = >%s<\n", output));
  ASSERT2(run == input + 6,
          ("run = input + %" NETSNMP_PRIz "d\n", run - input));
  run = copy_nword(run, output, sizeof(output));
  ASSERT2(strcmp(output, "red") == 0, ("output = >%s<\n", output));
  ASSERT2(run == input + 10,
          ("run = input + %" NETSNMP_PRIz "d\n", run - input));
  run = copy_nword(run, output, sizeof(output));
  ASSERT2(strcmp(output, "rose\"") == 0, ("output = >%s<\n", output));
  ASSERT1(run == NULL);
}

{
  /* Unterminated "-quote */
  char input[] = "\"The";
  char output[sizeof(input)] = "";
  char* run = copy_nword(input, output, sizeof(output));
  ASSERT2(strcmp(output, "The") == 0, ("output = >%s<\n", output));
  ASSERT1(run == NULL);
}

{
  /* Unterminated '-quote */
  char input[] = "\'The";
  char output[sizeof(input)] = "";
  char* run = copy_nword(input, output, sizeof(output));
  ASSERT2(strcmp(output, "The") == 0, ("output = >%s<\n", output));
  ASSERT1(run == NULL);
}

{
  /* Extract from NULL */
  char output[10] = "";
  char* run = NULL;
  run = copy_nword(run, output, sizeof(output));
  ASSERT1(run == NULL);
}

{
  /* Extract to NULL */
  char input[] = "The red rose";
  char* output = NULL;
  char* run = copy_nword(input, output, sizeof(output));
  ASSERT1(run == NULL);
}

{
  /* Long token */
  char input[] = "\"Very long token that overflows the buffer\" foo";
  char output[10] = "";
  char* run = copy_nword(input, output, sizeof(output));
  ASSERT2(strcmp(output, "Very long") == 0, ("output = >%s<\n", output));
  ASSERT2(run == input + 44,
          ("run = input + %" NETSNMP_PRIz "d\n", run - input));
}

{
  /* Quoted end of string / embedded \0 */
  char input[] = "The\\\0red rose";
  char output[sizeof(input)] = "";
  char* run = copy_nword(input, output, sizeof(output));
  ASSERT2(strcmp(output, "The\\") == 0, ("output = >%s<\n", output));
  ASSERT1(run == NULL);
}

{
  /* Empty string */
  char input[] = "";
  char output[sizeof(input) + 1] = "X";
  char* run = copy_nword(input, output, sizeof(output));
  ASSERT2(strcmp(output, "") == 0, ("output = >%s<\n", output));
  ASSERT2(run == NULL, ("run = >%s<\n", run));
}

{
  /* Whitespace string */
  char input[] = "    \t   ";
  char output[sizeof(input)] = "X";
  char* run = copy_nword(input, output, sizeof(output));
  ASSERT2(strcmp(output, "") == 0, ("output = >%s<\n", output));
  ASSERT2(run == NULL, ("run = >%s<\n", run));
}

{
  /* Quote, no whitespace after */
  char input[] = "\"The\"red rose";
  char output[sizeof(input)] = "";
  char* run = copy_nword(input, output, sizeof(output));
  ASSERT2(strcmp(output, "The") == 0, ("output = >%s<\n", output));
  ASSERT2(run == input + 5,
          ("run = input + %" NETSNMP_PRIz "d\n", run - input));
}

{
  /* Quote, no whitespace before */
  char input[] = "The\"red\" rose";
  char output[sizeof(input)] = "";
  char* run = copy_nword(input, output, sizeof(output));
  ASSERT2(strcmp(output, "The\"red\"") == 0, ("output = >%s<\n", output));
  ASSERT2(run == input + 9,
          ("run = input + %" NETSNMP_PRIz "d\n", run - input));
}

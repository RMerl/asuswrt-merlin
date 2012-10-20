/* ============================================================================
 * Copyright (C) 1999 Angus Mackay. All rights reserved; 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * ============================================================================
 */

/*
 * conf_file.c
 *
 * simple config file code
 *
 */


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_ERRNO_H
#  include <errno.h>
#endif

#include <conf_file.h>
#include <dprintf.h>

#if HAVE_STRERROR
//extern int errno;
#  define error_string strerror(errno)
#elif HAVE_SYS_ERRLIST
extern const char *const sys_errlist[];
extern int errno;
#  define error_string (sys_errlist[errno])
#else
#  define error_string "error message not found"
#endif

int parse_conf_file(char *fname, struct conf_cmd *commands)
{
  char buf[BUFSIZ+1];
  FILE *in;
  int using_a_file = 1;
  char *p;
  char *cmd_start;
  char *arg;
  struct conf_cmd *cmd;
  int lnum = 0;

  // safety first
  buf[BUFSIZ] = '\0';

  if(strcmp("-", fname) == 0)
  {
    in = stdin;
    using_a_file = 0;
  }
  else
  {
    if((in=fopen(fname, "r")) == NULL)
    {
      show_message("could not open config file \"%s\": %s\n", fname, error_string);
      return(-1);
    }
  }

  while(lnum++, fgets(buf, BUFSIZ, in) != NULL)
  {
    p = buf;

    /* eat space */
    while(*p == ' ' || *p == '\t') { p++; }
      
    /* ignore comments and blanks */
    if(*p == '#' || *p == '\r' || *p == '\n' || *p == '\0')
    {
      continue;
    }

    cmd_start = p;

    /* chomp new line */
    while(*p != '\0' && *p != '\r' && *p != '\n') { p++; }
    *p = '\0';
    p = cmd_start;

    /* find the end of the command */
    while(*p != '\0' && *p != '=') { p++; }

    /* insure that it is terminated and find arg */
    if(*p == '\0')
    {
      arg = NULL;
    }
    else
    {
      *p = '\0';
      p++;
      arg = p;
    }

    /* look up the command */
    cmd = commands;
    while(cmd->name != NULL)
    {
      if(strcmp(cmd->name, cmd_start) == 0)
      {
        dprintf((stderr, "using cmd %s\n", cmd->name));
        break;
      }
      cmd++;
    }
    if(cmd->name == NULL)
    {
      show_message("%s,%d: unknown command: %s\n", fname, lnum, cmd_start);

      fprintf(stderr, "commands are:\n");
      cmd = commands;
      while(cmd->name != NULL)
      {
        fprintf(stderr, "  %-14s usage: ", cmd->name);
        fprintf(stderr, cmd->help, cmd->name);
        fprintf(stderr, "\n");
        cmd++;
      }
      goto ERR;
    }

    /* check the arg */
    switch(cmd->arg_type)
    {
      case CONF_NEED_ARG:
        if(arg == NULL)
        {
          show_message("option \"%s\" requires an argument\n", cmd->name);
          goto ERR;
        }
        break;
      case CONF_OPT_ARG:
        if(arg == NULL)
        {
          arg = "";
        }
        break;
      case CONF_NO_ARG:
        arg = "";
        break;
      default:
        dprintf((stderr, "case not handled: %d\n", cmd->arg_type));
        break;
    }
    
    /* is the command implemented? */
    if(!cmd->available)
    {
      show_message("the command \"%s\" is not available\n", cmd->name);
      continue;
    }

    /* handle the command */
    cmd->proc(cmd, arg);
  }

  if(using_a_file)
  {
    if(in)
    {
      fclose(in);
    }
  }
  return 0;

ERR:
  if(using_a_file)
  {
    if(in)
    {
      fclose(in);
    }
  }
  return(-1);
}

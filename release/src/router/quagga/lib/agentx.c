/* SNMP support
 * Copyright (C) 2012 Vincent Bernat <bernat@luffy.cx>
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#include <zebra.h>

#if defined HAVE_SNMP && defined SNMP_AGENTX
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include "command.h"
#include "smux.h"

int agentx_enabled = 0;

/* AgentX node. */
static struct cmd_node agentx_node =
{
  SMUX_NODE,
  ""                            /* AgentX has no interface. */
};

/* Logging NetSNMP messages */
static int
agentx_log_callback(int major, int minor,
		    void *serverarg, void *clientarg)
{
  struct snmp_log_message *slm = (struct snmp_log_message *)serverarg;
  char *msg = strdup (slm->msg);
  if (msg) msg[strlen(msg)-1] = '\0';
  switch (slm->priority)
    {
    case LOG_EMERG:   zlog_err   ("snmp[emerg]: %s",   msg?msg:slm->msg); break;
    case LOG_ALERT:   zlog_err   ("snmp[alert]: %s",   msg?msg:slm->msg); break;
    case LOG_CRIT:    zlog_err   ("snmp[crit]: %s",    msg?msg:slm->msg); break;
    case LOG_ERR:     zlog_err   ("snmp[err]: %s",     msg?msg:slm->msg); break;
    case LOG_WARNING: zlog_warn  ("snmp[warning]: %s", msg?msg:slm->msg); break;
    case LOG_NOTICE:  zlog_notice("snmp[notice]: %s",  msg?msg:slm->msg); break;
    case LOG_INFO:    zlog_info  ("snmp[info]: %s",    msg?msg:slm->msg); break;
    case LOG_DEBUG:   zlog_debug ("snmp[debug]: %s",   msg?msg:slm->msg); break;
    }
  free(msg);
  return SNMP_ERR_NOERROR;
}

static int
config_write_agentx (struct vty *vty)
{
  if (agentx_enabled)
      vty_out (vty, "agentx%s", VTY_NEWLINE);
  return 0;
}

DEFUN (agentx_enable,
       agentx_enable_cmd,
       "agentx",
       "SNMP AgentX protocol settings\n"
       "SNMP AgentX settings\n")
{
  if (!agentx_enabled)
    {
      init_snmp("quagga");
      agentx_enabled = 1;
      return CMD_SUCCESS;
    }
  vty_out (vty, "SNMP AgentX already enabled%s", VTY_NEWLINE);
  return CMD_WARNING;
}

DEFUN (no_agentx,
       no_agentx_cmd,
       "no agentx",
       NO_STR
       "SNMP AgentX protocol settings\n"
       "SNMP AgentX settings\n")
{
  if (!agentx_enabled) return CMD_SUCCESS;
  vty_out (vty, "SNMP AgentX support cannot be disabled once enabled%s", VTY_NEWLINE);
  return CMD_WARNING;
}

void
smux_init (struct thread_master *tm)
{
  netsnmp_enable_subagent ();
  snmp_disable_log ();
  snmp_enable_calllog ();
  snmp_register_callback (SNMP_CALLBACK_LIBRARY,
			  SNMP_CALLBACK_LOGGING,
			  agentx_log_callback,
			  NULL);
  init_agent ("quagga");

  install_node (&agentx_node, config_write_agentx);
  install_element (CONFIG_NODE, &agentx_enable_cmd);
  install_element (CONFIG_NODE, &no_agentx_cmd);
}

void
smux_register_mib (const char *descr, struct variable *var, 
		   size_t width, int num,
		   oid name[], size_t namelen)
{
  register_mib (descr, var, width, num, name, namelen);
}

int
smux_trap (struct variable *vp, size_t vp_len,
	   const oid *ename, size_t enamelen,
	   const oid *name, size_t namelen,
	   const oid *iname, size_t inamelen,
	   const struct trap_object *trapobj, size_t trapobjlen,
	   u_char sptrap)
{
  oid objid_snmptrap[] = { 1, 3, 6, 1, 6, 3, 1, 1, 4, 1, 0 };
  size_t objid_snmptrap_len = sizeof objid_snmptrap / sizeof (oid);
  oid notification_oid[MAX_OID_LEN];
  size_t notification_oid_len;
  unsigned int i;

  netsnmp_variable_list *notification_vars = NULL;
  if (!agentx_enabled) return 0;

  /* snmpTrapOID */
  oid_copy (notification_oid, ename, enamelen);
  notification_oid[enamelen] = sptrap;
  notification_oid_len = enamelen + 1;
  snmp_varlist_add_variable (&notification_vars,
			     objid_snmptrap, objid_snmptrap_len,
			     ASN_OBJECT_ID,
			     (u_char *) notification_oid,
			     notification_oid_len * sizeof(oid));

  /* Provided bindings */
  for (i = 0; i < trapobjlen; i++)
    {
      unsigned int j;
      oid oid[MAX_OID_LEN];
      size_t oid_len, onamelen;
      u_char *val;
      size_t val_len;
      WriteMethod *wm = NULL;
      struct variable cvp;

      /* Make OID. */
      if (trapobj[i].namelen > 0)
        {
	  /* Columnar object */
	  onamelen = trapobj[i].namelen;
	  oid_copy (oid, name, namelen);
	  oid_copy (oid + namelen, trapobj[i].name, onamelen);
	  oid_copy (oid + namelen + onamelen, iname, inamelen);
	  oid_len = namelen + onamelen + inamelen;
        }
      else
        {
	  /* Scalar object */
	  onamelen = trapobj[i].namelen * (-1);
	  oid_copy (oid, name, namelen);
	  oid_copy (oid + namelen, trapobj[i].name, onamelen);
	  oid[onamelen + namelen] = 0;
	  oid_len = namelen + onamelen + 1;
        }

      /* Locate the appropriate function and type in the MIB registry. */
      for (j = 0; j < vp_len; j++)
	{
	  if (oid_compare (trapobj[i].name, onamelen, vp[j].name, vp[j].namelen) != 0)
	    continue;
	  /* We found the appropriate variable in the MIB registry. */
	  oid_copy(cvp.name, name, namelen);
	  oid_copy(cvp.name + namelen, vp[j].name, vp[j].namelen);
	  cvp.namelen = namelen + vp[j].namelen;
	  cvp.type = vp[j].type;
	  cvp.magic = vp[j].magic;
	  cvp.acl = vp[j].acl;
	  cvp.findVar = vp[j].findVar;
	  /* Grab the result. */
	  val = cvp.findVar (&cvp, oid, &oid_len, 1, &val_len, &wm);
	  if (!val) break;
	  snmp_varlist_add_variable (&notification_vars,
				     oid, oid_len,
				     vp[j].type,
				     val,
				     val_len);
	  break;
	}
    }


  send_v2trap (notification_vars);
  snmp_free_varbind (notification_vars);
  return 1;
}

#endif /* HAVE_SNMP */

/* Copyright Cloud Linux Inc 2010-2011 All Rights Reserved
 *
 * Licensed under CLOUD LINUX LICENSE AGREEMENT
 * http://cloudlinux.com/docs/LICENSE.TXT
 *
 * db_governor application
 * author Igor Seletskiy <iseletsk@cloudlinux.com>
 * author Alexey Berezhok <alexey.berezhok@cloudlinux.com>
 *
 */

#include "dbgovernor_string_functions.h"
#include "governor_config.h"
#include "user_account.h"

static GHashTable *user_account_table;

int check_if_user_restricted(username_t username, GHashTable * accounts){
	struct user_account *ua = get_user_account (username);
	Account *ac = NULL;
	ac = g_hash_table_lookup (accounts, ua->account);
	return (ac->timeout>0);
}

User_stats *
add_user_stats (username_t username, GHashTable * accounts,
		GHashTable * users)
{
  struct user_account *ua = get_user_account (username);
  Account *ac = NULL;
  ac = g_hash_table_lookup (accounts, ua->account);
  if (!ac)
    {
      g_hash_table_insert (accounts, ua->account, ac =
			   init_account (ua->account));
    }
  User_stats *us = init_user_stats (ua->username, ac);
  g_hash_table_insert (users, (gpointer) ua->username, us);
  g_ptr_array_add (ac->users, us);
  return us;
}

void
init_user_account_table ()
{
  user_account_table = g_hash_table_new (g_str_hash, g_str_equal);
}

void
free_user_account_table ()
{
  g_hash_table_foreach (user_account_table, (GHFunc) free, NULL);
  g_hash_table_unref (user_account_table);
}

struct user_account *
get_user_account (username_t username)
{
  struct user_account *ua =
    g_hash_table_lookup (user_account_table, username);
  struct governor_config data_cfg;
  get_config_data( &data_cfg );

  if (!ua)
    {
      ua = malloc (sizeof (struct user_account));
      strlcpy (ua->username, username, USERNAMEMAXLEN);
      g_hash_table_insert (user_account_table, ua->username, ua);
      stats_limit_cfg cfg_buf;
      stats_limit_cfg *sl = config_get_account_limit (ua->username, &cfg_buf);
      if (sl->account_flag)
	{
	  char *ptr = strchr (username, data_cfg.separator);
	  /*  NULL - no underscore, 0 -- first char is underscore */
	  if (ptr == NULL || ptr == username)
	    {
	      strlcpy (ua->account, username, USERNAMEMAXLEN);
	    }
	  else
	    {
	      strlcpy (ua->account, username, ptr - username + 1);
	    }
	}
      else
	{
	  strlcpy (ua->account, username, USERNAMEMAXLEN);
	}
    }
  return ua;
}


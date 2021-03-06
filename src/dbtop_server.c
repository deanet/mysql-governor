/*
 * dbtop_server.c
 *
 *  Created on: Aug 13, 2012
 *      Author: alexey
 */

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "data.h"
#include "dbgovernor_string_functions.h"
#include "dlload.h"
#include "governor_config.h"
#include "log.h"
#include "stats.h"
#include "user_account.h"
#include "getsysinfo.h"
#include "log-decoder.h"
#include "wrappers.h"

#include "calc_stats.h"

#include "dbtop_server.h"

void accept_connections(int s);
void *run_writer(void *data);
void *run_dbctl_command( void *data );
void send_account(const char *key, Account * ac, FILE * out);

void *run_server(void *data) {
	int i, s, len;
	struct sockaddr_un saun;
	int ret;
	char buffer[_DBGOVERNOR_BUFFER_2048];
    struct governor_config data_cfg;
    
    get_config_data( &data_cfg );

	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		WRITE_LOG(NULL, 0, buffer, _DBGOVERNOR_BUFFER_2048,
				"Can't create socket(DBTOP)", data_cfg.log_mode);
		close_log();
		close_restrict_log();
		exit(EXIT_FAILURE);
	}

	saun.sun_family = AF_UNIX;
	strcpy(saun.sun_path, SOCK_ADDRESS);

	unlink(SOCK_ADDRESS);
	len = sizeof(saun.sun_family) + strlen(saun.sun_path);

	if (bind(s, (struct sockaddr *) &saun, len) < 0) {
		WRITE_LOG(NULL, 0, buffer, _DBGOVERNOR_BUFFER_2048,
				"Can't server bind(DBTOP)", data_cfg.log_mode);
		close_log();
		close_restrict_log();
		exit(EXIT_FAILURE);
	}

	if (listen(s, 3) < 0) {
		WRITE_LOG(NULL, 0, buffer, _DBGOVERNOR_BUFFER_2048,
				"Can't server listen(DBTOP)", data_cfg.log_mode);
		close_log();
		close_restrict_log();
		exit(EXIT_FAILURE);
	}
	/* Start daemon accept cycle */
	accept_connections(s);

	return NULL;
}

void *run_dbtop_command(void *data) {
	FILE *out;
	intptr_t ns = (intptr_t) data;
	out = fdopen((int) ns, "w+");
	if(!out) {
		//Try to open second time
		out = fdopen((int) ns, "w+");
		//If null, then cancel command
		if(!out) {
			close(ns);
			return NULL;
		}
	}
	int new_record = 1, get_response;
	fwrite_wrapper(&new_record, sizeof(int), 1, out);
	fread_wrapper(&get_response, sizeof(int), 1, out);
	g_hash_table_foreach((GHashTable *) get_accounts(), (GHFunc) send_account,
			out);
	new_record = 2;
	fwrite_wrapper(&new_record, sizeof(int), 1, out);
	fflush(out);
	fclose(out);
	close(ns);
	return NULL;
}

void accept_connections(int s) {
	int ns, result;
	struct sockaddr_un fsaun;
	int fromlen = sizeof((struct sockaddr *) &fsaun);
	pthread_t thread;
	int ret;
	FILE *in, *out;
	char buffer[_DBGOVERNOR_BUFFER_2048];
    struct governor_config data_cfg;
    
    get_config_data( &data_cfg );

	while (1) {

		if ((ns = accept(s, (struct sockaddr *) &fsaun, &fromlen)) < 0) {
			if (errno == EINTR) {
				continue;
			} else {
				WRITE_LOG(NULL, 0, buffer, _DBGOVERNOR_BUFFER_2048,
						"Can't server accept(DBTOP)", data_cfg.log_mode);
				close_log();
				close_restrict_log();
				exit(EXIT_FAILURE);
			}

		}
		intptr_t accept_socket = (intptr_t) ns;
		client_type_t ctt;
		result = read(ns, &ctt, sizeof(client_type_t));
		switch (result) {
		case 0:
		case -1:
			close(ns);
			continue;
			break;
		}

		if (ctt == DBTOP) {
			ret = pthread_create(&thread, NULL, run_writer,
					(void *) accept_socket);
			pthread_detach(thread);
		} else if (ctt == DBCTL) {
			ret = pthread_create(&thread, NULL, run_dbctl_command,
					(void *) accept_socket);
			pthread_detach(thread);
		} else if (ctt == DBTOPCL) {
			ret = pthread_create(&thread, NULL, run_dbtop_command,
					(void *) accept_socket);
			pthread_detach(thread);
		} else {

			WRITE_LOG(NULL, 0, buffer, _DBGOVERNOR_BUFFER_2048,
					"incorrect connection(DBTOP)", data_cfg.log_mode);

			close(ns);
		}

	}
}

void *run_dbctl_command( void *data )
{
  int result;
  intptr_t ns = (intptr_t)data;

  DbCtlCommand command;
  result = read( ns, &command, sizeof( DbCtlCommand ) );

  struct governor_config data_cfg;
  get_config_data( &data_cfg );

  if( command.command == REREAD_CFG )
  {
    //config_free();
	//config_init( CONFIG_PATH );
    reread_config();
  }
  else if( command.command == RESTRICT )
  {
    if( !data_cfg.is_gpl ){
      lock_acc();
      g_hash_table_foreach( (GHashTable *)get_accounts(), (GHFunc)dbctl_restrict_set, &command );
      unlock_acc();
    }
  }
  else if( command.command == UNRESTRICT )
  {
    if( !data_cfg.is_gpl ) {
      lock_acc();
      g_hash_table_foreach( (GHashTable *)get_accounts(), (GHFunc)dbctl_unrestrict_set, &command );
      unlock_acc();
    }
  }
  else if( command.command == UNRESTRICT_A )
  {
    if( !data_cfg.is_gpl ) {
      lock_acc();
      g_hash_table_foreach( (GHashTable *)get_accounts(), (GHFunc)dbctl_unrestrict_all_set, NULL );
      unlock_acc();
    }
  }
  else if( command.command == LIST || command.command == LIST_R )
  {
    FILE *out;
    out = fdopen((int) ns, "w+");
    int new_record = 1, get_response;
	
    while( !feof( out ) ) 
    {
      fwrite_wrapper( &new_record, sizeof(int), 1, out );
      if( !fread_wrapper( &get_response, sizeof(int), 1, out ) )  break;

      g_hash_table_foreach( (GHashTable *)get_accounts(), (GHFunc) send_account, out );
      new_record = 2;
      if( !fwrite_wrapper( &new_record, sizeof(int), 1, out ) )  break;
    
      fflush( out );
      sleep( 1 );
      new_record = 1;
    }
    fclose( out );
  }

  close( ns );

  return NULL;
}

void *run_writer(void *data) {
	FILE *out;
	intptr_t ns = (intptr_t) data;
	out = fdopen((int) ns, "w+");
	if(!out){
		out = fdopen((int) ns, "w+");
		if(!out) {
			close(ns);
			return;
		}
	}
	int new_record = 1, get_response;
	while (!feof(out)) {
		fwrite_wrapper(&new_record, sizeof(int), 1, out);
		if (!fread_wrapper(&get_response, sizeof(int), 1, out))
			break;
		g_hash_table_foreach((GHashTable *)get_accounts(), (GHFunc) send_account, out);
		new_record = 2;
		if (!fwrite_wrapper(&new_record, sizeof(int), 1, out))
			break;
		fflush(out);
		sleep(1);
		new_record = 1;
	}
	fclose(out);
	close(ns);
	return NULL;
}

void send_account(const char *key, Account * ac, FILE * out) {
	int new_record = 0;
	stats_limit_cfg cfg_buf;
	stats_limit_cfg *sl = config_get_account_limit(ac->id, &cfg_buf);
	if (sl->mode != IGNORE_MODE) {
		if (!fwrite_wrapper(&new_record, sizeof(int), 1, out))
			return;
		dbtop_exch dt;
		lock_acc();		
		strncpy(dt.id, ac->id, sizeof(username_t));
		memcpy(&dt.current, &ac->current, sizeof(Stats));
		memcpy(&dt.short_average, &ac->short_average, sizeof(Stats));
		memcpy(&dt.mid_average, &ac->mid_average, sizeof(Stats));
		memcpy(&dt.long_average, &ac->long_average, sizeof(Stats));
		memcpy(&dt.restricted, &ac->restricted, sizeof(int));
		memcpy(&dt.timeout, &ac->timeout, sizeof(int));
		memcpy(&dt.info, &ac->info, sizeof(restrict_info));
		memcpy(&dt.start_count, &ac->start_count, sizeof(time_t));
		unlock_acc();
		if (!fwrite_wrapper(&dt, sizeof(dbtop_exch), 1, out))
			return;
	}
}

/*
 * libcsync -- a library to sync a replica with another
 *
 * Copyright (c) 2006-2007 by Andreas Schneider <mail@cynapses.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>

#include <csync.h>

#include <c_string.h>
#include <c_alloc.h>

#include "csync_version.h"
#include "csync_auth.h"
#include "../src/std/c_private.h"
#include "../src/csync_misc.h"

const char *csync_program_version = "csync commandline client "
  CSYNC_STRINGIFY(LIBCSYNC_VERSION);

/* Program documentation. */
static char doc[] = "Usage: csync [OPTION...] LOCAL REMOTE\n\
csync -- a user level file synchronizer which synchronizes the files\n\
at LOCAL with the ones at REMOTE.\n\
\n\
-c, --conflict-copies      Create conflict copys if file changed on both\n\
                           sides.\n\
-d, --debug-level=DEBUGLVL Set debug level\n\
    --disable-statedb      Disable the usage and creation of a statedb.\n\
    --dry-run              This runs only update detection and reconcilation.\n\
\n\
    --exclude-file=<file>  Add an additional exclude file\n"
#ifdef WITH_ICONV
"    --iconv=codec          Request charset conversion of local filenames\n"
#endif
"-p  --proxy=<host:port>    Use an http proxy (ownCloud module only)\n\
    --test-statedb         Test creation of the statedb. Runs update\n\
                           detection.\n\
    --test-update          Test the update detection\n\
-?, --help                 Give this help list\n\
    --usage                Give a short usage message\n\
-V, --version              Print program version\n\
";

/* The options we understand. */
static const struct option long_options[] =
{
    {"exclude-file",    required_argument, 0,  0  },
    {"debug-level",     required_argument, 0, 'd' },
    {"disable-statedb", no_argument,       0,  0  },
#ifdef WITH_ICONV
    {"iconv",           required_argument, 0,  0  },
#endif
    {"dry-run",         no_argument,       0,  0  },
    {"test-statedb",    no_argument,       0,  0  },
    {"conflict-copies", no_argument,       0, 'c' },
    {"test-update",     no_argument,       0,  0  },
    {"version",         no_argument,       0, 'V' },
    {"usage",           no_argument,       0, 'h' },
    {"proxy",           required_argument, 0, 'p' },
    {0, 0, 0, 0}
};

/* Used by main to communicate with parse_opt. */
struct argument_s {
  char *args[2]; /* SOURCE and DESTINATION */
  char *exclude_file;
  int debug_level;
  char *iconv;
  int disable_statedb;
  int create_statedb;
  int update;
  int reconcile;
  int propagate;
  bool with_conflict_copys;
  char *http_proxy;
};

static void print_version()
{
    printf( "%s\n", csync_program_version );
    exit(0);
}

static void print_help()
{
    printf( "%s\n", doc );
    exit(0);
}

static int parse_args(struct argument_s *csync_args, int argc, char **argv)
{
    while(optind < argc) {
        int c = -1;
        struct option *opt = NULL;
        int result = getopt_long( argc, argv, "d:cVhp:", long_options, &c );

        if( result == -1 ) {
            break;
        }

        switch(result) {
        case 'd':
            if (optarg != NULL) {
              csync_args->debug_level = atoi(optarg);
            }
            break;
        case 'c':
            csync_args->with_conflict_copys = true;
            /* printf("Argument: With conflict copies\n"); */
            break;
        case 'V':
            print_version();
            break;
        case 'h':
            print_help();
            break;
        case 'p':
            if (optarg != NULL) {
                csync_args->http_proxy = c_strdup(optarg);
            }
            break;
        case 0:
            opt = (struct option*)&(long_options[c]);
            if(c_streq(opt->name, "exclude-file")) {
                csync_args->exclude_file = c_strdup(optarg);
                /* printf("Argument: exclude-file: %s\n", csync_args->exclude_file); */
            } else if(c_streq(opt->name, "disable-statedb")) {
                csync_args->disable_statedb = 1;
            } else if(c_streq(opt->name, "test-update")) {
                csync_args->create_statedb = 0;
                csync_args->update = 1;
                csync_args->reconcile = 0;
                csync_args->propagate = 0;
                /* printf("Argument: test-update\n"); */

            } else if(c_streq(opt->name, "dry-run")) {
                csync_args->create_statedb = 0;
                csync_args->update = 1;
                csync_args->reconcile = 1;
                csync_args->propagate = 0;
                /* printf("Argument: dry-run\n" ); */
            } else if(c_streq(opt->name, "iconv")) {
                csync_args->iconv = c_strdup(optarg);
                /* printf("Argument: iconv\n" ); */
            } else if(c_streq(opt->name, "http-proxy")) {
                csync_args->http_proxy = c_strdup(optarg);
            } else if(c_streq(opt->name, "test-statedb")) {
                csync_args->create_statedb = 1;
                csync_args->update = 1;
                csync_args->reconcile = 0;
                csync_args->propagate = 0;
                /* printf("Argument: test-statedb\n"); */
            } else {
                fprintf(stderr, "Argument: No idea what!\n");
            }
            break;
        default:
            break;
        }
    }
    return optind;
}


int main(int argc, char **argv) {
  int rc = 0;
  CSYNC *csync;
  char errbuf[256] = {0};
  int curser = 0;
  int i;

  struct argument_s arguments;

  /* Default values. */
  arguments.exclude_file = NULL;
  arguments.debug_level = 4;
  arguments.iconv = NULL;
  arguments.disable_statedb = 0;
  arguments.create_statedb = 0;
  arguments.update = 1;
  arguments.reconcile = 1;
  arguments.propagate = 1;
  arguments.with_conflict_copys = false;
  arguments.http_proxy = NULL;

  parse_args(&arguments, argc, argv);
  /* two options must remain as source and target       */
  /* printf("ARGC: %d -> optind: %d\n", argc, optind ); */
  if( argc - optind < 2 ) {
      print_help();
  }

  if (csync_create(&csync, argv[optind], argv[optind+1]) < 0) {
    fprintf(stderr, "csync_create: failed\n");
    exit(1);
  }

  /*
   * Protect password from ps listing
   * Find and replace :password@ by :********@
   */
  for (i = 0; i < argc; i++) {
    if (csync_fnmatch("*://*:*@*", argv[i], 0) == 0) {
      curser = strlen(argv[i]);
      while(curser > 0 && argv[i][curser] != '@') {
        curser--;
      }
      while(curser > 0 && argv[i][curser - 1] != ':') {
        curser--;
        argv[i][curser] = '*';
      }
    }
  }

  csync_set_auth_callback(csync, csync_getpass);

  if (arguments.debug_level) {
    csync_set_log_verbosity(csync, arguments.debug_level);
  }

  if (arguments.disable_statedb) {
    csync_disable_statedb(csync);
  }

#ifdef WITH_ICONV
  if (arguments.iconv) {
    csync_set_iconv_codec(arguments.iconv);
  }
#endif

  if(arguments.with_conflict_copys)
  {
    csync_enable_conflictcopys(csync);
  }

  if (csync_init(csync) < 0) {
    perror("csync_init");
    rc = 1;
    goto out;
  }

  if (arguments.http_proxy) {
    char *host = NULL;
    int port = 0;
    char *colon;

    host = arguments.http_proxy;
    colon = strrchr( arguments.http_proxy, ':');
    if( colon ) {
        port = atoi(colon+1);
        *colon = '\0';
    }
    csync_set_module_property(csync, "proxy_type", (void*) "HttpProxy");
    csync_set_module_property(csync, "proxy_host", host);
    if( port )
        csync_set_module_property(csync, "proxy_port", (void*) &port);
  }

  if (arguments.exclude_file != NULL) {
    if (csync_add_exclude_list(csync, arguments.exclude_file) < 0) {
      strerror_r(errno, errbuf, sizeof(errbuf));
      fprintf(stderr, "csync_add_exclude_list - %s: %s\n",
          arguments.exclude_file, errbuf);
      rc = 1;
      goto out;
    }
  }

  if (arguments.update) {
    if (csync_update(csync) < 0) {
      perror("csync_update");
      rc = 1;
      goto out;
    }
  }

  if (arguments.reconcile) {
    if (csync_reconcile(csync) < 0) {
      perror("csync_reconcile");
      rc = 1;
      goto out;
    }
  }

  if (arguments.propagate) {
    if (csync_propagate(csync) < 0) {
      perror("csync_propagate");
      rc = 1;
      goto out;
    }
  }

  if (arguments.create_statedb) {
    csync_set_status(csync, 0xFFFF);
  }

out:
  csync_destroy(csync);

  return rc;
}

/* vim: set ts=8 sw=2 et cindent: */

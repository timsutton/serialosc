/**
 * Copyright (c) 2010 William Light <wrl@illest.net>
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* for vasprintf */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

#include <confuse.h>
#include <monome.h>

#include "serialosc.h"


#define DEFAULT_SERVER_PORT  0
#define DEFAULT_OSC_PREFIX   "/monome"
#define DEFAULT_APP_PORT     8000
#define DEFAULT_APP_HOST     "127.0.0.1"
#define DEFAULT_ROTATION     MONOME_ROTATE_0


static cfg_opt_t server_opts[] = {
	CFG_INT("port",       DEFAULT_SERVER_PORT, CFGF_NONE),
	CFG_END()
};

static cfg_opt_t app_opts[] = {
	CFG_STR("osc_prefix", DEFAULT_OSC_PREFIX,  CFGF_NONE),
	CFG_STR("host",       DEFAULT_APP_HOST,    CFGF_NONE),
	CFG_INT("port",       DEFAULT_APP_PORT,    CFGF_NONE),
	CFG_END()
};

static cfg_opt_t dev_opts[] = {
	CFG_INT("rotation",   DEFAULT_ROTATION,    CFGF_NONE),
	CFG_END()
};

static cfg_opt_t opts[] = {
	CFG_SEC("server", server_opts, CFGF_NONE),
	CFG_SEC("application", app_opts, CFGF_NONE),
	CFG_SEC("device", dev_opts, CFGF_NONE),
	CFG_END()
};


static void prepend_slash_if_necessary(char **dest, char *prefix) {
	if( *prefix != '/' )
		asprintf(dest, "/%s", prefix);
	else
		*dest = strdup(prefix);
}

static char *config_directory() {
	char *dir;
#ifdef DARWIN
	asprintf(&dir, "%s/Library/Preferences/org.monome.serialosc", getenv("HOME"));
#else
	if( getenv("XDG_CONFIG_HOME") )
		asprintf(&dir, "%s/serialosc", getenv("XDG_CONFIG_HOME"));
	else
		asprintf(&dir, "%s/.config/serialosc", getenv("HOME"));
#endif

	return dir;
}

static char *path_for_serial(const char *serial) {
	char *path, *cdir;

	cdir = config_directory();

	asprintf(&path, "%s/%s.conf", cdir, serial);

	free(cdir);
	return path;
}

int sosc_config_create_directory() {
	char *cdir = config_directory();
	struct stat buf[1];

	if( !stat(cdir, buf) )
		return 0; /* all is well */

#ifndef DARWIN
	if( !getenv("XDG_CONFIG_HOME") ) {
		/* well, I guess somebody's got to do it */
		char *xdgdir;
		asprintf(&xdgdir, "%s/.config", getenv("HOME"));
		if( mkdir(xdgdir, S_IRWXU) )
			return 1;

		free(xdgdir);
	}
#endif

	if( mkdir(cdir, S_IRWXU) )
		return 1;

	return 0;
}

int sosc_config_read(const char *serial, sosc_config_t *config) {
	cfg_t *cfg, *sec;
	char *path;

	if( !serial )
		return 1;

	cfg = cfg_init(opts, CFGF_NOCASE);
	path = path_for_serial(serial);

	switch( cfg_parse(cfg, path) ) {
	case CFG_PARSE_ERROR:
		fprintf(stderr, "serialosc [%s]: parse error in saved configuration\n",
				serial);
		break;
	}

	free(path);

	sec = cfg_getsec(cfg, "server");
	sosc_port_itos(config->server.port, cfg_getint(sec, "port"));

	sec = cfg_getsec(cfg, "application");
	prepend_slash_if_necessary(&config->app.osc_prefix, cfg_getstr(sec, "osc_prefix"));
	config->app.host = strdup(cfg_getstr(sec, "host"));
	sosc_port_itos(config->app.port, cfg_getint(sec, "port"));

	sec = cfg_getsec(cfg, "device");
	config->dev.rotation = (cfg_getint(sec, "rotation") / 90) % 4;

	cfg_free(cfg);

	return 0;
}

int sosc_config_write(const char *serial, sosc_config_t *config) {
	cfg_t *cfg, *sec;
	char *path;
	FILE *f;

	if( !serial )
		return 1;

	cfg = cfg_init(opts, CFGF_NOCASE);

	path = path_for_serial(serial);
	if( !(f = fopen(path, "w")) ) {
		free(path);
		return 1;
	}

	free(path);

	sec = cfg_getsec(cfg, "server");
	cfg_setint(sec, "port", atoi(config->server.port));

	sec = cfg_getsec(cfg, "application");
	cfg_setstr(sec, "osc_prefix", config->app.osc_prefix);
	cfg_setstr(sec, "host", config->app.host);
	cfg_setint(sec, "port", atoi(config->app.port));

	sec = cfg_getsec(cfg, "device");
	cfg_setint(sec, "rotation", config->dev.rotation * 90);

	cfg_print(cfg, f);
	fclose(f);

	return 0;
}

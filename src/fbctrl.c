/*
 *  fbctrl.c -- functions to switch windows from a window manager.
 *
 *  Copyright (C) 2007 Maik Broemme <mbroemme@plusserver.de>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* generic includes. */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

/* xserver includes. */
#include <X11/Xlib.h>
#include <X11/Xatom.h>

/* fbctrl configuration includes. */
#include "config.h"

/* some required global variables. */
static char *program_name;
static struct {
	int window_next;
	int window_prev;
	int desktop_next;
	int desktop_prev;
	int quiet;
	int debug;
} options;

/* some global defines. */
#define MAX_PROPERTY_VALUE_LEN	4096
#define WINDOW_NEXT		1
#define WINDOW_PREV		2
#define DESKTOP_NEXT		3
#define DESKTOP_PREV		4

/* define new print functions for error. */
#define ERROR(...) fprintf(stderr, __VA_ARGS__);

/* define new print functions for debug. */
#define DEBUG(...) if (options.debug == 1) { printf(__VA_ARGS__); }

/* define new print functions for quiet. */
#define NOTICE(...) if (options.quiet != 1) { printf(__VA_ARGS__); }

/* this function show the usage. */
static int fbctrl__usage() {

	/* show the help. */
	printf("Usage: %s [OPTION]...\n", program_name);
	printf("Set focus on another window. (Example: %s --next)\n", program_name);
	printf("\n");
	printf("[main]\n");
	printf("\n");
	printf("  -h, --help		shows this help screen\n");
	printf("  -v, --version		shows the version information\n");
	printf("  -d, --debug		shows many debug information\n");
	printf("  -q, --quiet		shows nothing\n");
	printf("\n");
	printf("[window]\n");
	printf("\n");
	printf("  -n, --next-window	switch to next window on active desktop\n");
	printf("  -p, --prev-window	switch to previous window on active desktop\n");
	printf("      --next-desktop	switch to next workspace on active screen\n");
	printf("      --prev-desktop	switch to previous workspace on active screen\n");
	printf("\n");
	printf("Please report bugs to the appropriate authors, which can be found in the\n");
	printf("version information. All other things can be send to <%s>\n", PACKAGE_BUGREPORT);

	/* if no error was found, return zero. */
	return 0;
}

/* this function shows the version information. */
static int fbctrl__version() {

	/* show the version. */
	printf("%s (fluxnation) %s %s\n", program_name, PACKAGE_VERSION, RELEASE);
	printf("Written by %s\n", AUTHOR);
	printf("\n");
	printf("This is free software; see the source for copying conditions.  There is NO\n");
	printf("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");

	/* if no error was found, return zero. */
	return 0;
}

/* get the window properties. */
static char *fbctrl__get_property(Display *disp, Window win, Atom xa_prop_type, char *prop_name, unsigned long *size) {

	/* some common variables. */
	Atom xa_prop_name;
	Atom xa_ret_type;
	int ret_format;
	unsigned long ret_nitems;
	unsigned long ret_bytes_after;
	unsigned long tmp_size;
	unsigned char *ret_prop;
	char *ret;

	xa_prop_name = XInternAtom(disp, prop_name, False);

	/* MAX_PROPERTY_VALUE_LEN / 4 explanation (XGetWindowProperty manpage):
	 *
	 * long_length = Specifies the length in 32-bit multiples of the
	 *               data to be retrieved.
	 */
	if (XGetWindowProperty(disp, win, xa_prop_name, 0, MAX_PROPERTY_VALUE_LEN / 4, False, xa_prop_type, &xa_ret_type, &ret_format, &ret_nitems, &ret_bytes_after, &ret_prop) != Success) {
		ERROR("Cannot get %s property.\n", prop_name);
		return NULL;
	}

	/* check property type. */
	if (xa_ret_type != xa_prop_type) {
		ERROR("Invalid type of %s property.\n", prop_name);
		XFree(ret_prop);
		return NULL;
	}

	/* null terminate the result to make string handling easier */
	tmp_size = (ret_format / 8) * ret_nitems;
	ret = malloc(tmp_size + 1);
	memcpy(ret, ret_prop, tmp_size);
	ret[tmp_size] = '\0';

	/* set new size. */
	if (size) {
		*size = tmp_size;
	}

	/* free return property and return. */
	XFree(ret_prop);
	return ret;
}

/* this function gets the window client list for the whole desktop. */
static Window *fbctrl__get_client_list_desktop(Display *disp, unsigned long *size) {

	/* some common variables. */
	Window *client_list_desktop;

	if ((client_list_desktop = (Window *)fbctrl__get_property(disp, DefaultRootWindow(disp), XA_WINDOW, "_NET_CLIENT_LIST", size)) == NULL) {
		if ((client_list_desktop = (Window *)fbctrl__get_property(disp, DefaultRootWindow(disp), XA_CARDINAL, "_WIN_CLIENT_LIST", size)) == NULL) {
			ERROR("Cannot get client list properties.\n");
			return NULL;
		}
	}

	/* return the client list if no error occur. */
	return client_list_desktop;
}

/* get the active (focused) window. */
static Window fbctrl__get_active_window(Display *disp) {

	/* some common variables. */
	char *prop;
	unsigned long size;
	Window ret = (Window)0;

	/* get the active window. */
	prop = fbctrl__get_property(disp, DefaultRootWindow(disp), XA_WINDOW, "_NET_ACTIVE_WINDOW", &size);
	if (prop) {
		ret = *((Window*)prop);
		free(prop);
	}

	/* return the active window. */
	return(ret);
}

/* this function sends the client a message. */
static int fbctrl__client_message(Display *disp, Window win, char *msg, unsigned long data0) {
	XEvent event;
	long mask = SubstructureRedirectMask | SubstructureNotifyMask;

	event.xclient.type = ClientMessage;
	event.xclient.serial = 0;
	event.xclient.send_event = True;
	event.xclient.message_type = XInternAtom(disp, msg, False);
	event.xclient.window = win;
	event.xclient.format = 32;
	event.xclient.data.l[0] = data0;
	event.xclient.data.l[1] = 0;
	event.xclient.data.l[2] = 0;
	event.xclient.data.l[3] = 0;
	event.xclient.data.l[4] = 0;

	/* check if event was sent successfully. */
	if (!XSendEvent(disp, DefaultRootWindow(disp), False, mask, &event)) {
		ERROR("Cannot send %s event.\n", msg);
		return 1;
	}

	/* if no error was found, return zero. */
	return 0;
}

/* this function switches to the next or previous window. */
static int fbctrl__switch_window(unsigned int direction) {

	/* some common variables. */
	Display *disp;
	Window *client_list_desktop;
	Window *client_list_workspace = NULL;
	Window active = 0;
	int i;
	int j = 0;
	unsigned long client_list_size;
	unsigned long *cur_desktop = NULL;

	/* show some debug information. */
	DEBUG("Open the X display\n");

	/* try top open the display. */
	if (! (disp = XOpenDisplay(NULL)) ) {
		ERROR("Cannot open display.\n");
		return 1;
	}

	/* get the window client list. */
	if ((client_list_desktop = fbctrl__get_client_list_desktop(disp, &client_list_size)) == NULL) {
		return 1;
	}

	/* get current desktop. */
	if (! (cur_desktop = (unsigned long *)fbctrl__get_property(disp, DefaultRootWindow(disp), XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL))) {
		if (! (cur_desktop = (unsigned long *)fbctrl__get_property(disp, DefaultRootWindow(disp), XA_CARDINAL, "_WIN_WORKSPACE", NULL))) {
			return 1;
		}
	}

	/* check which window is active. */
	active = fbctrl__get_active_window(disp);

	/* print the list */
	for (i = 0; i < client_list_size / sizeof(Window); i++) {

		/* some common variables for this loop. */
		unsigned long *desktop;

		/* get desktop id. */
		if ((desktop = (unsigned long *)fbctrl__get_property(disp, client_list_desktop[i], XA_CARDINAL, "_NET_WM_DESKTOP", NULL)) == NULL) {
			desktop = (unsigned long *)fbctrl__get_property(disp, client_list_desktop[i], XA_CARDINAL, "_WIN_WORKSPACE", NULL);
		}

		/* special desktop id -1 means "all desktops", so we have to convert the desktop value to signed long. */
		DEBUG("Window ID: 0x%.8lx Desktop ID: %i\n", client_list_desktop[i], desktop ? (signed long)*desktop : 0);

		/* check which desktop we are using. */
		if (*cur_desktop == *desktop) {

			/* build new window list based on active workspace. */
			client_list_workspace = realloc(client_list_workspace, (j + 1) * sizeof(Window));
			client_list_workspace[j] = client_list_desktop[i];
			j = j + 1;
		}
	}

	/* show the active desktop. */
	DEBUG("Active Window ID: 0x%.8lx\n", active);
	DEBUG("Active Desktop ID: %i\n", *cur_desktop);

	/* print the list */
	for (i = 0; i < j; i++) {

		/* show some debug information. */
		DEBUG("Active Desktop Window ID: 0x%.8lx\n", client_list_workspace[i]);

		/* check if window is the active one. */
		if (client_list_workspace[i] == active) {

			/* check we should activate next window. */
			if (direction == WINDOW_NEXT) {

				/* check if we are on last window and need to start from the beginning. */
				if (i == j - 1) {

					/* restart from beginning and switch to first created window. */
					NOTICE("Restarting from Window ID: 0x%.8lx on Desktop ID: %i\n", client_list_workspace[0], *cur_desktop);
					fbctrl__client_message(disp, client_list_workspace[0], "_NET_ACTIVE_WINDOW", 0);
				} else {

					/* switch to next window. */
					NOTICE("Activating next Window ID: 0x%.8lx on Desktop ID: %i\n", client_list_workspace[i + 1], *cur_desktop);
					fbctrl__client_message(disp, client_list_workspace[i + 1], "_NET_ACTIVE_WINDOW", 0);
				}
			}

			/* check we should activate previous window. */
			if (direction == WINDOW_PREV) {

				/* check if we are on first window and need to start from the end. */
				if (i == 0) {

					/* restart from end and switch to first created window. */
					NOTICE("Restarting from Window ID: 0x%.8lx on Desktop ID: %i\n", client_list_workspace[j - 1], *cur_desktop);
					fbctrl__client_message(disp, client_list_workspace[j - 1], "_NET_ACTIVE_WINDOW", 0);
				} else {

					/* switch to previous window. */
					NOTICE("Activating prev Window ID: 0x%.8lx on Desktop ID: %i\n", client_list_workspace[i - 1], *cur_desktop);
					fbctrl__client_message(disp, client_list_workspace[i - 1], "_NET_ACTIVE_WINDOW", 0);
				}
			}
		}
	}

	/* show some debug information. */
	DEBUG("Free X resources\n");

	/* free the memory. */
	free(client_list_workspace);
	free(client_list_desktop);

	/* show some debug information. */
	DEBUG("Close the X display\n");

	/* close the display handle. */
	XCloseDisplay(disp);

	/* if no error was found, return zero. */
	return 0;
}

/* this function switches to the next or previous desktop. */
static int fbctrl__switch_desktop(unsigned int direction) {

	/* some common variables. */
	Display *disp;
	unsigned long *cur_desktop = NULL;
	unsigned long *num_desktops = NULL;

	/* show some debug information. */
	DEBUG("Open the X display\n");

	/* try top open the display. */
	if (! (disp = XOpenDisplay(NULL)) ) {
		ERROR("Cannot open display.\n");
		return 1;
	}

	/* get current desktop. */
	if (! (cur_desktop = (unsigned long *)fbctrl__get_property(disp, DefaultRootWindow(disp), XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL))) {
		if (! (cur_desktop = (unsigned long *)fbctrl__get_property(disp, DefaultRootWindow(disp), XA_CARDINAL, "_WIN_WORKSPACE", NULL))) {
			return 1;
		}
	}

	/* show the active desktop. */
	DEBUG("Active Desktop ID: %i\n", *cur_desktop);

	/* get number of desktops. */
	if (! (num_desktops = (unsigned long *)fbctrl__get_property(disp, DefaultRootWindow(disp), XA_CARDINAL, "_NET_NUMBER_OF_DESKTOPS", NULL))) {
		if (! (num_desktops = (unsigned long *)fbctrl__get_property(disp, DefaultRootWindow(disp), XA_CARDINAL, "_WIN_WORKSPACE_COUNT", NULL))) {
			return 1;
		}
	}

	/* show the number of desktops. */
	DEBUG("Desktop Count: %i\n", *num_desktops);

	/* check we should activate next desktop. */
	if (direction == DESKTOP_NEXT) {

		/* check if we are on last desktop and need to start from the beginning. */
		if (*cur_desktop == *num_desktops - 1) {

			/* restart from beginning and switch to first desktop. */
			NOTICE("Restarting from Desktop ID: 0\n");
			fbctrl__client_message(disp, DefaultRootWindow(disp), "_NET_CURRENT_DESKTOP", 0);
		} else {

			/* switch to next desktop. */
			NOTICE("Activating next Desktop ID: %i\n", (*cur_desktop + 1));
			fbctrl__client_message(disp, DefaultRootWindow(disp), "_NET_CURRENT_DESKTOP", (*cur_desktop + 1));
		}
	}

	/* check we should activate previous desktop. */
	if (direction == DESKTOP_PREV) {

		/* check if we are on first desktop and need to start from the end. */
		if (*cur_desktop == 0) {

			/* restart from end and switch to first desktop. */
			NOTICE("Restarting Desktop ID: %i\n", (*num_desktops - 1));
			fbctrl__client_message(disp, DefaultRootWindow(disp), "_NET_CURRENT_DESKTOP", (*num_desktops - 1));
		} else {

			/* switch to previous desktop. */
			NOTICE("Activating prev Desktop ID: %i\n", (*cur_desktop - 1));
			fbctrl__client_message(disp, DefaultRootWindow(disp), "_NET_CURRENT_DESKTOP", (*cur_desktop - 1));
		}
	}

	/* show some debug information. */
	DEBUG("Close the X display\n");

	/* close the display handle. */
	XCloseDisplay(disp);

	/* if no error was found, return zero. */
	return 0;
}

/* the main function starts here. */
int main(int argc, char **argv) {

	/* common variables for the command line. */
	int opt;
	int option_index = 0;
	static char const short_options[] = "hvdqnp";
	static struct option const long_options[] = {
		{"help",		no_argument,	0,	'h'},
		{"version",		no_argument,	0,	'v'},
		{"debug",		no_argument,	0,	'd'},
		{"quiet",		no_argument,	0,	'q'},
		{"next-window",		no_argument,	0,	'n'},
		{"prev-window",		no_argument,	0,	'p'},
		{"next-desktop",	no_argument,	0,	1},
		{"prev-desktop",	no_argument,	0,	2},
		{0,			0,		0,	0}
        };
	optind = 0;
	opterr = 0;

	/* get program name. */
	program_name = argv[0];
	if (program_name && strrchr(program_name, '/')) {
		program_name = strrchr(program_name, '/') + 1;
	}

	/* if no command line option was given, show some info. */
	if (argc <= 1) {

		/* show some info on how to get help. :) */
		ERROR("%s: no action was given\n", program_name);
		ERROR("Try `%s --help' for more information.\n", program_name);

		/* exit with error. */
		exit(1);
	}

	/* cleanup. */
	memset(&options, 0, sizeof(options));

	/* parse command line. */
	while ((opt = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {

		if (opt == -1) {
			break;
		}

		/* parse option. */
		switch (opt) {
			case 'h':
				fbctrl__usage();
				exit(0);
			case 'v':
				fbctrl__version();
				exit(0);
			case 'n':
				options.window_next = 1;
				break;
			case 'p':
				options.window_prev = 1;
				break;
			case 'd':
				options.debug = 1;
				break;
			case 'q':
				options.quiet = 1;
				break;
			case 1:
				options.desktop_next = 1;
				break;
			case 2:
				options.desktop_prev = 1;
				break;
			default:

				/* show some info on how to get help. :) */
				ERROR("%s: unrecognized option `%s'\n", program_name, argv[optind - 1]);
				ERROR("Try `%s --help' for more information.\n", program_name);

				/* exit with error. */
				exit(1);
		}
	}

	/* check if we should activate next window. */
	if (options.window_next == 1) {
		if (fbctrl__switch_window(WINDOW_NEXT) != 0) {
			exit(1);
		}
	}

	/* check if we should activate previous window. */
	if (options.window_prev == 1) {
		if (fbctrl__switch_window(WINDOW_PREV) != 0) {
			exit(1);
		}
	}

	/* check if we should activate next desktop. */
	if (options.desktop_next == 1) {
		if (fbctrl__switch_desktop(DESKTOP_NEXT) != 0) {
			exit(1);
		}
	}

	/* check if we should activate previous desktop. */
	if (options.desktop_prev == 1) {
		if (fbctrl__switch_desktop(DESKTOP_PREV) != 0) {
			exit(1);
		}
	}

	/* execution was successful. */
	exit(0);
}

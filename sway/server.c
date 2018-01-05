#define _POSIX_C_SOURCE 200112L
#include <stdlib.h>
#include <stdbool.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/render.h>
#include <wlr/render/gles2.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_wl_shell.h>
// TODO WLR: make Xwayland optional
#include <wlr/xwayland.h>
#include <wlr/util/log.h>
#include "sway/server.h"
#include "sway/input/input-manager.h"

bool server_init(struct sway_server *server) {
	wlr_log(L_DEBUG, "Initializing Wayland server");

	server->wl_display = wl_display_create();
	server->wl_event_loop = wl_display_get_event_loop(server->wl_display);
	server->backend = wlr_backend_autocreate(server->wl_display);

	server->renderer = wlr_gles2_renderer_create(server->backend);
	wl_display_init_shm(server->wl_display);

	server->compositor = wlr_compositor_create(
			server->wl_display, server->renderer);

	server->data_device_manager =
		wlr_data_device_manager_create(server->wl_display);

	server->output_add.notify = output_add_notify;
	wl_signal_add(&server->backend->events.output_add, &server->output_add);

	server->output_remove.notify = output_remove_notify;
	wl_signal_add(&server->backend->events.output_remove,
			&server->output_remove);

	server->xdg_shell_v6 = wlr_xdg_shell_v6_create(server->wl_display);
	wl_signal_add(&server->xdg_shell_v6->events.new_surface,
		&server->xdg_shell_v6_surface);
	server->xdg_shell_v6_surface.notify = handle_xdg_shell_v6_surface;

	// TODO make xwayland optional
	server->xwayland =
		wlr_xwayland_create(server->wl_display, server->compositor);
	wl_signal_add(&server->xwayland->events.new_surface,
		&server->xwayland_surface);
	server->xwayland_surface.notify = handle_xwayland_surface;

	server->wl_shell = wlr_wl_shell_create(server->wl_display);
	wl_signal_add(&server->wl_shell->events.new_surface,
		&server->wl_shell_surface);
	server->wl_shell_surface.notify = handle_wl_shell_surface;

	server->socket = wl_display_add_socket_auto(server->wl_display);
	if (!server->socket) {
		wlr_log(L_ERROR, "Unable to open wayland socket");
		wlr_backend_destroy(server->backend);
		return false;
	}

	input_manager = sway_input_manager_create(server);

	return true;
}

void server_fini(struct sway_server *server) {
	// TODO WLR: tear down more stuff
	wlr_backend_destroy(server->backend);
}

void server_run(struct sway_server *server) {
	wlr_log(L_INFO, "Running compositor on wayland display '%s'",
			server->socket);
	setenv("_WAYLAND_DISPLAY", server->socket, true);
	if (!wlr_backend_start(server->backend)) {
		wlr_log(L_ERROR, "Failed to start backend");
		wlr_backend_destroy(server->backend);
		return;
	}
	setenv("WAYLAND_DISPLAY", server->socket, true);
	wl_display_run(server->wl_display);
}
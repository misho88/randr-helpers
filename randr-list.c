#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <xcb/xcb.h>
#include <xcb/randr.h>
#include <unistd.h>

#define EDID_HEADER UINT64_C(0x00FFFFFFFFFFFF00)

void
die(const char * message)
{
	fprintf(stderr, "%s\n", message);
	exit(1);
}

xcb_connection_t *
get_connection()
{
	xcb_connection_t * connection = xcb_connect(NULL, NULL);
	if (connection == NULL) die("failed to connect to server");
	return connection;
}

struct outputs { void * reply; int count; xcb_randr_output_t * outputs; xcb_timestamp_t timestamp; }
get_outputs(xcb_connection_t * connection)
{
	xcb_screen_t * screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
	if (screen == NULL) die("got a NULL screen");
	xcb_window_t root = screen->root;
	xcb_randr_select_input(connection, root, XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE);
	xcb_flush(connection);

	xcb_randr_get_screen_resources_current_reply_t * reply;
	reply = xcb_randr_get_screen_resources_current_reply(
		connection,
		xcb_randr_get_screen_resources_current(connection, root),
		NULL
	);
	if (reply == NULL) die("got NULL from xcb_randr_get_screen_resources_current_reply()");

	return (struct outputs) {
		.reply = reply,
		.count = xcb_randr_get_screen_resources_current_outputs_length(reply),
		.outputs = xcb_randr_get_screen_resources_current_outputs(reply),
		.timestamp = reply->config_timestamp,
	};
}

struct output_info { void * reply; int name_length; uint8_t * name; bool connected; }
get_output_info(xcb_connection_t * connection, xcb_randr_output_t output, xcb_timestamp_t timestamp)
{
	xcb_randr_get_output_info_reply_t * reply;
	reply = xcb_randr_get_output_info_reply(
		connection,
		xcb_randr_get_output_info(connection, output, timestamp),
		NULL
	);
	if (reply == NULL) die("got NULL from xcb_randr_get_output_info_reply()");
	if (reply->connection == XCB_RANDR_CONNECTION_UNKNOWN) die("got XCB_RANDR_CONNECTION_UNKNOWN about the connection state");
	return (struct output_info) {
		.reply = reply,
		.name_length = xcb_randr_get_output_info_name_length(reply),
		.name = xcb_randr_get_output_info_name(reply),
		.connected = reply->connection == XCB_RANDR_CONNECTION_CONNECTED,
	};
}

struct output_properties { void * reply; int count; xcb_atom_t * properties; }
get_output_properties(xcb_connection_t * connection, xcb_randr_output_t output)
{
	xcb_randr_list_output_properties_reply_t * reply;
	reply = xcb_randr_list_output_properties_reply(
		connection,
		xcb_randr_list_output_properties(connection, output),
		NULL
	);
	if (reply == NULL) die("got NULL from xcb_randr_list_output_properties_reply()");

	return (struct output_properties){
		.reply = reply,
		.count = xcb_randr_list_output_properties_atoms_length(reply),
		.properties = xcb_randr_list_output_properties_atoms(reply),
	};
}

struct output_property_data { void * reply; int size; uint8_t * data; bool is_edid; }
get_output_property_data(xcb_connection_t * connection, xcb_randr_output_t output, xcb_atom_t property)
{
	xcb_randr_get_output_property_reply_t * reply;
	reply = xcb_randr_get_output_property_reply(
		connection,
		xcb_randr_get_output_property(connection, output, property, 0, 0, 0xffffffffUL, 0, 0),
		NULL
	);
	if (reply == NULL) die("got NULL from xcb_randr_get_output_property_reply()");
	int size = xcb_randr_get_output_property_data_length(reply);
	uint8_t * data = xcb_randr_get_output_property_data(reply);
	return (struct output_property_data){
		.reply = reply,
		.size = size,
		.data = data,
		.is_edid = size >= 8 && *(uint64_t *)data == EDID_HEADER,
	};
}

uint8_t
letter(uint16_t id, int offset)
{
	uint8_t code = (id >> offset) & 0x1F;
	return 'A' + code - 1;
}

struct edid_info { uint8_t manufacturer[4]; uint16_t model; uint32_t serial; }
get_edid_info(int size, uint8_t * data)
{
	if (8 > size || *(uint64_t *)data != EDID_HEADER) die("got invalid EDID");
	data += 8;
	size -= 8;

	if (2 > size) die("got invalid EDID");
	uint16_t id = (data[0] << 8) | data[1]; /* big-endian field */
	data += 2;
	size -= 2;

	if (2 > size) die("got invalid EDID");
	uint16_t model = (data[1] << 8) | data[0]; /* little-endian field */
	data += 2;
	size -= 2;

	if (4 > size) die("got invalid EDID");
	uint32_t serial = (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
	return (struct edid_info){
		.manufacturer = { letter(id, 10), letter(id, 5), letter(id, 0), 0 },
		.model = model,
		.serial = serial,
	};
}

struct edid_info
find_edid_info(xcb_connection_t * connection, xcb_randr_output_t output)
{
	struct edid_info edid_info = { "???", 0, 0 };
	struct output_properties output_properties = get_output_properties(connection, output);
	for (int j = 0; j < output_properties.count; j++) {
		xcb_atom_t property = output_properties.properties[j];

		struct output_property_data data = get_output_property_data(connection, output, property);
		if (data.is_edid) {
			edid_info = get_edid_info(data.size, data.data);
			free(data.reply);
			break;
		}
		free(data.reply);
	}
	free(output_properties.reply);
	return edid_info;
}


int
main()
{
	xcb_connection_t * connection = get_connection();

	struct outputs outputs = get_outputs(connection);
	for (int i = 0; i < outputs.count; i++) {
		const char * end = i < outputs.count - 1 ? "," : isatty(STDOUT_FILENO) ? "\n" : "";
		xcb_randr_output_t output = outputs.outputs[i];

		struct output_info output_info = get_output_info(connection, output, outputs.timestamp);
		printf("%.*s:", output_info.name_length, output_info.name);
		free(output_info.reply);
		if (!output_info.connected) {
			printf("%s%s", "::", end);
			continue;
		}

		struct edid_info edid = find_edid_info(connection, output);
		printf("%s:%d:%d%s", edid.manufacturer, edid.model, edid.serial, end);
	}
	free(outputs.reply);
	free(connection);
	return 0;
}

/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <clog/console.h>
#include <stdio.h>
#include <udp-client/udp_client.h>

#include <unistd.h>

clog_config g_clog;

int main(int argc, char* argv[])
{
	(void) argc;
	(void) argv;

	g_clog.log = clog_console;
	CLOG_VERBOSE("example start")
	UdpClientSocket socket;

	udpClientInit(&socket, "127.0.0.1", 27000);
	CLOG_VERBOSE("initialized")
	while (1) {
		CLOG_INFO("sending")
		udpClientSend(&socket, (const uint8_t*) "Hello", 5);
		sleep(1);
	}
}

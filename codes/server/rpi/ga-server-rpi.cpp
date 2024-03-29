/*
 * Copyright (c) 2014-2015 Chun-Ying Huang
 *
 * This file is part of GamingAnywhere (GA).
 *
 * GA is free software; you can redistribute it and/or modify it
 * under the terms of the 3-clause BSD License as published by the
 * Free Software Foundation: http://directory.fsf.org/wiki/License:BSD_3Clause
 *
 * GA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the 3-clause BSD License along with GA;
 * if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <bcm_host.h>
#include <interface/vcos/vcos_semaphore.h>
#include <interface/vmcs_host/vchost.h>
#include <IL/OMX_Core.h>
#include <IL/OMX_Component.h>
#include <IL/OMX_Video.h>
#include <IL/OMX_Broadcom.h>
#ifdef __cplusplus
}
#endif
#include "ga-common.h"
#include "ga-conf.h"
#include "ga-module.h"
#include "rtspconf.h"
#include "controller.h"
#include "encoder-common.h"

//#define	TEST_RECONFIGURE

// image source pipeline:
//	vsource -- [vsource-%d] --> filter -- [filter-%d] --> encoder

// configurations:
static char *imagepipefmt = "video-%d";
static char *imagepipe0 = "video-0";
static char *video_encoder_param = imagepipefmt;
static void *audio_encoder_param = NULL;

static ga_module_t *m_vsource, *m_vencoder, *m_server;

int
load_modules() {
#if 0
	if((m_vsource = ga_load_module("mod/vsource-omx", "vsource_")) == NULL)
		return -1;
	if((m_vencoder = ga_load_module("mod/encoder-omx", "vencoder_")) == NULL)
		return -1;
#else
	if((m_vencoder = ga_load_module("mod/streamer-omx", "vencoder_")) == NULL)
		return -1;
#endif
	if((m_server = ga_load_module("mod/server-live555", "live_")) == NULL)
		return -1;
	return 0;
}

int
init_modules() {
#if 0
	ga_init_single_module_or_quit("video source", m_vsource, NULL);
#endif
	ga_init_single_module_or_quit("video encoder", m_vencoder, imagepipefmt);
	ga_init_single_module_or_quit("server-live555", m_server, NULL);
	return 0;
}

int
run_modules() {
#if 0
	if(m_vsource->start(NULL) < 0)	
		exit(-1);
#endif
	encoder_register_vencoder(m_vencoder, video_encoder_param);
	// server
	if(m_server->start(NULL) < 0)		exit(-1);
	//
	return 0;
}

#ifdef TEST_RECONFIGURE
static void *
test_reconfig(void *) {
	int s = 0, err;
	int kbitrate[] = { 1000, 10000 };
	int width[] = { 160, 640 };
	int height[] = { 120, 480 };
	ga_error("reconfigure thread started ...\n");
	while(1) {
		ga_ioctl_reconfigure_t reconf;
		if(encoder_running() == 0) {
			sleep(1);
			continue;
		}
		sleep(70);
		bzero(&reconf, sizeof(reconf));
		reconf.id = 0;
		reconf.bitrateKbps = kbitrate[s];
		reconf.bufsize = 5 * kbitrate[s] / 24;
		reconf.width = width[s];
		reconf.height = height[s];
		err = m_vencoder->ioctl(GA_IOCTL_RECONFIGURE, sizeof(reconf), &reconf);
		if(err < 0) {
			ga_error("reconfigure failed, err = %d.\n", err);
		} else {
			ga_error("reconfigure OK, bitrate=%d; bufsize=%d; width=%d; height=%d.\n",
				reconf.bitrateKbps, reconf.bufsize,
				reconf.width, reconf.height);
		}
		s = (s + 1) & 1;
	}
	return NULL;
}
#endif

void
handle_netreport(ctrlmsg_system_t *msg) {
	ctrlmsg_system_netreport_t *msgn = (ctrlmsg_system_netreport_t*) msg;
	ga_error("net-report: capacity=%.3f Kbps; loss-rate=%.2f%% (%u/%u); overhead=%.2f [%u KB received in %.3fs (%.2fKB/s)]\n",
		msgn->capacity / 1024.0,
		100.0 * msgn->pktloss / msgn->pktcount,
		msgn->pktloss, msgn->pktcount,
		1.0 * msgn->pktcount / msgn->framecount,
		msgn->bytecount / 1024,
		msgn->duration / 1000000.0,
		msgn->bytecount / 1024.0 / (msgn->duration / 1000000.0));
	return;
}

int
main(int argc, char *argv[]) {
	int notRunning = 0;
	//
	bcm_host_init();
	if(OMX_Init() != OMX_ErrorNone) {
		ga_error("OMX_Init() failed.\n");
		return -1;
	}
	//
	if(argc < 2) {
		fprintf(stderr, "usage: %s config-file\n", argv[0]);
		return -1;
	}
	//
	if(ga_init(argv[1], NULL) < 0)	{ return -1; }
	//
	ga_openlog();
	//
	if(rtspconf_parse(rtspconf_global()) < 0)
					{ return -1; }
	// load controller
	ga_run_single_module_or_quit("controller", ctrl_server_thread,
		rtspconf_global());
	//
	if(load_modules() < 0)	 	{ return -1; }
	if(init_modules() < 0)	 	{ return -1; }
	if(run_modules() < 0)	 	{ return -1; }
	// enable handler to monitored network status
	ctrlsys_set_handler(CTRL_MSGSYS_SUBTYPE_NETREPORT, handle_netreport);
	//
#ifdef TEST_RECONFIGURE
	pthread_t t;
	pthread_create(&t, NULL, test_reconfig, NULL);
#endif
	while(1) {
		usleep(5000000);
	}
	//
	ga_deinit();
	OMX_Deinit();
	//
	return 0;
}


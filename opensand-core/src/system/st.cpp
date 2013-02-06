/*
 *
 * OpenSAND is an emulation testbed aiming to represent in a cost effective way a
 * satellite telecommunication system for research and engineering activities.
 *
 *
 * Copyright © 2012 TAS
 * Copyright © 2012 CNES
 *
 *
 * This file is part of the OpenSAND testbed.
 *
 *
 * OpenSAND is free software : you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY, without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see http://www.gnu.org/licenses/.
 *
 */

/**
 * @file st.cpp
 * @brief Satellite station (ST) process
 * @author Didier Barvaux <didier.barvaux@toulouse.viveris.com>
 *
 * ST uses the following stack of Margouilla blocs installed over 2 NICs
 * (nic1 on user network side and nic2 on satellite network side):
 *
 * <pre>
 *
 *                     eth nic 1
 *                         |
 *                      IP QoS  --------------
 *                         |                  |
 *                   Encap/Desencap      IpMacQoSInteraction
 *                         |                  |
 *                    Dvb Rcs Tal  -----------
 *                    [Dama Agent]
 *                         |
 *                  Sat Carrier Eth
 *                         |
 *                     eth nic 2
 *
 * </pre>
 *
 */


// System includes
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

// Scheduling policy
#include <sched.h>

// Margouilla includes
#include "opensand_margouilla/mgl_eventmgr.h"
#include "opensand_margouilla/mgl_bloc.h"
#include "opensand_margouilla/mgl_blocmgr.h"

// Project includes
#include "opensand_conf/uti_debug.h"
#include "opensand_conf/ConfigurationFile.h"

#include "bloc_ip_qos.h"
#include "bloc_encap.h"
#include "bloc_dvb_rcs_tal.h"
#include "bloc_sat_carrier.h"
#include "BlocPhysicalLayer.h"
#include "PluginUtils.h"

// output include
#include "opensand_output/Output.h"


/// global variable saying whether the ST component is alive or not
bool alive = true;


/**
 * Argument treatment
 */
bool init_process(int argc, char **argv,
                  string &ip_addr,
                  string &iface_name,
                  tal_id_t &instance_id)
{
	int opt;
	bool output_enabled = true;
	event_level_t output_event_level = LEVEL_INFO;

	/* setting environment agent parameters */
	while((opt = getopt(argc, argv, "-hqdi:a:n:")) != EOF)
	{
		switch(opt)
		{
		case 'q':
			// disable output
			output_enabled = false;
			break;
		case 'd':
			// enable output debug
			output_event_level = LEVEL_DEBUG;
		case 'i':
			// get instance id
			instance_id = atoi(optarg);
			break;
		case 'a':
			// get local IP address
			ip_addr = optarg;
			break;
		case 'n':
			// get local interface name
			iface_name = optarg;
			break;
		case 'h':
		case '?':
			fprintf(stderr, "usage: %s [-h] [[-q] [-d] -i instance_id -a ip_address "
				"-n interface_name]\n",
			        argv[0]);
			fprintf(stderr, "\t-h                   print this message\n");
			fprintf(stderr, "\t-q                   disable output\n");
			fprintf(stderr, "\t-d                   enable output debug events\n");
			fprintf(stderr, "\t-a <ip_address>      set the IP address\n");
			fprintf(stderr, "\t-n <interface_name>  set the interface name\n");
			fprintf(stderr, "\t-i <instance>        set the instance id\n");

			UTI_ERROR("usage printed on stderr\n");
			return false;
		}
	}

	UTI_PRINT(LOG_INFO, "starting output\n");

	// output initialisation
	Output::init(output_enabled, output_event_level);

	if(ip_addr.size() == 0)
	{
		UTI_ERROR("missing mandatory IP address option");
		return false;
	}

	if(iface_name.size() == 0)
	{
		UTI_ERROR("missing mandatory interface name option");
		return false;
	}
	return true;
}


// signal handler
void sigendHandler(int sig)
{
	UTI_PRINT(LOG_INFO, "Signal %d received, terminate the process\n", sig);
	alive = false;
}


int main(int argc, char **argv)
{
	const char *progname = argv[0];
	struct sched_param param;
	bool is_init = false;
	bool with_phy_layer = false;
	string ip_addr;
	string iface_name;
	tal_id_t mac_id;

	mgl_eventmgr *eventmgr;
	mgl_blocmgr *blocmgr;

	BlocIPQoS *blocIPQoS;
	BlocEncap *blocEncap;
	BlocDVBRcsTal *blocDvbRcsTal;
    BlocPhysicalLayer *blocPhysicalLayer;
	BlocSatCarrier *blocSatCarrier;
	PluginUtils utils;
	vector<string> conf_files;

	Event *status = NULL;
	Event *failure = NULL;

	int is_failure = 1;

	// catch TERM and INT signals
	signal(SIGTERM, sigendHandler);
	signal(SIGINT, sigendHandler);

	// retrieve arguments on command line
	if(init_process(argc, argv, ip_addr, iface_name, mac_id) == false)
	{
		UTI_ERROR("%s: failed to init the process\n", progname);
		goto quit;
	}

	// increase the realtime responsiveness of the process
	param.sched_priority = sched_get_priority_max(SCHED_FIFO);
	sched_setscheduler(0, SCHED_FIFO, &param);

	conf_files.push_back(CONF_TOPOLOGY);
	conf_files.push_back(CONF_GLOBAL_FILE);
	conf_files.push_back(CONF_DEFAULT_FILE);
	// Load configuration files content
	if(!globalConfig.loadConfig(conf_files))
	{
		UTI_ERROR("%s: cannot load configuration files, quit\n",
		          progname);
		goto unload_config;
	}

	// read all packages debug levels
	UTI_readDebugLevels();

	// Retrieve the value of the ‘enable’ parameter for the physical layer
	if(!globalConfig.getValue(PHYSICAL_LAYER_SECTION, ENABLE,
	                          with_phy_layer))
	{
		UTI_ERROR("%s: cannot  check if physical layer is enabled\n",
		          progname);
		goto unload_config;
	}
	UTI_PRINT(LOG_INFO, "%s: physical layer is %s\n",
	          progname, with_phy_layer ? "enabled" : "disabled");

	// instantiate event manager
	eventmgr = new mgl_eventmgr(realTime);
	if(eventmgr == NULL)
	{
		UTI_ERROR("%s: cannot create the event manager\n", progname);
		goto unload_config;
	}

	// instantiate bloc manager
	blocmgr = new mgl_blocmgr();
	if(blocmgr == NULL)
	{
		UTI_ERROR("%s: cannot create the bloc manager\n", progname);
		goto destroy_eventmgr;
	}

	MGL_TRACE_SET_LEVEL(0);		  // set mgl runtime debug level
	blocmgr->setEventMgr(eventmgr);

	// load the plugins
	if(!utils.loadPlugins(with_phy_layer))
	{
		UTI_ERROR("%s: cannot load the plugins\n", progname);
		goto destroy_blocmgr;
	}

	// instantiate all blocs
	blocIPQoS = new BlocIPQoS(blocmgr, 0, "IP-QoS", terminal);
	if(blocIPQoS == NULL)
	{
		UTI_ERROR("%s: cannot create the IP-QoS bloc\n", progname);
		goto release_plugins;
	}

	blocEncap = new BlocEncap(blocmgr, 0, "Encap", terminal, utils);
	if(blocEncap == NULL)
	{
		UTI_ERROR("%s: cannot create the Encap bloc\n", progname);
		goto release_plugins;
	}

	blocIPQoS->setLowerLayer(blocEncap->getId());
	blocEncap->setUpperLayer(blocIPQoS->getId());

	blocDvbRcsTal = new BlocDVBRcsTal(blocmgr, 0, "DvbRcsTal",
	                                  mac_id, utils);
	if(blocDvbRcsTal == NULL)
	{
		UTI_ERROR("%s: cannot create the DvbRcsTal bloc\n", progname);
		goto release_plugins;
	}

	blocEncap->setLowerLayer(blocDvbRcsTal->getId());
	blocDvbRcsTal->setUpperLayer(blocEncap->getId());

	blocSatCarrier = new BlocSatCarrier(blocmgr, 0, "SatCarrier",
	                                    terminal, ip_addr, iface_name);
	if(blocSatCarrier == NULL)
	{
		UTI_ERROR("%s: cannot create the SatCarrier bloc\n", progname);
		goto release_plugins;
	}

	if(with_phy_layer)
	{
		blocPhysicalLayer = new BlocPhysicalLayer(blocmgr, 0,
		                                          "PhysicalLayer",
		                                          terminal,
		                                          utils);
		if(blocPhysicalLayer == NULL)
		{
			UTI_ERROR("%s: cannot create the PhysicalLayer bloc\n", progname);
			goto release_plugins;
		}

		blocDvbRcsTal->setLowerLayer(blocPhysicalLayer->getId());
		blocPhysicalLayer->setUpperLayer(blocDvbRcsTal->getId());

		blocPhysicalLayer->setLowerLayer(blocSatCarrier->getId());
		blocSatCarrier->setUpperLayer(blocPhysicalLayer->getId());
	}
	else
	{
		blocDvbRcsTal->setLowerLayer(blocSatCarrier->getId());
		blocSatCarrier->setUpperLayer(blocDvbRcsTal->getId());
	}


	// make the ST alive
	while(alive)
	{
		blocmgr->process_step();
		if(!is_init && blocmgr->isRunning())
		{
			// finish output init, sent the initial event
			failure = Output::registerEvent("failure", LEVEL_ERROR);
			status = Output::registerEvent("status", LEVEL_INFO);
			if(!Output::finishInit())
			{
				UTI_PRINT(LOG_INFO,
				          "%s: failed to init the output => disable it\n",
				         progname);
			}

			Output::sendEvent(status, "Simulation started");
			is_init = true;
		}
	}

	Output::sendEvent(status, "Simulation stopped");

	// everything went fine, so report success
	is_failure = 0;

	// cleanup before ST stops
release_plugins:
	utils.releasePlugins();
destroy_blocmgr:
	delete blocmgr; /* destroy the bloc manager and all the blocs */
destroy_eventmgr:
	delete eventmgr;
unload_config:
	if(is_failure)
	{
		Output::sendEvent(failure, "Failure while launching component\n");
	}
	globalConfig.unloadConfig();
quit:
	UTI_PRINT(LOG_INFO, "%s: end of the ST process\n", progname);
	closelog();
	return is_failure;
}

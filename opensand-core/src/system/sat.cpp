/*
 *
 * OpenSAND is an emulation testbed aiming to represent in a cost effective way a
 * satellite telecommunication system for research and engineering activities.
 *
 *
 * Copyright © 2019 TAS
 * Copyright © 2019 CNES
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
 * @file sat.cpp
 * @brief satellite emulator process
 * @author Didier Barvaux <didier.barvaux@toulouse.viveris.com>
 * @author Julien BERNARD <jbernard@toulouse.viveris.com>
 * @author Joaquin MUGUERZA <jmuguerza@toulouse.viveris.com>
 * @author Aurelien DELRIEU <adelrieu@toulouse.viveris.com>
 *
 * SE uses the following stack of mgl blocs installed over 1 NIC:
 *
 * <pre>
 *
 *                +---+
 *                |   |
 *            Encap/Desencap
 *                |   |
 *               Dvb Sat
 *                |   |
 *           Sat Carrier Eth
 *                |   |
 *               eth nic
 *
 * </pre>
 *
 *
 */


#include "BlockEncapSat.h"
#include "BlockDvbSatTransp.h"
#include "BlockDvbSatRegen.h"
#include "BlockSatCarrier.h"
#include "BlockPhysicalLayerSat.h"
#include "Plugin.h"
#include "OpenSandConf.h"

#include <opensand_conf/conf.h>
#include <opensand_output/Output.h>
#include <opensand_rt/Rt.h>

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sched.h>
#include <unistd.h>
#include <iostream>


/**
 * Argument treatment
 */
bool init_process(int argc, char **argv, std::string &ip_addr, std::string &conf_path)
{
  int opt;
  std::string output_folder = "";
  std::string remote_address = "";
  unsigned short stats_port = 12345;
  unsigned short logs_port = 23456;
  bool stop = false;
  std::string entity{"sat"};

  /* setting environment agent parameters */
  while(!stop && (opt = getopt(argc, argv, "-ha:c:f:r:l:s:")) != EOF)
  {
    switch(opt)
    {
      case 'a':
        /// get local IP address
        ip_addr = optarg;
        break;
      case 'c':
        // get the configuration path
        conf_path = optarg;
        break;
      case 'f':
        output_folder = optarg;
        break;
      case 'r':
        remote_address = optarg;
        break;
      case 'l':
        logs_port = atoi(optarg);
        break;
      case 's':
        stats_port = atoi(optarg);
        break;
      case 'h':
      case '?':
        std::cerr << "usage: " << argv[0] << " [-h] -a ip_address -c conf_path "
                     "[-f output_folder] [-r remote_address [-l logs_port] [-s stats_port]]\n"
                     "\t-h                       print this message\n"
                     "\t-a <ip_address>          set the IP address\n"
                     "\t-c <conf_path>           specify the configuration path\n"
                     "\t-f <output_folder>       activate and specify the folder for logs and probes files\n"
                     "\t-r <remote_address>      activate and specify the address for logs and probes socket messages\n"
                     "\t-l <logs_port>           specify the port for logs socket messages\n"
                     "\t-s <stats_port>          specify the port for probes socket messages\n";
        stop = true;
        break;
    }
  }

  if (!output_folder.empty())
  {
    stop = stop || !Output::Get()->configureLocalOutput(output_folder, entity);
  }

  if (!remote_address.empty())
  {
    stop = stop || !Output::Get()->configureRemoteOutput(remote_address, stats_port, logs_port);
  }

  if(stop)
  {
    return false;
  }



  DFLTLOG(LEVEL_NOTICE,
          "starting output\n");

  if(ip_addr.size() == 0)
  {
    DFLTLOG(LEVEL_CRITICAL,
            "missing mandatory IP address option\n");
    return false;
  }

  if(conf_path.size() == 0)
  {
    DFLTLOG(LEVEL_CRITICAL,
            "missing mandatory configuration path option\n");
    return false;
  }
  return true;
}


int main(int argc, char **argv)
{
  const char *progname = argv[0];
  struct sched_param param;
  bool init_ok;
  std::string ip_addr;
  struct sc_specific specific;

  std::string satellite_type;

  Block *block_encap;
  Block *block_dvb;
  Block *block_phy_layer;
  Block *up_sat_carrier;
  Block *block_sat_carrier;

  std::string conf_path;
  std::string plugin_conf_path;
  std::string topology_file;
  std::string global_file;
  std::string default_file;

  vector<std::string> conf_files;
  map<std::string, log_level_t> levels;
  map<std::string, log_level_t> spec_level;

  std::shared_ptr<OutputEvent> status;

  int is_failure = 1;

  // retrieve arguments on command line
  init_ok = init_process(argc, argv, ip_addr, conf_path);

  plugin_conf_path = conf_path + std::string("plugins/");

  status = Output::Get()->registerEvent("Status");
  if(!init_ok)
  {
    DFLTLOG(LEVEL_CRITICAL,
            "%s: failed to init the process\n", progname);
    goto quit;
  }


  // increase the realtime responsiveness of the process
  param.sched_priority = sched_get_priority_max(SCHED_FIFO);
  sched_setscheduler(0, SCHED_FIFO, &param);

  topology_file = conf_path + std::string(CONF_TOPOLOGY);
  global_file = conf_path + std::string(CONF_GLOBAL_FILE);
  default_file = conf_path + std::string(CONF_DEFAULT_FILE);

  conf_files.push_back(topology_file.c_str());
  conf_files.push_back(global_file.c_str());
  conf_files.push_back(default_file.c_str());
  // Load configuration files content
  if(!Conf::loadConfig(conf_files))
  {
    DFLTLOG(LEVEL_CRITICAL,
            "%s: cannot load configuration files, quit\n",
            progname);
    goto quit;
  }

  OpenSandConf::loadConfig();

  // read all packages debug levels
  if(!Conf::loadLevels(levels, spec_level))
  {
    DFLTLOG(LEVEL_CRITICAL,
            "%s: cannot load default levels, quit\n",
            progname);
    goto quit;
  }
  // Output::setLevels(levels, spec_level);

  // retrieve the type of satellite from configuration
  if(!Conf::getValue(Conf::section_map[COMMON_SECTION],
                   SATELLITE_TYPE,
                     satellite_type))
  {
    DFLTLOG(LEVEL_CRITICAL,
            "section '%s': missing parameter '%s'\n",
            COMMON_SECTION, SATELLITE_TYPE);
    goto quit;
  }
  DFLTLOG(LEVEL_NOTICE,
          "Satellite type = %s\n", satellite_type.c_str());

  // load the plugins
  if(!Plugin::loadPlugins(true, plugin_conf_path))
  {
    DFLTLOG(LEVEL_CRITICAL,
            "%s: cannot load the plugins\n", progname);
    goto quit;
  }

  block_encap = NULL;
  // instantiate all blocs
  if(strToSatType(satellite_type) == REGENERATIVE)
  {
    block_encap = Rt::createBlock<BlockEncapSat,
                                  BlockEncapSat::Upward,
                                  BlockEncapSat::Downward>("Encap");
    if(!block_encap)
    {
      DFLTLOG(LEVEL_CRITICAL,
              "%s: cannot create the Encap block\n", progname);
      goto release_plugins;
    }
  }

  if(strToSatType(satellite_type) == REGENERATIVE)
  {
    block_dvb = Rt::createBlock<BlockDvbSatRegen,
                                BlockDvbSatRegen::UpwardRegen,
                                BlockDvbSatRegen::DownwardRegen>("Dvb", block_encap);
  }
  else
  {
    block_dvb = Rt::createBlock<BlockDvbSatTransp,
                                BlockDvbSatTransp::UpwardTransp,
                                BlockDvbSatTransp::DownwardTransp>("Dvb", block_encap);
  }

  if(!block_dvb)
  {
    DFLTLOG(LEVEL_CRITICAL,
            "%s: cannot create the DvbSat block\n", progname);
    goto release_plugins;
  }

  up_sat_carrier = block_dvb;
  if(strToSatType(satellite_type) == REGENERATIVE)
  {
    block_phy_layer = Rt::createBlock<BlockPhysicalLayerSat,
                                      BlockPhysicalLayerSat::Upward,
                                      BlockPhysicalLayerSat::Downward>("PhysicalLayer",
                                                                       block_dvb);
    if(block_phy_layer == NULL)
    {
      DFLTLOG(LEVEL_CRITICAL,
              "%s: cannot create the PhysicalLayer block\n", progname);
      goto release_plugins;
    }
    up_sat_carrier = block_phy_layer;
  }

  specific.ip_addr = ip_addr;
  block_sat_carrier = Rt::createBlock<BlockSatCarrier,
                                      BlockSatCarrier::Upward,
                                      BlockSatCarrier::Downward,
                                      struct sc_specific>("SatCarrier",
                                                          up_sat_carrier,
                                                          specific);
  if(!block_sat_carrier)
  {
    DFLTLOG(LEVEL_CRITICAL,
            "%s: cannot create the SatCarrier block\n", progname);
    goto release_plugins;
  }

  DFLTLOG(LEVEL_DEBUG,
          "All blocks are created, start\n");

  // make the SAT alive
  if(!Rt::init())
  {
    goto release_plugins;
  }

  Output::Get()->finalizeConfiguration();

  status->sendEvent("Blocks initialized");
  if(!Rt::run())
  {
    DFLTLOG(LEVEL_CRITICAL,
            "%s: cannot run process loop\n",
            progname);
  }

  status->sendEvent("Simulation stopped");

  // everything went fine, so report success
  is_failure = 0;

  // cleanup when SAT stops
release_plugins:
  Plugin::releasePlugins();
quit:
  DFLTLOG(LEVEL_NOTICE,
          "%s: SAT process stopped with exit code %d\n",
          progname, is_failure);
  return is_failure;
}

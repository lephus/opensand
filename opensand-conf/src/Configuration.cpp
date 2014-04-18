/*
 *
 * OpenSAND is an emulation testbed aiming to represent in a cost effective way a
 * satellite telecommunication system for research and engineering activities.
 *
 *
 * Copyright © 2014 TAS
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
 * @file Configuration.cpp
 * @brief Global interface for configuration file reading
 * @author Viveris Technologies
 */


#include "Configuration.h"
#include "ConfigurationFile.h"


ConfigurationFile Conf::global_config;

Conf::Conf()
{
}

Conf::~Conf()
{
	global_config.unloadConfig();
}

bool Conf::loadConfig(const string conf_file)
{
	return global_config.loadConfig(conf_file);
}

bool Conf::loadConfig(const vector<string> conf_files)
{
	return global_config.loadConfig(conf_files);
}

bool Conf::getComponent(string &compo)
{
	return global_config.getComponent(compo);
}


bool Conf::getNbListItems(const char *section,
                          const char *key,
                          int &nbr)
{
	return global_config.getNbListItems(section, key, nbr);
}

bool Conf::getNbListItems(const char *section,
                          const char *key,
                          unsigned int &nbr)
{
	return global_config.getNbListItems(section, key, (int &)nbr);
}



bool Conf::getListItems(const char *section,
                        const char *key,
                        ConfigurationList &list)
{
	return global_config.getListItems(section, key, list);
}

bool Conf::loadLevels(map<string, log_level_t> &levels)
{
	return global_config.loadLevels(levels);
}


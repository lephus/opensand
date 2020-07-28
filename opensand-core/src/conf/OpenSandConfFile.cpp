/*
 *
 * OpenSAND is an emulation testbed aiming to represent in a cost effective way a
 * satellite telecommunication system for research and engineering activities.
 *
 *
 * Copyright © 2020 TAS
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
 * @file OpenSandConfFile.cpp
 * @brief Reading parameters from a configuration file
 * @author Viveris Technologies
 */


#include "OpenSandConfFile.h"

#include <opensand_conf/conf.h>
#include <opensand_output/Output.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>



OpenSandConfFile::OpenSandConfFile() :
	scpc_encap_stacks()
{
	this->scpc_encap_stacks["DVB-RCS"] = vector<string>();
	this->scpc_encap_stacks["DVB-RCS"].push_back(string("GSE"));

	this->scpc_encap_stacks["DVB-RCS2"] = vector<string>();
	//this->scpc_encap_stacks["DVB-RCS2"].push_back(string("RLE"));
	this->scpc_encap_stacks["DVB-RCS2"].push_back(string("GSE"));
}

OpenSandConfFile::~OpenSandConfFile()
{
}

void OpenSandConfFile::loadCarrierMap(map<unsigned int, uint16_t> &carrier_map)
{
	ConfigurationList section_sat_car;
	ConfigurationList spots;
	ConfigurationList::iterator iter_spots;

	section_sat_car = Conf::section_map[SATCAR_SECTION];

	if(!Conf::getListNode(section_sat_car, SPOT_LIST, spots))
	{
		return;
	}

	for(iter_spots = spots.begin() ; iter_spots != spots.end() ; ++iter_spots)
	{
		ConfigurationList current_spot;
		ConfigurationList carrier_list;
		ConfigurationList::iterator iter_carrier;
		xmlpp::Node* spot_node = *iter_spots;
		// TODO avoid using xmlpp::Node
		current_spot.push_front(spot_node);
		uint16_t gw_id = 0;

		// get current gw id
		if(!Conf::getAttributeValue(iter_spots, GW, gw_id))
		{
			return;
		}

	 	// get spot channel
		if(!Conf::getListItems(*iter_spots, CARRIER_LIST, carrier_list))
		{
			return;
		}

		// associate channel to spot
		for(iter_carrier = carrier_list.begin() ; iter_carrier != carrier_list.end() ;
		    ++iter_carrier)
		{
			int carrier_id = 0;

			//get carrier ID
			if(!Conf::getAttributeValue(iter_carrier, CARRIER_ID, carrier_id))
			{
				return;
			}

			carrier_map[carrier_id] = gw_id;
		}
	}

}

void OpenSandConfFile::loadGwTable(map<uint16_t, uint16_t> &gw_table)
{
	ConfigurationList gw_table_section;
	ConfigurationList gws;
	ConfigurationList::iterator iter_gws;

	gw_table_section = Conf::section_map[GW_TABLE_SECTION];

	if(!Conf::getListNode(gw_table_section, GW_LIST, gws))
	{
		return;
	}

	for(iter_gws = gws.begin() ; iter_gws != gws.end() ; ++iter_gws)
	{
		ConfigurationList current_gw;
		ConfigurationList terminal_list;
		ConfigurationList::iterator iter_terminal;
		xmlpp::Node* gw_node = *iter_gws;
		// TODO avoid using xmlpp::Node
		current_gw.push_front(gw_node);
		uint8_t gw_id = 0;

		// get current spot id
		if(!Conf::getAttributeValue(iter_gws, ID, gw_id))
		{
			return;
		}

		// get spot channel
		if(!Conf::getListItems(current_gw, TERMINAL_LIST, terminal_list))
		{
			return;
		}

		// associate channel to spot
		for(iter_terminal = terminal_list.begin() ; iter_terminal != terminal_list.end() ;
		    ++iter_terminal)
		{
			uint16_t tal_id = 0;

			//get carrier ID
			if(!Conf::getAttributeValue(iter_terminal, ID, tal_id))
			{
				return;
			}
			gw_table[tal_id] = gw_id;
		}
	}
}

bool OpenSandConfFile::getGwWithTalId(map<uint16_t, uint16_t> terminal_map,
		                      uint16_t tal_id,
                                      uint16_t &gw_id)
{
	map<uint16_t, uint16_t>::iterator tal_iter;
	tal_iter = terminal_map.find(tal_id);
	if(tal_iter == terminal_map.end())
	{
		return false;
	}
	gw_id = (*tal_iter).second;
	return true;
}

bool OpenSandConfFile::getGwWithCarrierId(map<unsigned int, uint16_t> carrier_map,
                                            unsigned int car_id,
                                            uint16_t &gw)
{
	map<unsigned int, uint16_t>::iterator car_iter;
	car_iter = carrier_map.find(car_id);
	if(car_iter == carrier_map.end())
	{
		return false;
	}

	gw = car_iter->second;
	return true;
}

bool OpenSandConfFile::isGw(map<uint16_t, uint16_t> &gw_table, uint16_t gw_id)
{
	map<uint16_t, uint16_t>::const_iterator it;

	for( it = gw_table.begin(); it != gw_table.end(); ++it)
	{
		if(it->second == gw_id)
		{
			return true;
		}
	}
	return false;
}

bool OpenSandConfFile::getSpot(string section,
                               uint16_t gw_id,
                               ConfigurationList &current_gw)
{
	ConfigurationList spot_list;

	if(!Conf::getListNode(Conf::section_map[section],
	                      SPOT_LIST, spot_list))
	{
		return false;;
	}

	if(!Conf::getElementWithAttributeValue(spot_list, GW,
	                                       gw_id, current_gw))
	{
		return false;
	}

	return true;
}

bool OpenSandConfFile::getScpcEncapStack(string return_link_std,
                                        vector<string> &encap_stack)
{
	map< string, vector<string> >::iterator ite;

	// Check this return link standard is valid
	ite = this->scpc_encap_stacks.find(return_link_std);
	if (ite == this->scpc_encap_stacks.end())
	{
		return false;
	}

	// Return the encapsulation stack
	encap_stack = this->scpc_encap_stacks[return_link_std];
	return true;
}

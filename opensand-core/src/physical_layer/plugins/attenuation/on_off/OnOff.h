/*
 *
 * OpenSAND is an emulation testbed aiming to represent in a cost effective way a
 * satellite telecommunication system for research and engineering activities.
 *
 *
 * Copyright © 2014 TAS
 * Copyright © 2014 CNES
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
 * @file OnOff.h
 * @brief OnOff
 * @author Fatima LAHMOUAD <fatima.lahmouad@etu.enseeiht.fr>
 */

#ifndef OnOff_H
#define OnOff_H


#include "PhysicalLayerPlugin.h"
#include <opensand_conf/ConfigurationFile.h>

#include <string>

using std::string;

/**
 * @class OnOff
 * @brief OnOff
 */
class OnOff: public AttenuationModelPlugin
{
	private:

		int duration_counter;
		int on_duration;
		int off_duration;
		double amplitude;
		map<string, ConfigurationList> config_section_map;

	public:

		/**
		 * @brief Build a OnOff
		 */
		OnOff();

		/**
		 * @brief Destroy the OnOff
		 */
		~OnOff();

		bool init(time_ms_t refresh_period_ms, string link);

		bool updateAttenuationModel();
};

CREATE(OnOff, attenuation_plugin, "On/Off");

#endif

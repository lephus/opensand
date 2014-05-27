/*
 *
 * OpenSAND is an emulation testbed aiming to represent in a cost effective way a
 * satellite telecommunication system for research and engineering activities.
 *
 *
 * Copyright © 2013 TAS
 * Copyright © 2013 CNES
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
 * @file SlottedAlohaBackoffEied.cpp
 * @brief The EIED backoff algorithm
 * @author Vincent WINKEL <vincent.winkel@thalesaleniaspace.com> <winkel@live.fr>
*/

#include "SlottedAlohaBackoffEied.h"

#include <math.h>
#include <stdlib.h>

SlottedAlohaBackoffEied::SlottedAlohaBackoffEied(uint16_t max, uint16_t multiple):
	SlottedAlohaBackoff(max, multiple)
{
	this->setOk();
}

SlottedAlohaBackoffEied::~SlottedAlohaBackoffEied()
{
}

void SlottedAlohaBackoffEied::setOk()
{
	this->cw = fmin((int)this->cw * (int)sqrt(this->multiple), this->cw_max);
	this->setRandom();
}

void SlottedAlohaBackoffEied::setNok()
{
	this->cw = fmin((int)this->cw * (int)this->multiple, this->cw_max);
	this->setRandom();
}


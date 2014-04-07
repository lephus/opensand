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
 * @file OutputLog.h
 * @brief The OutputLog class represent a log
 *        (debug, info, notice, warning, error, critical)
 *        generated by the application.
 *        Its level can be adjusted according to what we want to display
 *        except for events
 * @author Fabrice Hobaya <fhobaya@toulouse.viveris.com>
 */


#ifndef _OUTPUT_LOG_H
#define _OUTPUT_LOG_H

#include "OutputMutex.h"

#include <string>
#include <stdint.h>

using std::string;

/**
 * @brief log severity levels
 **/
enum log_level_t
{
	// Sorted from the least important to the most important
	// with the same id as in syslog except for event
	LEVEL_DEBUG     = 7, /*!< Debug level */
	LEVEL_INFO      = 6, /*!< Information level */
	LEVEL_NOTICE    = 5, /*!< Notice level */
	LEVEL_WARNING   = 4, /*!< Warning level */
	LEVEL_ERROR     = 3, /*!< Error level */
	LEVEL_CRITICAL  = 2, /*!< Critical level */
	LEVEL_EVENT     = 10,  /*!< Event level */
};


/**
 * @class Represent a log
 */
class OutputLog
{
	friend class OutputInternal;

 public:
	/**
	 * @brief Set the current log display level
	 *
	 * @param level  the current log display level
	 */
	virtual void setDisplayLevel(log_level_t level);

	/**
	 * @brief Get the current log diaplay level
	 *
	 * @return the current log display level
	 */
	log_level_t getDisplayLevel(void) const;


 protected:
	/**
	 * @brief create a log
	 *
	 * @param id             The log unique id
	 * @param display_level  The current log level
	 * @param name           The log name
	 */
	OutputLog(uint8_t id,
	          log_level_t display_level,
	          const string &name);

	~OutputLog();

	/**
	 * @brief Get the name of the log
	 *
	 * @return the name of the log
	 **/
	inline const string getName() const
	{
		return this->name;
	};

	/// The levels string representation
	const static char *levels[];

	/// The levels colors for terminal
	const static int colors[];

private:
	/// the event ID
	uint8_t id;
	/// the event name
	string name;
	/// the level 
	log_level_t display_level;
	/// The mutex on log
	mutable OutputMutex mutex;
};

#endif

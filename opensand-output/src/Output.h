/*
 *
 * OpenSAND is an emulation testbed aiming to represent in a cost effective way a
 * satellite telecommunication system for research and engineering activities.
 *
 *
 * Copyright © 2013 TAS
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
 * @file Output.h
 * @brief Definition of the Output static class, used by the application to
 *        interact with the output library.
 * @author Vincent Duvert <vduvert@toulouse.viveris.com>
 */


#ifndef _OUTPUT_H
#define _OUTPUT_H


#include "Probe.h"
#include "Event.h"
#include "OutputInternal.h"

#include <vector>
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>

#define PRINTFLIKE(fmt_pos, vararg_pos) __attribute__((format(printf,fmt_pos,vararg_pos)))

class Output
{
	friend class CommandThread;
	friend uint8_t receiveMessage(int, char*, size_t);

public:

	/**
	 * @brief Initialize the output library
	 *        Prepares the library for registering probes and events
	 *
	 * @param enabled      Set to false to disable the output library
	 * @param min_level    The minimum event level which will be reported
	 * @param sock_prefix  Custom socket path prefix (for testing purposes)
	 */
	static void init(bool enabled, event_level_t min_level,
	                 const char *sock_prefix = NULL);

	/**
	 * @brief Register a probe in the output library
	 *
	 * @param name          The probe name
	 * @param enabled       Whether the probe is enabled by default
	 * @param sample_type_t   The sample type
	 *
	 * @return the probe object
	 **/
	template<typename T>
	static Probe<T> *registerProbe(const std::string &name,
	                               bool enabled,
	                               sample_type_t type);

	/**
	 * @brief Register a probe in the output library
	 *
	 * @param name     The probe name
	 * @param unit     The probe unit
	 * @param enabled  Whether the probe is enabled by default
	 * @param type     The sample type
	 *
	 * @return the probe object
	 **/
	template<typename T>
	static Probe<T> *registerProbe(const std::string &name,
	                               const std::string &unit,
	                               bool enabled, sample_type_t type);

	/**
	 * @brief Register a probe in the output library
	 *        with variable arguments in name
	 *
	 * @param enabled   Whether the probe is enabled by default
	 * @param type      The sample type
	 * @param name      The probe name with variable arguments
	 *
	 * @return the probe object
	 **/
	template<typename T>
	static Probe<T> *registerProbe(bool enabled,
	                               sample_type_t type,
	                               const char *msg_format, ...);

	/**
	 * @brief Register a probe in the output library
	 *        with variable arguments in name
	 *
	 * @param unit     The probe unit
	 * @param enabled  Whether the probe is enabled by default
	 * @param type     The sample type
	 * @param name     The probe name with variable arguments
	 *
	 * @return the probe object
	 **/
	template<typename T>
	static Probe<T> *registerProbe(const std::string &unit,
	                               bool enabled, sample_type_t type,
	                               const char *name, ...);

	/**
	 * @brief Register an event in the output library
	 *
	 * @param identifier   The event name
	 * @param level        The event severity
	 *
	 * @return the event object
	 **/
	static Event *registerEvent(const std::string &identifier,
	                            event_level_t level);

	/**
	 * @brief Register an event in the output library
	 *        with variable arguments
	 *
	 * @param level        The event severity
	 * @param identifier   The event name with variable arguments
	 *
	 * @return the event object
	 **/
	static Event *registerEvent(event_level_t level,
	                            const char *identifier, ...);

	/**
	 * @brief Finish the output library initialization
	 *       Performs the library registration on the OpenSAND daemon.
	 *
	 * @warning Needs to be called after registering probes and before
	 *          starting using them.
	 **/
	static bool finishInit(void);

	/**
	 * @brief Send all probes which got new values sinces the last call.
	 **/
	static void sendProbes(void);

	/**
	 * @brief Send the specified event with the specified message format.
	 *
	 * @param event       The event
	 * @param msg_format  The message format
	 **/
	static void sendEvent(Event *event, const char *msg_format, ...)
		PRINTFLIKE(2, 3);

private:
	/**
	 * @brief  Get the daemon socket address
	 *
	 * @return the daemon socket address
	 */
	inline static const sockaddr_un *daemonSockAddr()
	{
		return &instance.daemon_sock_addr;
	};

	/**
	 * @brief  Get the output instance socket address
	 *
	 * @return the output instance socket address
	 */
	inline static const sockaddr_un *selfSockAddr()
	{
		return &instance.self_sock_addr;
	};

	Output();
	~Output();

	/**
	 * @brief Set the probe state
	 *
	 * @param probe_id  The probe ID
	 * @param enabled   Whether the probe is enabled or not
	 */
	static void setProbeState(uint8_t probe_id, bool enabled);

	/**
	 * @brief disable all stats
	 */
	static void disable();

	/**
	 * @brief Enable output
	 */
	static void enable();

	/**
	 * @brief Acquire lock on output
	 */
	static void acquireLock();

	/**
	 * @brief Release lock on output
	 */
	static void releaseLock();

	/// The output instance
	static OutputInternal instance;

	/// The mutex on Output
	static pthread_mutex_t mutex;
};

template<typename T>
Probe<T> *Output::registerProbe(const std::string &name, bool enabled,
                                sample_type_t type)
{
	return Output::registerProbe<T>(name, "", enabled, type);
}

template<typename T>
Probe<T> *Output::registerProbe(const std::string &name,
                                const std::string &unit,
                                bool enabled, sample_type_t type)
{
	Probe<T> *probe;

	Output::acquireLock();
	probe = Output::instance.registerProbe<T>(name, unit, enabled, type);
	Output::releaseLock();

	return probe;
}

template<typename T>
Probe<T> *Output::registerProbe(bool enabled,
                                sample_type_t type,
                                const char *name, ...)
{
	char buf[1024];
	va_list args;
	
	va_start(args, name);

	vsnprintf(buf, sizeof(buf), name, args);

	va_end(args);

	return Output::registerProbe<T>(buf, "", enabled, type);
}

template<typename T>
Probe<T> *Output::registerProbe(const std::string &unit,
                                bool enabled,
                                sample_type_t type,
                                const char *name, ...)
{
	Probe<T> *probe;
	char buf[1024];
	va_list args;
	
	va_start(args, name);

	vsnprintf(buf, sizeof(buf), name, args);

	va_end(args);

	return Output::registerProbe<T>(buf, unit, enabled, type);

	return probe;
}

#endif

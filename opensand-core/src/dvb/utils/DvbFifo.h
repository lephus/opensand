/*
 *
 * OpenSAND is an emulation testbed aiming to represent in a cost effective way a
 * satellite telecommunication system for research and engineering activities.
 *
 *
 * Copyright © 2019 TAS
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
 * @file DvbFifo.h
 * @brief FIFO queue containing MAC packets
 * @author Julien Bernard / Viveris Technologies
 */

#ifndef DVD_FIFO_H
#define DVD_FIFO_H

#include "OpenSandCore.h"
#include "FifoElement.h"
#include "Sac.h"

#include <opensand_rt/RtMutex.h>
#include <opensand_output/OutputLog.h>

#include <vector>
#include <map>
#include <sys/times.h>


///> The priority of FIFO that indicates the MAC QoS which is sometimes equivalent
///>  to Diffserv IP QoS

/// DVB fifo statistics context
typedef struct
{
	vol_pkt_t current_pkt_nbr;        ///< current number of elements
	vol_bytes_t current_length_bytes; ///< current length of data in fifo
	vol_pkt_t in_pkt_nbr;             ///< number of elements inserted during period
	vol_pkt_t out_pkt_nbr;            ///< number of elements extracted during period
	vol_bytes_t in_length_bytes;      ///< current length of data inserted during period
	vol_bytes_t out_length_bytes;     ///< current length of data extraction during period
	vol_pkt_t drop_pkt_nbr;           ///< number of elements dropped
	vol_bytes_t drop_bytes;           ///< current length of data dropped
} mac_fifo_stat_context_t;

/// Access type for fifo (mapping between mac_fifo and carrier)
typedef enum
{
	access_acm,
	access_vcm,
} fwd_access_type_t;


/**
 * @class DvbFifo
 * @brief Defines a DVB fifo
 *
 * Manages a DVB fifo, for queuing, statistics, ...
 */
class DvbFifo
{
 public:
	/**
	 * @brief Create the DvbFifo
	 *
	 * @param fifo_priority the fifo priority
	 * @param fifo_name     the name of the fifo queue (NM, EF, ...) or SAT
	 * @param type_name     the CR type name for this fifo if it is for a ST 
	 *                      (return link, the carrier access type name for
	 *                      this fifo if it is for the GW (forward link)
	 * @param max_size_pkt  the fifo maximum size
	 */
	DvbFifo(unsigned int fifo_priority, std::string mac_fifo_name,
	        std::string type_name,
	        vol_pkt_t max_size_pkt);

	/**
	 * @brief Create the Spot DvbFifo
	 *
	 * @param carrier_id    the carrier id for the fifo
	 * @param max_size_pkt  the fifo maximul size
	 * @param fifo_name     the name of the fifo
	 */
	DvbFifo(uint8_t carrier_id,
	        vol_pkt_t max_size_pkt,
	        std::string fifo_name);

	virtual ~DvbFifo();

	/**
	 * @brief Get the fifo_name of the fifo
	 *
	 * @return the fifo_name of the fifo
	 */
	std::string getName() const;

	/**
	 * @brief Get the access type associated to the fifo
	 *
	 * return the access type associated to the fifo
	 */
	int getAccessType() const;

	/**
	 * @brief Get the VCM id
	 *
	 * @return the VCM id
	 */
	unsigned int getVcmId() const;

	/**
	 * @brief Get the priority of the fifo (value from ST FIFO configuration)
	 *
	 * @return the priority of the fifo
	 */
	unsigned int getPriority() const;

	/** 
	* @brief Get the carrier_id of the fifo (for SAT and GW configuration)
	*
	* @return the carrier_id of the fifo
	*/
	uint8_t getCarrierId() const;

	/**
	 * @brief Get the fifo current size
	 *
	 * @return the queue current size
	 */
	vol_pkt_t getCurrentSize() const;

	/**
	 * @brief Get the length of data in the fifo (in kbits)
	 *
	 * @return the size of data in the fifo (in kbits)
	 */
	vol_bytes_t getCurrentDataLength() const;

	/**
	 * @brief Get the fifo maximum size
	 *
	 * @return the queue maximum size
	 */
	vol_pkt_t getMaxSize() const;

	/**
	 * @brief Get the number of packets that fed the queue since
	 *        last reset
	 *
	 * @return the number of packets that fed the queue since last call
	 */
	vol_pkt_t getNewSize() const;

	/**
	 * @brief Get the length of data in the fifo (in kbits)
	 *
	 * @return the size of data in the fifo (in kbits)
	 */
	vol_bytes_t getNewDataLength() const;

	/**
	 * @brief Get the head element tick out
	 *
	 * @return the head element tick out
	 */
	clock_t getTickOut() const;

	/**
	 * @brief Reset filled, only if the FIFO has the requested CR type
	 *
	 * @param access_type is the CR type for which reset must be done
	 */
	void resetNew(const ret_access_type_t access_type);

	/**
	 * @brief Add an element at the end of the list
	 *        (Increments new_size_pkt)
	 *
	 * @param elem is the pointer on FifoElement
	 * @return true on success, false otherwise
	 */
	bool push(FifoElement *elem);

	/**
	 * @brief Add an element at the head of the list
	 *        (Decrements new_length_bytes)
	 * @warning This function should be use only to replace a fragment of
	 *          previously removed data in the fifo
	 *
	 * @param elem is the pointer on FifoElement
	 * @return true on success, false otherwise
	 */
	bool pushFront(FifoElement *elem);

	/**
	 * @brief Add an element at the back of the list
	 *        (Decrements new_length_bytes)
	 *
	 * @param elem is the pointer on FifoElement
	 * @return true on success, false otherwise
	 */
	bool pushBack(FifoElement *elem);

	/**
	 * @brief Remove an element at the head of the list
	 *
	 * @return NULL pointer if extraction failed because fifo is empty
	 *         pointer on extracted FifoElement otherwise
	 */
	FifoElement *pop();

	/**
	 * @brief Flush the dvb fifo and reset counters
	 */
	void flush();

	/**
	 * @brief Returns statistics of the fifo in a context
	 *        and reset counters
	 *
	 * @return statistics context
	 */
	void getStatsCxt(mac_fifo_stat_context_t &stat_info);

	void setCni(uint8_t cni);

	uint8_t getCni(void) const;

	std::vector<FifoElement *> getQueue(void);

 protected:
	/**
	 * @brief Reset the fifo counters
	 */
	void resetStats();

	std::vector<FifoElement *> queue; ///< the FIFO itself

	unsigned int fifo_priority;     ///< the MAC priority of the fifo
	std::string fifo_name;               ///< the MAC fifo name: for ST (EF, AF, BE, ...) or SAT
	int access_type;                ///< the forward or return access type
	unsigned int vcm_id;            ///< the associated VCM id (if VCM access type)
	vol_pkt_t new_size_pkt;         ///< the number of packets that filled the fifo
	                                ///< since previous check
	vol_bytes_t cur_length_bytes;   ///< the size of data that filled the fifo
	vol_bytes_t new_length_bytes;   ///< the size of data that filled the fifo
	                                ///< since previous check
	vol_pkt_t max_size_pkt;         ///< the maximum size for that FIFO
	uint8_t carrier_id;             ///< the carrier id of the fifo (for SAT and GW purposes)
	mac_fifo_stat_context_t stat_context; ///< statistics context used by MAC layer

	mutable RtMutex fifo_mutex; ///< The mutex to protect FIFO from concurrent access

	uint8_t cni;                ///< is Scpc mode add cni as option into gse packet

	// Output log
	std::shared_ptr<OutputLog> log_dvb_fifo;
};


typedef std::map<qos_t, DvbFifo *> fifos_t;


#endif

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
 * @file SlottedAlohaTal.h
 * @brief The Slotted Aloha
 * @author Vincent WINKEL <vincent.winkel@thalesaleniaspace.com> <winkel@live.fr>
*/

#ifndef SALOHA_TAL_H
#define SALOHA_TAL_H

#include "SlottedAloha.h"

#include "SlottedAlohaBackoff.h"
#include "DvbFifo.h"
#include "DvbFrame.h"
#include "NetBurst.h"
#include "SlottedAlohaFrame.h"
#include "TerminalCategorySaloha.h"

#include <list>

/**
 * @class SlottedAlohaTal
 * @brief The Slotted Aloha
*/

class SlottedAlohaTal: public SlottedAloha
{
 private:

	/// The terminal ID
	tal_id_t tal_id;

	/// packet timeout
	uint16_t timeout;

	/// The packets waiting for ACK
	map<qos_t, saloha_packets_t> packets_wait_ack;

	/// list of  packets to be retransmitted
	saloha_packets_t retransmission_packets;

	/// Number of successive transmissions
	uint16_t nb_success;

	/// Maximum number of packets per superframe
	uint16_t nb_max_packets;

	/// Configuration parameter : maximum number of retransmissions before
	/// packet deleting
	uint16_t nb_max_retransmissions;

	/// Current packet identifiant
	uint64_t base_id;

	/// Backoff algorithm used
	SlottedAlohaBackoff *backoff;

	/// The terminal category
	TerminalCategorySaloha *category;

 public:

	/**
	 * Class constructor whithout any parameters
	 */
	SlottedAlohaTal();

	/**
	 * Class destructor
	 */
	~SlottedAlohaTal();

	/**
	 * @brief Initialize Slotted Aloha for terminal
	 *
	 * @param tal_id                  The terminal ID
	 * @param frames_per_superframe   The number of frames per superframes
	 * @return true on success, false otherwise
	 */
	bool init(tal_id_t tal_id,
	          unsigned int frames_per_superframe);

	/**
	 * Called when a packet is received from encap block
	 *
	 * @param encap_packet  encap packet received
	 * @param offset        offset of packet about initial packet
	 * @param burst_size    number of packets
	 *
	 * @return Slotted Aloha packet
	 */
	SlottedAlohaPacketData* onRcvEncapPacket(NetPacket *encap_packet,
	                                         uint16_t offset,
	                                         uint16_t burst_size);

	/**
	 * Schedule Slotted Aloha packets
	 *
	 * @param dvb_fifos            FIFO containing encap packets received
	 *                             propagate to encap block
	 * @param complete_dvb_frames  frames to attach Slotted Aloha frame to send
	 * @param counter              current SoF counter
	 *
	 * @return true if packets was successful scheduled, false otherwise
	 */
	bool schedule(fifos_t &dvb_fifos,
	              list<DvbFrame *> &complete_dvb_frames,
	              uint64_t counter); // uint32 ?

	//Implementation of a virtual function
	bool onRcvFrame(DvbFrame *frame);

 private:

	/**
	 * generate random unique time slots for packets to send
	 *
	 * @param dvb_fifos  FIFO containing encap packets received, used to know
	 *                   its size and calculate the number of packets
	 * @return set containing random unique time slots
	 */
	saloha_ts_list_t getTimeSlots(fifos_t &dvb_fifos);

	/**
	 * Add a Slotted Aloha data packet and its replicas into Slotted Aloha frames
	 *
	 * @param complete_dvb_frames  frames to attach Slotted Aloha frame to send
	 * @param frame                current Slotted Aloha frame to fill
	 * @param sa_packet            Slotted Aloha packet to add into the frame
	 * @param slot                 slots to set for replicas
	 * @param qos                  qos of the packet
	 * @return true if the packet was successful added, false otherwise
	 */
	bool sendPacketData(list<DvbFrame *> &complete_dvb_frames,
	                    SlottedAlohaFrame **frame,
	                    SlottedAlohaPacketData *packet,
	                    saloha_ts_list_t::iterator &slot,
	                    qos_t qos);


	//Implementation of virtual debug functions
	/// TODO REMOVE
	void debugFifo(const char* title);
};

#endif


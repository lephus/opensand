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
 * @file sat_carrier_udp_channel.cpp
 * @brief This implements an UDP satellite carrier channel
 * @author Didier Barvaux <didier.barvaux@toulouse.viveris.com>
 */

// FIXME we need to include uti_debug.h before...
#define DBG_PACKAGE PKG_SAT_CARRIER
#include <opensand_conf/uti_debug.h>

#include "sat_carrier_udp_channel.h"

#include <cstring>
#include <stdlib.h>
#include <strings.h>
#include <arpa/inet.h>
#include <errno.h>


/**
 * Constructor
 *
 * @param channelID           the Id of the new channel
 * @param input               true if the channel accept incoming data
 * @param output              true if channel send data
 * @param is_data             true is this is an intput data channel
 * @param local_interface_name.c_str()  the name of the local network interface to use
 * @param port                the port on which the channel is bind
 * @param multicast           true is this is a multicast channel
 * @param local_ip_addr       the host IP address
 * @param ip_addr             the IP address of the remote host in case of
 *                            output channel or the multicast IP address in
 *                            case of input multicast channel
 *
 * @see sat_carrier_channel::sat_carrier_channel()
 */
sat_carrier_udp_channel::sat_carrier_udp_channel(unsigned int channelID,
                                                 bool input,
                                                 bool output,
                                                 bool is_data,
                                                 const string local_interface_name,
                                                 unsigned short port,
                                                 bool multicast,
                                                 const string local_ip_addr,
                                                 const string ip_addr):
	sat_carrier_channel(channelID, input, output, is_data),
	sock_channel(-1),
	m_multicast(multicast),
	stacked_ip("")
{
	int ifIndex;
	struct ip_mreq imr;
	unsigned char ttl = 1;
	int one = 1;
	int buffer = 0;
	socklen_t size = 4;

	this->max_counter = (2 << ((COUNTER_SIZE * 8) - 1));

	// sanity check
	if(COUNTER_SIZE > 2)
	{
		UTI_ERROR("The counter size should be 1 or 2\n");
		goto error;
	}
	if(MAX_DATA_STACK > this->max_counter)
	{
		UTI_ERROR("The maximum stack size is too high regarding "
		          "the counter size (%u > %u)\n",
		          MAX_DATA_STACK, this->max_counter);
		goto error;
	}

	bzero(&this->m_socketAddr, sizeof(this->m_socketAddr));
	m_socketAddr.sin_family = AF_INET;
	m_socketAddr.sin_port = htons(port);

	bzero(&m_remoteIPAddress, sizeof(m_remoteIPAddress));

	// open the socket
	this->sock_channel = socket(AF_INET, SOCK_DGRAM, 0);
	if(this->sock_channel < 0)
	{
		UTI_ERROR("Can't open the receive socket, errno %d (%s)\n",
		          errno, strerror(errno));
		goto error;
	}

	if(setsockopt(this->sock_channel, SOL_SOCKET, SO_REUSEADDR,
	              (char *)&one, sizeof(one))<0)
	{
		UTI_ERROR("Error in reusing addr\n");
		goto error;
	}

	// get the index of the network interface
	ifIndex = this->getIfIndex(local_interface_name.c_str());

	if(ifIndex < 0)
	{
		UTI_ERROR("cannot get the index for %s\n", local_interface_name.c_str());
		goto error;
	}

	// Set destination characteristics to send the datagramms
	if(this->isOutputOk())
	{
		this->counter = 0;
		// get the remote IP address
		if(inet_aton(ip_addr.c_str(), &(m_remoteIPAddress.sin_addr))<0)
		{
			UTI_ERROR("cannot get the remote IP address for %s \n",
			          ip_addr.c_str());
			goto error;
		}
		m_remoteIPAddress.sin_family = AF_INET;
		m_remoteIPAddress.sin_port = htons(port);
		m_socketAddr.sin_addr.s_addr = inet_addr(local_ip_addr.c_str());

		// creation of the link between the socket and its port
		if(bind(this->sock_channel, (struct sockaddr *) &this->m_socketAddr,
		         sizeof(this->m_socketAddr)) < 0)
		{
			UTI_ERROR("failed to bind to UDP socket: %s (%d)\n",
			          strerror(errno), errno);
			goto error;
		}

		if(this->m_multicast)
		{
			if(setsockopt(sock_channel, IPPROTO_IP, IP_MULTICAST_TTL, &ttl,
			              sizeof(ttl)) < 0)
			{
				UTI_ERROR("setsockopt: IP_MULTICAST_TTL activation failed\n");
				goto error;
			}
		}
	}
	else if(this->isInputOk())
	{
		// check if the socket buffer is enough
		if(getsockopt(this->sock_channel, SOL_SOCKET, SO_RCVBUF,
		              (char *)&buffer, &size) < 0)
		{
			UTI_ERROR("getsockopt : SO_RCVBUF failed\n");
			goto error;
		}
		UTI_INFO("size of socket buffer: %d \n", buffer);

		if(this->m_multicast)
		{
			if(inet_aton(ip_addr.c_str(), &this->m_socketAddr.sin_addr) < 0)
			{
				UTI_ERROR("error with inet_aton: %s", strerror(errno));
				goto error;
			}

			// creation of the link between the socket and its port
			if(bind(this->sock_channel, (struct sockaddr *) &this->m_socketAddr,
			        sizeof(this->m_socketAddr)) < 0)
			{
				UTI_ERROR("failed to bind to multicast UDP socket: %s (%d)\n",
				          strerror(errno), errno);
				goto error;
			}

			memset(&imr, 0, sizeof(struct ip_mreq));
			imr.imr_multiaddr.s_addr = inet_addr(ip_addr.c_str());
			imr.imr_interface.s_addr = inet_addr(local_ip_addr.c_str());

			if(setsockopt(this->sock_channel, IPPROTO_IP, IP_ADD_MEMBERSHIP,
			              (void *) &imr, sizeof(struct ip_mreq)) < 0)
			{
				UTI_ERROR("failed to join multicast group with multicast address "
				          "%s and interface address %s: %s (%d)\n",
				          ip_addr.c_str(), local_ip_addr.c_str(), strerror(errno), errno);
				goto error;
			}
		}
		else
		{
			m_socketAddr.sin_addr.s_addr = inet_addr(local_ip_addr.c_str());
			// creation of the link between the socket and its port
			if(bind(this->sock_channel, (struct sockaddr *) &this->m_socketAddr,
			        sizeof(this->m_socketAddr)) < 0)
			{
				UTI_ERROR("failed to bind unicast UDP socket: %s (%d)\n",
				          strerror(errno), errno);
				goto error;
			}
		}
	}
	else
	{
		UTI_ERROR("channel doesn't receive and doesn't send data\n");
		goto error;
	}
	bzero(this->send_buffer, sizeof(this->send_buffer));

	UTI_INFO("UDP channel %u created with local IP %s and local port %u\n",
	         getChannelID(), inet_ntoa(m_socketAddr.sin_addr),
			 ntohs(m_socketAddr.sin_port));

	this->init_success = true;
	return;

error:
	UTI_ERROR("Can't create channel\n");
}

/**
 * Destructor
 */
sat_carrier_udp_channel::~sat_carrier_udp_channel()
{
	close(this->sock_channel);
	this->udp_counters.clear();
	for(map<string, UdpStack *>::iterator it = this->stacks.begin();
	    it != this->stacks.end(); ++it)
	{
		delete (*it).second;
	}
	this->stacks.clear();
}

/**
 * Return the network socket of the udp channel
 * @return the network socket of the udp channel
 */
int sat_carrier_udp_channel::getChannelFd()
{
	return this->sock_channel;
}

/**
 * @brief Get the message in NetSocketEvent
 *
 * @param event    The NetSocketEvent on fd
 * @param buf      pointer to a char buffer
 * @param data_len length of the received data
 * @return         0 on success, 1 if the function should be
 *                 called another time, -1 on error
 */
// TODO why not work directly with Data here instead of buf, length
int sat_carrier_udp_channel::receive(NetSocketEvent *const event,
                                     unsigned char **buf, size_t &data_len)
{
	struct sockaddr_in remote_addr;
	map<string , uint16_t>::iterator ip_count_it;
	std::string ip_address;
	uint16_t nb_sequencing;
	uint16_t current_sequencing;
	unsigned char *data;
	size_t recv_len;
	unsigned char *recv_data;

	if(!this->is_data)
	{
		return this->receiveSig(event, buf, data_len);
	}

	if(!this->stacked_ip.empty())
	{
		UTI_DEBUG("Send content of stack for address %s\n",
		          this->stacked_ip.c_str());
		if(!this->handleStack(buf, data_len))
		{
			goto error;
		}
		if(!this->stacked_ip.empty())
		{
			// we still have packets to send
			goto stacked;
		}
		goto end;
	}

	UTI_DEBUG("try to receive a packet from satellite channel %d\n",
	          this->getChannelID());

	// the channel file descriptor must be valid
	if(this->getChannelFd() < 0)
	{
		UTI_ERROR("socket not opened !\n");
		goto error;
	}

	// error if channel doesn't accept incoming data
	if(!this->isInputOk())
	{
		UTI_ERROR("channel %d does not accept data\n",
		          this->getChannelID());
		goto error;
	}

	// we need to memcpy as the start pointer cannot be reused
	recv_len = event->getSize() - COUNTER_SIZE;
	recv_data = (unsigned char *)calloc(recv_len, sizeof(unsigned char));
	data = event->getData();
	memcpy(recv_data, data + COUNTER_SIZE, recv_len);
	remote_addr = event->getSrcAddr();

	// get the IP address of the sender
	ip_address = inet_ntoa(remote_addr.sin_addr);

	// check the sequencing of the datagramm
	if(COUNTER_SIZE == 2)
	{
		nb_sequencing = (data[0] & 0xff) << 8 | (data[1] & 0xff);
		// TODO do we need that for endianess ?
		//nb_sequencing = ntohs(nb_sequencing);
	}
	else
	{
		nb_sequencing = data[0] & 0xff;
	}
	free(data);
	ip_count_it = this->udp_counters.find(ip_address);
	if(ip_count_it == this->udp_counters.end())
	{
		this->udp_counters[ip_address] = nb_sequencing;
		if(nb_sequencing != 0)
		{
			UTI_NOTICE("force synchronisation on UDP channel %d "
			           "from %s at startup: received counter is %d "
			           "while it should have been 0\n",
			           this->getChannelID(), ip_address.c_str(),
			           nb_sequencing);
		}
		this->stacks[ip_address] = new UdpStack();
		current_sequencing = nb_sequencing;
	}
	else
	{
		current_sequencing = (ip_count_it->second + 1) % this->max_counter;
		UTI_DEBUG_L3("Current UDP sequencing for address %s: %u\n",
		             ip_address.c_str(), current_sequencing);
	}
	// add the new packet in stack
	this->stacks[ip_address]->add(nb_sequencing, recv_data, recv_len);
	// send the current packet
	if(this->stacks[ip_address]->hasNext(current_sequencing))
	{
		UTI_DEBUG_L3("Next UDP packet is in stack\n");
		this->handleStack(buf, data_len, current_sequencing, this->stacks[ip_address]);
		if(!this->stacked_ip.empty())
		{
			// we still have packets to send
			goto stacked;
		}
		ip_count_it->second = current_sequencing;
	}
	else
	{
		UTI_DEBUG("No UDP packet for current sequencing (%u) at IP %s "
		          "wait for next packets (last received %u)\n",
		          current_sequencing, ip_address.c_str(), nb_sequencing);
	}

	// check that we do not have too much packets in stack
	if(this->stacks[ip_address]->getCounter() > MAX_DATA_STACK)
	{
		// suppose we lost the packet
		UTI_ERROR("we may have lost UDP packets, check /etc/default/opensand-daemon "
		          "and adjust UDP buffers");
		// send the next packets from stack
		current_sequencing = (current_sequencing + 1) % this->max_counter;
		while(!this->stacks[ip_address]->hasNext(current_sequencing))
		{
			UTI_DEBUG("packet missing: %u\n", current_sequencing);
			current_sequencing = (current_sequencing + 1) % this->max_counter;
		}
		// we should be able to return a packet here
		ip_count_it->second = current_sequencing;
		this->stacked_ip = ip_address;
		goto stacked;
	}

end:
	return 0;

stacked:
	return 1;

error:
	return -1;
}


int sat_carrier_udp_channel::receive(unsigned char **buf, size_t &data_len)
{
	if(!this->stacked_ip.empty())
	{
		UTI_DEBUG("Send content of stack for address %s\n",
		          this->stacked_ip.c_str());
		if(!this->handleStack(buf, data_len))
		{
			goto error;
		}
		if(!this->stacked_ip.empty())
		{
			// we still have packets to send
			goto stacked;
		}
		goto end;
	}
end:
	return 0;

stacked:
	return 1;

error:
	return -1;
}


/**
 * @brief Get the signalisation message in NetSocketEvent
 *
 * @param event    The NetSocketEvent on fd
 * @param buf      pointer to a char buffer
 * @param data_len length of the received data
 * @return         0 on success, -1 on error
 */
int sat_carrier_udp_channel::receiveSig(NetSocketEvent *const event,
                                        unsigned char **buf, size_t &data_len)
{
	struct sockaddr_in remote_addr;
	map<string , uint16_t>::iterator ip_count_it;
	std::string ip_address;
	uint16_t nb_sequencing;
	unsigned char *data;

	UTI_DEBUG("try to receive a packet from satellite channel %d\n",
	          this->getChannelID());

	// the channel file descriptor must be valid
	if(this->getChannelFd() < 0)
	{
		UTI_ERROR("socket not opened !\n");
		goto error;
	}

	// error if channel doesn't accept incoming data
	if(!this->isInputOk())
	{
		UTI_ERROR("channel %d does not accept data\n",
		          this->getChannelID());
		goto error;
	}

	// we need to memcpy as the start pointer cannot be reused
	data_len = event->getSize() - COUNTER_SIZE;
	*buf = (unsigned char *)calloc(data_len, sizeof(unsigned char));
	data = event->getData();
	memcpy(*buf, data + COUNTER_SIZE, data_len);
	remote_addr = event->getSrcAddr();

	// get the IP address of the sender
	ip_address = inet_ntoa(remote_addr.sin_addr);

	// check the sequencing of the datagramm
	if(COUNTER_SIZE == 2)
	{
		nb_sequencing = (data[0] & 0xff) << 8 | (data[1] & 0xff);
		// TODO do we need that for endianess ?
		//nb_sequencing = ntohs(nb_sequencing);
	}
	else
	{
		nb_sequencing = data[0] & 0xff;
	}

	// TODO for sig we could remove counters because we do not
	// really check sequencing except for debug
	free(data);
	ip_count_it = this->udp_counters.find(ip_address);
	if(ip_count_it == this->udp_counters.end())
	{
		this->udp_counters[ip_address] = nb_sequencing;
		if(nb_sequencing != 0)
		{
			UTI_NOTICE("force synchronisation on UDP channel %d "
			           "from %s at startup: received counter is %d "
			           "while it should have been 0\n",
			           this->getChannelID(), ip_address.c_str(),
			           nb_sequencing);
		}
	}
	else
	{
		ip_count_it->second = (ip_count_it->second + 1) % this->max_counter;
		if(ip_count_it->second != nb_sequencing)
		{
			UTI_ERROR("Gap between sinalisation message: expected %u, received %u\n",
			          ip_count_it->second, nb_sequencing);
		}
		ip_count_it->second = nb_sequencing;
		UTI_DEBUG_L3("Current UDP sequencing for address %s: %u\n",
		             ip_address.c_str(), ip_count_it->second);
	}

	return 0;

error:
	return -1;
}


bool sat_carrier_udp_channel::handleStack(unsigned char **buf, size_t &data_len)
{
	map<string, uint16_t>::iterator count_it = this->udp_counters.find(this->stacked_ip);
	map<string, UdpStack *>::iterator stack_it = this->stacks.find(this->stacked_ip);
	uint16_t counter;
	
	if(count_it == this->udp_counters.end())
	{
		UTI_ERROR("cannot find UDP counter for IP %s\n", this->stacked_ip.c_str());
		return false;
	}
	if(stack_it == this->stacks.end())
	{
		UTI_ERROR("cannot find UDP stack for IP %s\n", this->stacked_ip.c_str());
		return false;
	}

	counter = (*count_it).second;
	this->handleStack(buf, data_len,
	                  counter,
	                  (*stack_it).second);
	if(!this->stacked_ip.empty())
	{
		// update counter for next stacked packet
		counter = (counter + 1) % this->max_counter;
		(*count_it).second = counter;
	}
	return true;
}
	
	
void sat_carrier_udp_channel::handleStack(unsigned char **buf, size_t &data_len,
                                          uint16_t counter, UdpStack *stack)
{
	UTI_DEBUG("transmit UDP packet for source IP %s at counter %d\n",
	          this->stacked_ip.c_str(), counter);
	stack->remove(counter, buf, data_len);
	counter = (counter + 1) % this->max_counter;
	// if we don't have following packets in FIFO reset stacked_ip
	if(!stack->hasNext(counter))
	{
		this->stacked_ip = "";
	}
}


bool sat_carrier_udp_channel::send(const unsigned char *data, size_t length)
{
	ssize_t data_len;
	ssize_t slen = 0; 

	UTI_DEBUG("data are trying to be send on channel %d\n", m_channelID);

	// check that the channel sends data
	if(!this->isOutputOk())
	{
		UTI_ERROR("Channel %d is not configure to send data\n", m_channelID);
		goto error;
	}

	// check if the socket is open
	if(this->getChannelFd() < 0)
	{
		UTI_ERROR("Socket not open !\n");
		goto error;
	}

	// add a sequencing field
	bzero(this->send_buffer, sizeof(this->send_buffer));
	if(COUNTER_SIZE == 2)
	{
		// TODO endianess ?
		this->send_buffer[0] = (this->counter >> 8) & 0xff;
		this->send_buffer[1] = this->counter & 0xff;
	}
	else
	{
		this->send_buffer[0] = this->counter & 0xff;
	}
	memcpy(this->send_buffer + COUNTER_SIZE, data, length);
	data_len = length + COUNTER_SIZE;

	while(slen < data_len)
	{
		slen = sendto(this->sock_channel, this->send_buffer + slen,
		              data_len - slen, 0,
		              (struct sockaddr *) &this->m_remoteIPAddress,
		              sizeof(this->m_remoteIPAddress));
		if(slen < 0)
		{
			UTI_ERROR("Error:  sendto(..,0,..) errno %s (%d)\n",
			          strerror(errno), errno);
			goto error;
		}
	}

	// update of the counter
	counter = (counter + 1) % this->max_counter;

	UTI_DEBUG("==> SAT_Channel_Send [%d] (%s:%d): len=%zd, counter: %d\n",
	          m_channelID, inet_ntoa(this->m_remoteIPAddress.sin_addr),
	          ntohs(this->m_remoteIPAddress.sin_port), slen, this->counter);

	return true;

 error:
	return false;
}

bool sat_carrier_udp_channel::sofReceived(void)
{
	if(!this->is_data)
	{
		return false;
	}

	for(map<string, UdpStack *>::iterator it = this->stacks.begin();
	    it != this->stacks.end(); ++it)
	{
		UdpStack *stack = (*it).second;
		string ip_address = (*it).first;
		if(stack->onTimer())
		{
			uint16_t current_sequencing;
			map<string , uint16_t>::iterator ip_count_it;
			// suppose we lost the packet
			UTI_INFO("Timer on stack, send next available packet, "
			         "some packets may be lost\n");
			ip_count_it = this->udp_counters.find(ip_address);
			if(ip_count_it == this->udp_counters.end())
			{
				UTI_ERROR("This should not happend !!\n");
				return false;
			}

			current_sequencing = (ip_count_it->second + 1) % this->max_counter;
			while(!this->stacks[ip_address]->hasNext(current_sequencing))
			{
				UTI_DEBUG("packet missing: %u\n", current_sequencing);
				current_sequencing = (current_sequencing + 1) % this->max_counter;
			}
			// we should be able to return a packet next time an event
			// is triggered on this socket
			ip_count_it->second = current_sequencing;
			this->stacked_ip = ip_address;
			// stop on the first stack that timeout
			return true;
		}
	}
	return false;
}

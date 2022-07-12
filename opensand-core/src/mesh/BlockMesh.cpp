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
 * @file BlockMesh.cpp
 * @brief Block that handles mesh or star architecture on satellites
 * @author Yohan Simard <yohan.simard@viveris.fr>
 */

#include "BlockMesh.h"
#include "NetBurst.h"
#include "NetPacket.h"
#include "NetPacketSerializer.h"
#include <iterator>
#include <opensand_rt/MessageEvent.h>


BlockMesh::BlockMesh(const std::string &name, tal_id_t entity_id):
    Block(name), entity_id{entity_id} {}

bool BlockMesh::onInit()
{
	auto conf = OpenSandModelConf::Get();
	auto downward = dynamic_cast<Downward *>(this->downward);
	auto upward = dynamic_cast<Upward *>(this->upward);

	bool mesh_arch = conf->isMeshArchitecture();
	upward->mesh_architecture = mesh_arch;
	downward->mesh_architecture = mesh_arch;
	LOG(log_init, LEVEL_INFO, "Architecture: %s", mesh_arch ? "mesh" : "star");

	if (!conf->getInterSatLinkCarriers(entity_id, downward->isl_in, upward->isl_out))
	{
		return false;
	}

	// auto handled_entities = conf->getEntitiesHandledBySat(entity_id);
	// upward->handled_entities = handled_entities;
	// downward->handled_entities = handled_entities;

	// std::stringstream ss;
	// std::copy(handled_entities.begin(), handled_entities.end(), std::ostream_iterator<tal_id_t>{ss, " "});
	// LOG(log_init, LEVEL_INFO, "Handled entities: %s", ss.str().c_str());

	tal_id_t default_entity;
	if (!conf->getDefaultEntityForSat(entity_id, default_entity))
	{
		return false;
	}
	upward->default_entity = default_entity;
	downward->default_entity = default_entity;
	LOG(log_init, LEVEL_INFO, "Default entity: %d", default_entity);

	return true;
}

/*****************************************************************************/
/*                               Upward                                      */
/*****************************************************************************/

BlockMesh::Upward::Upward(const std::string &name, tal_id_t UNUSED(sat_id)):
    RtUpwardMux(name) {}

bool BlockMesh::Upward::onInit()
{
	// the ISL out port is set to 0 if the default entity is not a sat
	if (isl_out.port > 0)
	{
		// Open the inter-satellite out channel
		std::string local_ip_addr;
		if (!OpenSandModelConf::Get()->getSatInfrastructure(local_ip_addr))
		{
			return false;
		};
		std::string isl_name = getName() + "_isl_out";
		LOG(log_init, LEVEL_INFO, "Creating ISL output channel bound to %s, sending to %s:%d",
		    local_ip_addr.c_str(),
		    isl_out.address.c_str(),
		    isl_out.port);
		isl_out_channel = std::unique_ptr<UdpChannel>{
		    new UdpChannel{isl_name,
		                   0, // unused (spot id)
		                   isl_out.id,
		                   false, // input
		                   true,  // output
		                   isl_out.port,
		                   isl_out.is_multicast,
		                   local_ip_addr,
		                   isl_out.address,
		                   isl_out.udp_stack,
		                   isl_out.udp_rmem,
		                   isl_out.udp_wmem}};
		if (addNetSocketEvent(isl_name, isl_out_channel->getChannelFd()) == -1)
		{
			return false;
		}
	}
	return true;
}

bool BlockMesh::Upward::onEvent(const RtEvent *const event)
{
	if (event->getType() != EventType::Message)
	{
		LOG(log_receive, LEVEL_ERROR, "Unexpected event received: %s",
		    event->getName().c_str());
		return false;
	}

	auto msg_event = static_cast<const MessageEvent *>(event);
	switch (to_enum<InternalMessageType>(msg_event->getMessageType()))
	{
		case InternalMessageType::decap_data:
		{
			auto burst = static_cast<const NetBurst *>(msg_event->getData());
			return handleNetBurst(std::unique_ptr<const NetBurst>{burst});
		}
		case InternalMessageType::sig:
		{
			auto data = msg_event->getData();
			return shareMessage(&data, msg_event->getLength(), msg_event->getMessageType());
		}
		case InternalMessageType::link_up:
			// ignore
			return true;
		default:
			LOG(log_receive, LEVEL_ERROR, "Unexpected message received: %s (%d)",
			    msg_event->getName().c_str(), msg_event->getMessageType());
			return false;
	}
}

bool BlockMesh::Upward::handleNetBurst(std::unique_ptr<const NetBurst> burst)
{
	if (burst->empty())
		return true;

	NetPacket &msg = *burst->front();

	// TODO: check that getDstTalId() returns the final destination
	tal_id_t dest_entity = msg.getDstTalId();

	LOG(log_receive, LEVEL_DEBUG, "Handling a NetBurst from entity %d to entity %d",
	    msg.getSrcTalId(), dest_entity);

	auto conf = OpenSandModelConf::Get();
	Component default_entity_type = conf->getEntityType(default_entity);

	if (mesh_architecture &&
	    handled_entities.find(dest_entity) == handled_entities.end() &&
	    default_entity_type == Component::satellite)
	{
		return sendViaIsl(std::move(burst));
	}
	else
	{
		return sendToOppositeChannel(std::move(burst));
	}
	// unreachable
	return false;
}

bool BlockMesh::Upward::sendToOppositeChannel(std::unique_ptr<const NetBurst> burst)
{
	LOG(log_send, LEVEL_DEBUG, "Sending a NetBurst to the opposite channel");

	auto burst_ptr = burst.release();
	bool ok = shareMessage((void **)&burst_ptr, sizeof(NetBurst), to_underlying(InternalMessageType::decap_data));
	if (!ok)
	{
		LOG(this->log_send, LEVEL_ERROR,
		    "Failed to transmit message to the opposite channel");
		delete burst_ptr;
		return false;
	}
	return true;
}

bool BlockMesh::Upward::sendToOppositeChannel(std::unique_ptr<const DvbFrame> frame)
{
	LOG(log_send, LEVEL_INFO, "Sending a control DVB frame to the opposite channel");

	auto frame_ptr = frame.release();
	bool ok = shareMessage((void **)&frame_ptr, sizeof(DvbFrame), to_underlying(InternalMessageType::sig));
	if (!ok)
	{
		LOG(this->log_send, LEVEL_ERROR,
		    "Failed to transmit a control DVB frame to the opposite channel");
		delete frame_ptr;
		return false;
	}
	return true;
}

bool BlockMesh::Upward::sendViaIsl(std::unique_ptr<const NetBurst> burst)
{
	LOG(log_send, LEVEL_INFO, "Sending a NetBurst via ISL");

	for (auto &&pkt: *burst)
	{
		net_packet_buffer_t buf{*pkt};
		if (!isl_out_channel->send((uint8_t *)&buf, sizeof(net_packet_buffer_t)))
		{
			LOG(this->log_send, LEVEL_ERROR,
			    "Failed to transmit message via ISL");
			return false;
		}
	}
	return true;
}

/*****************************************************************************/
/*                              Downward                                     */
/*****************************************************************************/

BlockMesh::Downward::Downward(const std::string &name, tal_id_t UNUSED(sat_id)):
    RtDownwardDemux<SpotComponentPair>(name) {}

bool BlockMesh::Downward::onInit()
{
	std::string local_ip_addr;
	if (!OpenSandModelConf::Get()->getSatInfrastructure(local_ip_addr))
	{
		return false;
	};
	std::string isl_name = getName() + "_isl_in";
	LOG(log_init, LEVEL_INFO, "Creating ISL input channel listening on %s:%d",
	    local_ip_addr.c_str(),
	    isl_in.port);
	isl_in_channel = std::unique_ptr<UdpChannel>{
	    new UdpChannel{isl_name,
	                   0, // unused (spot id)
	                   isl_in.id,
	                   true,  // input
	                   false, // output
	                   isl_in.port,
	                   isl_in.is_multicast,
	                   local_ip_addr,
	                   isl_in.address, // unused for now (dest IP), but may be used if we switch to multicast for ISL
	                   isl_in.udp_stack,
	                   isl_in.udp_rmem,
	                   isl_in.udp_wmem}};
	if (addNetSocketEvent(isl_name, isl_in_channel->getChannelFd()) == -1)
	{
		return false;
	}
	return true;
}

bool BlockMesh::Downward::onEvent(const RtEvent *const event)
{
	switch (event->getType())
	{
    case EventType::Message:
			return handleMessageEvent(static_cast<const MessageEvent *>(event));
    case EventType::NetSocket:
			return handleNetSocketEvent((NetSocketEvent *)event);
		default:
			LOG(log_receive, LEVEL_ERROR, "Unexpected event received: %s",
			    event->getName().c_str());
			return false;
	}
}

bool BlockMesh::Downward::handleMessageEvent(const MessageEvent *event)
{

	switch (to_enum<InternalMessageType>(event->getMessageType()))
	{
		case InternalMessageType::decap_data:
		{
			LOG(log_receive, LEVEL_DEBUG, "Received a NetBurst MessageEvent");
			auto burst = static_cast<const NetBurst *>(event->getData());
			return handleNetBurst(std::unique_ptr<const NetBurst>(burst));
		}
		case InternalMessageType::sig:
		{
			// LOG(log_receive, LEVEL_DEBUG, "Received a control message");
			auto dvb_frame = static_cast<const DvbFrame *>(event->getData());
			return handleControlMsg(std::unique_ptr<const DvbFrame>{dvb_frame});
		}
		default:
			LOG(log_receive, LEVEL_ERROR, "Unexpected message received: %s",
			    event->getName().c_str());
			return false;
	}
}

bool BlockMesh::Downward::handleNetSocketEvent(NetSocketEvent *event)
{
	LOG(log_receive, LEVEL_DEBUG, "Received a NetSocketEvent");

	std::size_t length;
	net_packet_buffer_t *buf;

	// TODO: remove this
	if (!NetBurst::log_net_burst)
		NetBurst::log_net_burst = Output::Get()->registerLog(LEVEL_WARNING, "NetBurst");

	std::unique_ptr<NetBurst> burst{new NetBurst{}};
	int ret;
	do
	{
		ret = isl_in_channel->receive(event, reinterpret_cast<uint8_t **>(&buf), length);
		if (ret < 0 || length <= 0 || buf == nullptr)
		{
			LOG(this->log_receive, LEVEL_ERROR, "Error while receiving an ISL packet");
			return false;
		}
		burst->add(buf->deserialize());
		delete buf;
	}
	while (ret == 1 && !burst->isFull());

	return handleNetBurst(std::move(burst));
}

bool BlockMesh::Downward::handleNetBurst(std::unique_ptr<const NetBurst> burst)
{
	if (burst->empty())
		return true;

	auto conf = OpenSandModelConf::Get();

	NetPacket &first_pkt = *burst->front();

	LOG(log_receive, LEVEL_DEBUG, "Handling a NetBurst from entity %d to entity %d",
	    first_pkt.getSrcTalId(), first_pkt.getDstTalId());

	spot_id_t spot_id = first_pkt.getSpot();

	if (mesh_architecture) // Mesh architecture -> packet are routed according to their destination
	{
		// TODO: check that getDstTalId() returns the final destination
		tal_id_t dest_entity = first_pkt.getDstTalId();

		if (handled_entities.find(dest_entity) != handled_entities.end())
		{
			Component dest_type = conf->getEntityType(dest_entity);
			LOG(log_send, LEVEL_DEBUG, "Transmitting to the destination of the packet: %s %d",
			    getComponentName(dest_type).c_str(), dest_entity);
			if (dest_type == Component::terminal)
			{
				return sendToLowerBlock({spot_id, Component::terminal}, std::move(burst));
			}
			else if (dest_type == Component::gateway)
			{
				return sendToLowerBlock({spot_id, Component::gateway}, std::move(burst));
			}
			else
			{
				LOG(log_receive, LEVEL_ERROR, "Destination of the packet (%d) is neither a terminal nor a gateway", dest_entity);
				return false;
			}
		}
		else // destination not handled by this satellite -> transmit to default entity
		{
			Component default_entity_type = conf->getEntityType(default_entity);
			LOG(log_send, LEVEL_DEBUG, "Transmitting to default entity: %s %d",
			    getComponentName(default_entity_type).c_str(), default_entity);
			if (default_entity_type == Component::satellite)
			{
				return sendToOppositeChannel(std::move(burst));
			}
			else if (default_entity_type == Component::gateway)
			{
				return sendToLowerBlock({spot_id, Component::gateway}, std::move(burst));
			}
			else
			{
				LOG(log_receive, LEVEL_ERROR, "Default entity is neither a satellite nor a gateway");
				return false;
			}
		}
	}
	else // Star architecture -> packet are routed according to their source
	{
		// TODO: check that getSrcTalId() returns the actual source
		tal_id_t src_entity = first_pkt.getSrcTalId();
		Component src_type = conf->getEntityType(src_entity);
		LOG(log_send, LEVEL_DEBUG, "Transmitting according to the source of the packet: %s %d",
		    getComponentName(src_type).c_str(), src_entity);

		if (src_type == Component::terminal)
		{
			return sendToLowerBlock({spot_id, Component::gateway}, std::move(burst));
		}
		else if (src_type == Component::gateway)
		{
			return sendToLowerBlock({spot_id, Component::terminal}, std::move(burst));
		}
		else
		{
			LOG(log_receive, LEVEL_ERROR, "Source of the packet (%d) is neither a terminal nor a gateway", src_entity);
			return false;
		}
	}
	// unreachable
	return false;
}

bool BlockMesh::Downward::handleControlMsg(std::unique_ptr<const DvbFrame> frame)
{
	switch (frame->getMessageType())
	{
		// Control messages ST->GW
		case EmulatedMessageType::Sac:
		case EmulatedMessageType::Csc:
		case EmulatedMessageType::SessionLogonReq:
		case EmulatedMessageType::SessionLogoff:
		{
			SpotComponentPair key{frame->getSpot(), Component::gateway};
			return sendToLowerBlock(key, std::move(frame));
		}

		// Control messages GW->ST
		case EmulatedMessageType::Sof:
		case EmulatedMessageType::Ttp:
		case EmulatedMessageType::SessionLogonResp:
		{
			SpotComponentPair key{frame->getSpot(), Component::terminal};
			return sendToLowerBlock(key, std::move(frame));
		}

		default:
			LOG(log_receive, LEVEL_ERROR, "Unexpected control message type received: %s (%d)",
			    frame->getName().c_str(), frame->getMessageType());
			return false;
	}
}

bool BlockMesh::Downward::sendToLowerBlock(SpotComponentPair key, std::unique_ptr<const NetBurst> burst)
{
	LOG(log_send, LEVEL_DEBUG, "Sending a NetBurst to the lower block, in the spot %d %s stack",
	    key.spot_id, key.dest == Component::gateway ? "GW" : "ST");
	auto burst_ptr = burst.release();
	bool ok = enqueueMessage(key, (void **)&burst_ptr, sizeof(NetBurst), to_underlying(InternalMessageType::decap_data));
	if (!ok)
	{
		LOG(this->log_send, LEVEL_ERROR,
		    "Failed to transmit a NetBurst to the lower block");
		delete burst_ptr;
		return false;
	}
	return true;
}

bool BlockMesh::Downward::sendToLowerBlock(SpotComponentPair key, std::unique_ptr<const DvbFrame> frame)
{
	// LOG(log_send, LEVEL_DEBUG, "Sending a control DVB frame to the lower block, in the spot %d %s stack",
	    // key.spot_id, key.dest == Component::gateway ? "GW" : "ST");
	auto frame_ptr = frame.release();
	bool ok = enqueueMessage(key, (void **)&frame_ptr, sizeof(DvbFrame), to_underlying(InternalMessageType::sig));
	if (!ok)
	{
		LOG(this->log_send, LEVEL_ERROR,
		    "Failed to transmit a control DVB frame to the lower block");
		delete frame_ptr;
		return false;
	}
	return true;
}

bool BlockMesh::Downward::sendToOppositeChannel(std::unique_ptr<const NetBurst> burst)
{
	LOG(log_send, LEVEL_DEBUG, "Sending a NetBurst to the opposite channel");

	auto burst_ptr = burst.release();
	bool ok = shareMessage((void **)&burst_ptr, sizeof(NetBurst), to_underlying(InternalMessageType::decap_data));
	if (!ok)
	{
		LOG(this->log_send, LEVEL_ERROR,
		    "Failed to transmit a NetBurst to the opposite channel");
		delete burst_ptr;
		return false;
	}
	return true;
}

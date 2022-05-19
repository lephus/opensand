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
 * @file SpotUpward.cpp
 * @brief Upward spot related functions for DVB NCC block
 * @author Bénédicte Motto <bmotto@toulouse.viveris.com>
 * @author Julien Bernard <julien.bernard@toulouse.viveris.com>
 */


#include <sstream>

#include "SpotUpward.h"

#include "DvbRcsStd.h"
#include "DvbS2Std.h"
#include "Sof.h"
#include "Logon.h"
#include "PhysicStd.h"
#include "NetBurst.h"
#include "SlottedAlohaNcc.h"
#include "TerminalCategoryDama.h"
#include "UnitConverterFixedSymbolLength.h"
#include "OpenSandModelConf.h"


#include <opensand_output/OutputEvent.h>


SpotUpward::SpotUpward(spot_id_t spot_id,
                       tal_id_t mac_id,
                       StFmtSimuList *input_sts,
                       StFmtSimuList *output_sts):
	DvbChannel{},
	DvbFmt{},
	spot_id{spot_id},
	mac_id{mac_id},
	saloha{nullptr},
	reception_std{nullptr},
	reception_std_scpc{nullptr},
	scpc_pkt_hdl{nullptr},
	ret_fmt_groups{},
	is_tal_scpc{},
	probe_gw_l2_from_sat{nullptr},
	probe_received_modcod{nullptr},
	probe_rejected_modcod{nullptr},
	log_saloha{nullptr},
	event_logon_req{nullptr}
{
	this->super_frame_counter = 0;
	this->input_sts = input_sts;
	this->output_sts = output_sts;
}

SpotUpward::~SpotUpward()
{
	// delete FMT groups here because they may be present in many carriers
	// TODO do something to avoid groups here
	for(fmt_groups_t::iterator it = this->ret_fmt_groups.begin();
	    it != this->ret_fmt_groups.end(); ++it)
	{
		delete (*it).second;
	}

	delete this->reception_std;
	delete this->reception_std_scpc;
	delete this->saloha;
}

void SpotUpward::generateConfiguration(std::shared_ptr<OpenSANDConf::MetaParameter> disable_ctrl_plane)
{
	SlottedAlohaNcc::generateConfiguration(disable_ctrl_plane);
}


bool SpotUpward::onInit(void)
{
	if(!this->initModcodDefinitionTypes())
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "failed to initialize MOCODS definitions types\n");
		return false;
	}

	// get the common parameters
	if(!this->initCommon(EncapSchemeList::RETURN_UP))
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "failed to complete the common part of the "
		    "initialisation\n");
		return false;
	}

	// Get and open the files
	if(!this->initModcodSimu())
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "failed to complete the files part of the "
		    "initialisation\n");
		return false;
	}

	if(!this->initAcmLoopMargin())
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "failed to complete the ACM loop margin  part of the initialisation\n");
		return false;
	}

	if(!this->initMode())
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "failed to complete the mode part of the "
		    "initialisation\n");
		return false;
	}

	// synchronized with SoF
	this->initStatsTimer(this->ret_up_frame_duration_ms);

	if(!this->initOutput())
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "failed to complete the initialization of "
		    "statistics\n");
		goto error_mode;
	}

	// initialize the slotted Aloha part
	if(!this->initSlottedAloha())
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "failed to complete the DAMA part of the "
		    "initialisation\n");
		goto error_mode;
	}

	// everything went fine
	return true;

error_mode:
	delete this->reception_std;
	return false;

}


bool SpotUpward::initSlottedAloha(void)
{
	TerminalCategories<TerminalCategorySaloha> sa_categories;
	TerminalMapping<TerminalCategorySaloha> sa_terminal_affectation;
	TerminalCategorySaloha *sa_default_category;
	UnitConverter *converter;
	vol_sym_t length_sym = 0;

	auto Conf = OpenSandModelConf::Get();
	
	// Skip if the control plane is disabled
	bool ctrl_plane_disabled;
	Conf->getControlPlaneDisabled(ctrl_plane_disabled);
	if (ctrl_plane_disabled)
	{
		LOG(this->log_init_channel, LEVEL_NOTICE,
		    "Control plane disabled: skipping slotted aloha initialization");
		return true;
	}

	OpenSandModelConf::spot current_spot;
	if (!Conf->getSpotReturnCarriers(this->mac_id, current_spot))
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "there is no spot definition attached to the gateway %d\n",
		    this->mac_id);
		return false;
	}

	if(!this->initBand<TerminalCategorySaloha>(current_spot,
	                                           "return up frequency plan",
	                                           AccessType::ALOHA,
	                                           this->ret_up_frame_duration_ms,
	                                           this->rcs_modcod_def,
	                                           sa_categories,
	                                           sa_terminal_affectation,
	                                           &sa_default_category,
	                                           this->ret_fmt_groups))
	{
		return false;
	}

	// check if there is Slotted Aloha carriers
	if(sa_categories.size() == 0)
	{
		LOG(this->log_init_channel, LEVEL_DEBUG,
		    "No Slotted Aloha carrier\n");
		return true;
	}

	// TODO possible loss with Slotted Aloha and ROHC or MPEG
	//      (see TODO in TerminalContextSaloha.cpp)
	if(this->pkt_hdl->getName() == "MPEG2-TS")
	{
		LOG(this->log_init_channel, LEVEL_WARNING,
		    "Cannot guarantee no loss with MPEG2-TS and Slotted Aloha "
		    "on return link due to interleaving\n");
	}

	auto encap = Conf->getProfileData()->getComponent("encapsulation");
	for(auto& item : encap->getList("lan_adaptation_schemes")->getItems())
	{
		std::string protocol_name;
		auto lan_adaptation_scheme = std::dynamic_pointer_cast<OpenSANDConf::DataComponent>(item);
		if(!OpenSandModelConf::extractParameterData(lan_adaptation_scheme->getParameter("protocol"), protocol_name))
		{
			LOG(this->log_init_channel, LEVEL_ERROR,
			    "LAN Adaptation Scheme in global section "
				"is missing a protocol name\n");
			return false;
		}

		if(protocol_name == "ROHC")
		{
			LOG(this->log_init_channel, LEVEL_WARNING,
			    "Cannot guarantee no loss with RoHC and Slotted Aloha "
			    "on return link due to interleaving\n");
		}
	}
	// end TODO

	// Create the Slotted ALoha part
	this->saloha = new SlottedAlohaNcc();
	if(!this->saloha)
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "failed to create Slotted Aloha\n");
		return false;
	}

	// Initialize the Slotted Aloha parent class
	// Unlike (future) scheduling, Slotted Aloha get all categories because
	// it also handles received frames and in order to know to which
	// category a frame is affected we need to get source terminal ID
	if(!this->saloha->initParent(this->ret_up_frame_duration_ms,
	                             // pkt_hdl is the up_ret one because transparent sat
	                             this->pkt_hdl))
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "Slotted Aloha NCC Initialization failed.\n");
		goto release_saloha;
	}

	if(!Conf->getRcs2BurstLength(length_sym))
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "cannot get 'RCS2 Burst Length' value");
		goto release_saloha;
	}
	converter = new UnitConverterFixedSymbolLength(this->ret_up_frame_duration_ms,
	                                               0,
	                                               length_sym
	                                              );

	if(!this->saloha->init(sa_categories,
	                       sa_terminal_affectation,
	                       sa_default_category,
	                       this->spot_id,
	                       converter))
	{
		delete converter;
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "failed to initialize the Slotted Aloha NCC\n");
		goto release_saloha;
	}
	delete converter;

	return true;

release_saloha:
	delete this->saloha;
	return false;
}


bool SpotUpward::initModcodSimu(void)
{
	if(!this->initModcodDefFile(MODCOD_DEF_S2,
	                            &this->s2_modcod_def))
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "failed to initialize the forward link definition MODCOD file\n");
		return false;
	}
	if(!this->initModcodDefFile(MODCOD_DEF_RCS2,
	                            &this->rcs_modcod_def,
	                            this->req_burst_length))
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "failed to initialize the return link definition MODCOD file\n");
		return false;
	}

	return true;
}


bool SpotUpward::initMode(void)
{
	// initialize the reception standard
	// depending on the satellite type
	this->reception_std = new DvbRcs2Std(this->pkt_hdl);

	// If available SCPC carriers, a new packet handler is created at NCC
	// to received BBFrames and to be able to deencapsulate GSE packets.
	if(this->checkIfScpc())
	{
		EncapPlugin::EncapPacketHandler *fwd_pkt_hdl;
		std::vector<std::string> scpc_encap;

		// check that the forward encapsulation scheme is GSE
		// (this should be automatically set by the manager)
		if(!this->initPktHdl(EncapSchemeList::FORWARD_DOWN,
		                     &fwd_pkt_hdl))
		{
			LOG(this->log_init_channel, LEVEL_ERROR,
			    "failed to get forward packet handler\n");
			return false;
		}
		if (!OpenSandModelConf::Get()->getScpcEncapStack(scpc_encap) ||
			scpc_encap.size() <= 0)
		{
			LOG(this->log_init_channel, LEVEL_ERROR,
			    "failed to get SCPC encapsulation names\n");
			return false;
		}
		if(fwd_pkt_hdl->getName() != scpc_encap[0])
		{
			LOG(this->log_init_channel, LEVEL_ERROR,
			    "Forward packet handler is not %s while there is SCPC channels\n",
			    scpc_encap[0].c_str());
			return false;
		}

		if(!this->initScpcPktHdl(&this->scpc_pkt_hdl))
		{
			LOG(this->log_init_channel, LEVEL_ERROR,
			    "failed to get packet handler for receiving GSE packets\n");
			return false;
		}

		this->reception_std_scpc = new DvbScpcStd(this->scpc_pkt_hdl);
		LOG(this->log_init_channel, LEVEL_NOTICE,
		    "NCC is aware that there are SCPC carriers available \n");
	}

	if(!this->reception_std)
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "failed to create the reception standard\n");
		return false;
	}

	return true;
}


bool SpotUpward::initAcmLoopMargin(void)
{
	double ret_acm_margin_db;
	double fwd_acm_margin_db;
	auto Conf = OpenSandModelConf::Get();

	if(!Conf->getReturnAcmLoopMargin(ret_acm_margin_db))
	{
		LOG(this->log_fmt, LEVEL_ERROR,
		    "Section Advanced Links Settings, Return link ACM loop margin missing\n");
		return false;
	}

	if(!Conf->getForwardAcmLoopMargin(fwd_acm_margin_db))
	{
		LOG(this->log_fmt, LEVEL_ERROR,
		    "Section Advanced Links Settings, Forward link ACM loop margin missing\n");
		return false;
	}

	this->input_sts->setAcmLoopMargin(ret_acm_margin_db);
	this->output_sts->setAcmLoopMargin(fwd_acm_margin_db);

	return true;
}


bool SpotUpward::initOutput(void)
{
	auto output = Output::Get();
	// Events
	this->event_logon_req = output->registerEvent("Spot_%d.DVB.logon_request",
	                                              this->spot_id);

	if(this->saloha)
	{
		this->log_saloha = output->registerLog(LEVEL_WARNING,
		                                       "Spot_%d.Dvb.SlottedAloha",
		                                       this->spot_id);
	}

	// Output probes and stats
	std::ostringstream probe_name;
	probe_name << "Spot_" << this->spot_id << ".Throughputs.L2_from_SAT";
	this->probe_gw_l2_from_sat = output->registerProbe<int>(probe_name.str(), "Kbits/s", true, SAMPLE_AVG);
	this->l2_from_sat_bytes = 0;

	return true;
}


bool SpotUpward::checkIfScpc()
{
	TerminalCategories<TerminalCategoryDama> scpc_categories;
	TerminalMapping<TerminalCategoryDama> terminal_affectation;
	TerminalCategoryDama *default_category;
	fmt_groups_t ret_fmt_groups;

	OpenSandModelConf::spot current_spot;
	if (!OpenSandModelConf::Get()->getSpotReturnCarriers(this->mac_id, current_spot))
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "there is no spot definition attached to the gateway %d\n",
		    this->mac_id);
		return false;
	}

	if(!this->initBand<TerminalCategoryDama>(current_spot,
	                                         "return up frequency plan",
	                                         AccessType::SCPC,
	                                         // used for checking, no need to get a relevant value
	                                         5,
	                                         // we need S2 modcod definitions
	                                         this->s2_modcod_def,
	                                         scpc_categories,
	                                         terminal_affectation,
	                                         &default_category,
	                                         ret_fmt_groups))
	{
		return false;
	}

	// clear unused fmt_group
	for(fmt_groups_t::iterator it = ret_fmt_groups.begin();
	    it != ret_fmt_groups.end(); ++it)
	{
		delete (*it).second;
	}

	if(scpc_categories.size() == 0)
	{
		LOG(this->log_init_channel, LEVEL_INFO,
		    "No SCPC carriers\n");
		return false;
	}

	// clear unused category
	for(TerminalCategories<TerminalCategoryDama>::iterator it = scpc_categories.begin();
	    it != scpc_categories.end(); ++it)
	{
		delete (*it).second;
	}

	return true;
}


bool SpotUpward::handleFrame(DvbFrame *frame, NetBurst **burst)
{
	uint8_t msg_type = frame->getMessageType();
	bool corrupted = frame->isCorrupted();
	PhysicStd *std = this->reception_std;

	if(msg_type == MSG_TYPE_BBFRAME)
	{
		// decode the first packet in frame to be able to get source terminal ID
		if(!this->reception_std_scpc)
		{
			LOG(this->log_receive_channel, LEVEL_ERROR,
			    "Got BBFrame in transparent mode, without SCPC on carrier %u\n",
			    frame->getCarrierId());
			return false;
		}
		std = this->reception_std_scpc;
	}
	// TODO factorize if SCPC modcod handling is the same as for regenerative case
	// Update stats
	this->l2_from_sat_bytes += frame->getPayloadLength();

	if(!std->onRcvFrame(frame, this->mac_id, burst))
	{
		LOG(this->log_receive_channel, LEVEL_ERROR,
		    "failed to handle DVB frame or BB frame\n");
		return false;
	}
	NetBurst *pkt_burst = *burst;
	if(pkt_burst)
	{
		for (auto&& packet : *pkt_burst)
		{
			tal_id_t tal_id = packet->getSrcTalId();
			auto it_scpc = std::find(this->is_tal_scpc.begin(),
			                         this->is_tal_scpc.end(),
			                         tal_id);
			if(it_scpc != this->is_tal_scpc.end() &&
			   packet->getDstTalId() == this->mac_id)
			{
				uint32_t opaque = 0;
				if(!this->scpc_pkt_hdl->getHeaderExtensions(packet,
				                                            "deencodeCniExt",
				                                            &opaque))
				{
					LOG(this->log_receive_channel, LEVEL_ERROR,
					    "error when trying to read header extensions\n");
					return false;
				}
				if(opaque != 0)
				{
					// This is the C/N0 value evaluated by the Terminal
					// and transmitted via GSE extensions
					// TODO we could make specific SCPC function
					this->setRequiredCniOutput(tal_id, ncntoh(opaque));
					break;
				}
			}
		}
	}

	// TODO MODCOD should also be updated correctly for SCPC but at the moment
	//      FMT simulations cannot handle this, fix this once this
	//      will be reworked
	if(std->getType() == "DVB-S2")
	{
		DvbS2Std *s2_std = (DvbS2Std *)std;
		if(!corrupted)
		{
			this->probe_received_modcod->put(s2_std->getReceivedModcod());
			this->probe_rejected_modcod->put(0);
		}
		else
		{
			this->probe_rejected_modcod->put(s2_std->getReceivedModcod());
			this->probe_received_modcod->put(0);
		}
	}

	return true;
}


void SpotUpward::handleFrameCni(DvbFrame *dvb_frame)
{
	double curr_cni = dvb_frame->getCn();
	uint8_t msg_type = dvb_frame->getMessageType();
	tal_id_t tal_id;

	switch(msg_type)
	{
		// Cannot check frame type because of currupted frame
		case MSG_TYPE_SAC:
		{
			Sac *sac = (Sac *)dvb_frame;
			tal_id = sac->getTerminalId();
			if(!tal_id)
			{
				LOG(this->log_receive_channel, LEVEL_ERROR,
				    "unable to read source terminal ID in"
				    " frame, won't be able to update C/N"
				    " value\n");
				return;
			}
			break;
		}
		case MSG_TYPE_DVB_BURST:
		{
			// transparent case : update return modcod for terminal
			DvbRcsFrame *frame = dvb_frame->operator DvbRcsFrame*();
			// decode the first packet in frame to be able to
			// get source terminal ID
			if(!this->pkt_hdl->getSrc(frame->getPayload(),
			                          tal_id))
			{
				LOG(this->log_receive_channel, LEVEL_ERROR,
				    "unable to read source terminal ID in"
				    " frame, won't be able to update C/N"
				    " value\n");
				return;
			}
			break;
		}
		case MSG_TYPE_BBFRAME:
		{
			// SCPC
			BBFrame *frame = dvb_frame->operator BBFrame*();
			// decode the first packet in frame to be able to
			// get source terminal ID
			if(!this->scpc_pkt_hdl->getSrc(frame->getPayload(),
			                               tal_id))
			{
				LOG(this->log_receive_channel, LEVEL_ERROR,
				    "unable to read source terminal ID in"
				    " frame, won't be able to update C/N"
				    " value\n");
				return;
			}
			break;
		}
		default:
			LOG(this->log_receive_channel, LEVEL_ERROR,
			    "Wrong message type %u, this shouldn't happened", msg_type);
			return;
	}
	this->setRequiredCniInput(tal_id, curr_cni);
}


bool SpotUpward::onRcvLogonReq(DvbFrame *dvb_frame)
{
	//TODO find why dynamic cast fail here !?
//	LogonRequest *logon_req = dynamic_cast<LogonRequest *>(dvb_frame);
	LogonRequest *logon_req = (LogonRequest *)dvb_frame;
	uint16_t mac = logon_req->getMac();

	LOG(this->log_receive_channel, LEVEL_INFO,
	    "Logon request from ST%u on spot %u\n", mac, this->spot_id);

	// refuse to register a ST with same MAC ID as the NCC
	// or if it's a gw
	if(OpenSandModelConf::Get()->isGw(mac) or mac == this->mac_id)
	{
		LOG(this->log_receive_channel, LEVEL_ERROR,
		    "a ST wants to register with the MAC ID of the NCC "
		    "(%d), reject its request!\n", mac);
		delete dvb_frame;
		return false;
	}

	// send the corresponding event
	event_logon_req->sendEvent("Logon request received from ST%u on spot %u",
	                           mac, this->spot_id);

	if(!(this->input_sts->isStPresent(mac) && this->output_sts->isStPresent(mac)))
	{
		if(!this->addOutputTerminal(mac, this->s2_modcod_def))
		{
			LOG(this->log_receive_channel, LEVEL_ERROR,
			    "failed to handle FMT for ST %u, "
			    "won't send logon response\n", mac);
			return false;
		}
	}

	if(logon_req->getIsScpc())
	{
		this->is_tal_scpc.push_back(mac);
		// handle ST for FMT simulation
		if(!(this->input_sts->isStPresent(mac) && this->output_sts->isStPresent(mac)))
		{
			// ST was not registered yet
			if(!this->addInputTerminal(mac, this->s2_modcod_def))
			{
				LOG(this->log_receive_channel, LEVEL_ERROR,
				    "failed to handle FMT for ST %u, "
				    "won't send logon response\n", mac);
				return false;
			}
		}
	}
	else
	{
		// handle ST for FMT simulation
		if(!(this->input_sts->isStPresent(mac) && this->output_sts->isStPresent(mac)))
		{
			// ST was not registered yet
			if(!this->addInputTerminal(mac, this->rcs_modcod_def))
			{
				LOG(this->log_receive_channel, LEVEL_ERROR,
				    "failed to handle FMT for ST %u, "
				    "won't send logon response\n", mac);
				return false;
			}
		}
	}

	// Inform SlottedAloha
	if(this->saloha)
	{
		if(!this->saloha->addTerminal(mac))
		{
			LOG(this->log_receive_channel, LEVEL_ERROR,
			    "Cannot add terminal in Slotted Aloha context\n");
			return false;
		}
	}

	return true;
}


void SpotUpward::updateStats(void)
{
	if(!this->doSendStats())
	{
		return;
	}
	this->probe_gw_l2_from_sat->put(
		this->l2_from_sat_bytes * 8.0 / this->stats_period_ms);
	this->l2_from_sat_bytes = 0;

	// Send probes
	Output::Get()->sendProbes();
}


bool SpotUpward::scheduleSaloha(DvbFrame *dvb_frame,
                                std::list<DvbFrame *>* &ack_frames,
                                NetBurst **sa_burst)
{
	if(!this->saloha)
	{
		return true;
	}

	if (dvb_frame)
	{
		uint16_t sfn;
		Sof *sof = (Sof *)dvb_frame;

		sfn = sof->getSuperFrameNumber();

		// increase the superframe number and reset
		// counter of frames per superframe
		this->super_frame_counter++;
		if(this->super_frame_counter != sfn)
		{
			LOG(this->log_receive_channel, LEVEL_WARNING,
			    "superframe counter (%u) is not the same as in"
			    " SoF (%u)\n",
			    this->super_frame_counter, sfn);
			this->super_frame_counter = sfn;
		}
	}

	ack_frames = new std::list<DvbFrame *>();
	if(!this->saloha->schedule(sa_burst,
	                           *ack_frames,
	                           this->super_frame_counter))
	{
		LOG(this->log_saloha, LEVEL_ERROR,
		    "failed to schedule Slotted Aloha\n");
		delete dvb_frame;
		delete ack_frames;
		return false;
	}

	return true;
}


bool SpotUpward::handleSlottedAlohaFrame(DvbFrame *frame)
{
	// Update stats
	this->l2_from_sat_bytes += frame->getPayloadLength();

	if(!this->saloha->onRcvFrame(frame))
	{
		LOG(this->log_saloha, LEVEL_ERROR,
		    "failed to handle Slotted Aloha frame\n");
		return false;
	}
	return true;
}


bool SpotUpward::handleSac(const DvbFrame *dvb_frame)
{
	Sac *sac = (Sac *)dvb_frame;

	// transparent : the C/N0 of forward link
	double cni = sac->getCni();
	tal_id_t tal_id = sac->getTerminalId();
	this->setRequiredCniOutput(tal_id, cni);
	LOG(this->log_receive_channel, LEVEL_INFO,
	    "handle received SAC from terminal %u with cni %f\n",
	    tal_id, cni);
	
	return true;
}



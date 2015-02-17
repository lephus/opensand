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
 * @file SpotDownward.h
 * @brief This bloc implements a DVB-S/RCS stack for a Ncc.
 * @author SatIP6
 * @author Bénédicte Motto <bmotto@toulouse.viveris.com>
 *
 *
 */

#ifndef SPOT_DOWNWARD_H
#define SPOT_DOWNWARD_H

#include "BlockDvb.h"
#include "DamaCtrlRcs.h"
#include "NccPepInterface.h"
#include "Scheduling.h"
#include "SlottedAlohaNcc.h"

#define SIMU_BUFF_LEN 255

enum Simulate
{
	none_simu,
	file_simu,
	random_simu,
} ;


class SpotDownward: public DvbChannel, public NccPepInterface
{
	public:
		SpotDownward(time_ms_t fwd_down_frame_duration,
		             time_ms_t ret_up_frame_duration,
		             time_ms_t stats_period,
		             const FmtSimulation &up_fmt_simu,
		             const FmtSimulation &down_fmt_simu,
		             sat_type_t sat_type,
		             EncapPlugin::EncapPacketHandler *pkt_hdl,
		             bool phy_layer);
		~SpotDownward();
		bool onInit(void);
		bool handleMsgSaloha(list<DvbFrame *> *ack_frames);
		bool handleBurst(NetBurst::iterator pkt_it,
                         time_sf_t super_frame_counter);
	
		bool schedule(time_ms_t current_time, uint32_t remaining_alloc_sym);
		
		// statistics update
		void updateStatistics(void);
		void resetStatsCxt(void);

		/**
		 * Simulate event based on an input file
		 * @return true on success, false otherwise
		 */
		bool simulateFile(void);

		/**
		 * Simulate event based on random generation
		 */
		void simulateRandom(void);

		/**
		 *  @brief Handle a logon request transmitted by the opposite
		 *         block
		 *
		 *  @param dvb_frame  The frame contining the logon request
		 *  @return true on success, false otherwise
		 */
		bool handleLogonReq(DvbFrame *dvb_frame,
		                    LogonResponse **logonResp,
		                    uint8_t &ctrlCarrierId,
		                    time_sf_t super_frame_counter);
		/**
		 *  @brief Handle a logoff request transmitted by the opposite
		 *         block
		 *
		 *  @param dvb_frame  The frame contining the logoff request
		 *  @return true on success, false otherwise
		 */
		bool handleLogoffReq(DvbFrame *dvb_frame, time_sf_t super_frame_counter);


		
		/**
		 * Set/Get downward spot id
		 */ 
		void setSpotId(uint8_t spot_id);
		uint8_t getSpotId(void);

		DamaCtrlRcs * getDamaCtrl(void);
		
		/// The uplink of forward scheduling depending on satellite
		Scheduling *getScheduling(void);

		double getCni(void);
		void setCni(double cni);

		/// counter for forward frames
		time_sf_t getFwdFrameCounter(void);
		void setFwdFrameCounter(time_sf_t);

		uint8_t getCtrlCarrierId(void);
		uint8_t getSofCarrierId(void);
		uint8_t getDataCarrierId(void);

		list<DvbFrame *> &getCompleteDvbFrames(void);
		
		/// FMT groups for up/return
		fmt_groups_t getRetFmtGroups(void);

		/// parameters for request simulation
		FILE * getEventFile(void);
		FILE * getSimuFile(void);
		void setSimuFile(FILE *);
		enum Simulate getSimulate(void);
		void setSimulate(enum Simulate);

		// Output probes
		Probe<float> *getProbeFrameInterval(void);
		// Physical layer information
		Probe<int> *getProbeUsedModcod(void);

		// Output logs and events
		OutputLog *getLogRequestSimulation(void);

		EncapPlugin::EncapPacketHandler *getUpReturnPktHdl(void);

	protected:

		/**
		 * Read configuration for the downward timers
		 *
		 * @return  true on success, false otherwise
		 */
		bool initTimers(void);

		/**
		 * Read configuration for the carrier IDs
		 *
		 * @return  true on success, false otherwise
		 */
		bool initCarrierIds(void);

		/**
		 * @brief Initialize the transmission mode
		 *
		 * @return  true on success, false otherwise
		 */
		bool initMode(void);

		/**
		 * Read configuration for the DAMA algorithm
		 *
		 * @return  true on success, false otherwise
		 */
		bool initDama(void);

		/**
		 * @brief Read configuration for the FIFO
		 *
		 * @return  true on success, false otherwise
		 */
		bool initFifo(void);

		/**
		 * Read configuration for simulated FMT columns ID
		 *
		 * @return  true on success, false otherwise
		 */
		bool initColumns(void);

		/**
		 * @brief Initialize the statistics
		 *
		 * @return  true on success, false otherwise
		 */
		bool initOutput(void);

		/** Read configuration for the request simulation
		 *
		 * @return  true on success, false otherwise
		 */
		bool initRequestSimulation(void);

		/**
		 * @brief Send a SAC message containing ACM parameters
		 *
		 * @return true on success, false otherwise
		 */
		bool sendAcmParameters(void);

		/**
		 * @brief Handle a DVB frame transmitted from upward channel
		 *
		 * @param dvb_frame  The frame
		 * @return true on success, false otherwise
		 */
		//bool handleDvbFrame(DvbFrame *dvb_frame, LogonResponse **logonResp, uint8_t *ctrlCarrierId);

		

		/// The DAMA controller
		DamaCtrlRcs *dama_ctrl;

		/// The uplink of forward scheduling depending on satellite
		Scheduling *scheduling;

		/// frame timer for return, used to awake the block every frame period
		event_id_t frame_timer;

		/// frame timer for forward, used to awake the block every frame period
		event_id_t fwd_timer;

		/// counter for forward frames
		time_sf_t fwd_frame_counter;

		/// carrier ids
		uint8_t ctrl_carrier_id;
		uint8_t sof_carrier_id;
		uint8_t data_carrier_id;

		/// spot id
		uint8_t spot_id;

		/* Fifos */
		/// map of FIFOs per MAX priority to manage different queues
		fifos_t dvb_fifos;
		/// the default MAC fifo index = fifo with the smallest priority
		unsigned int default_fifo_id;

		/// the list of complete DVB-RCS/BB frames that were not sent yet
		list<DvbFrame *> complete_dvb_frames;

		/// The terminal categories for forward band
		TerminalCategories<TerminalCategoryDama> categories;

		/// The terminal affectation for forward band
		TerminalMapping<TerminalCategoryDama> terminal_affectation;

		/// The default terminal category for forward band
		TerminalCategoryDama *default_category;

		/// The up/return packet handler
		EncapPlugin::EncapPacketHandler *up_return_pkt_hdl;

		// TODO remove FMT groups from attributes
		// TODO we may create a class that inherit from fmt_groups_t (map) with
		//      a destructor that erases the map elements
		/// FMT groups for down/forward
		fmt_groups_t fwd_fmt_groups;

		/// FMT groups for up/return
		fmt_groups_t ret_fmt_groups;

		/// The MODCOD simulation elements for up/return link
		FmtSimulation up_ret_fmt_simu;
		/// The MODCOD simulation elements for down/forward link
		FmtSimulation down_fwd_fmt_simu;

		/// timer used to awake the block every second in order to retrieve
		/// the current MODCODs
		/// In regenerative case with physical layer, is it used to send
		// ACM parameters to satellite
		event_id_t scenario_timer;

		/// The C/N0 for downlink in regenerative scenario that will be transmited
		//  to satellite in SAC
		//  For transparent scenario the return link cni will be used to update return
		//  MODCOD id for terminals (not this one)
		double cni;

		/// The column ID for FMT simulation
		map<tal_id_t, uint16_t> column_list;

		/// timer used for applying resources allocations received from PEP
		event_id_t pep_cmd_apply_timer;

		/// Delay for allocation requests from PEP (in ms)
		int pep_alloc_delay;

		/// parameters for request simulation
		FILE *event_file;
		FILE *simu_file;
		Simulate simulate;
		long simu_st;
		long simu_rt;
		long simu_max_rbdc;
		long simu_max_vbdc;
		long simu_cr;
		long simu_interval;
		bool simu_eof;
		char simu_buffer[SIMU_BUFF_LEN];

		// Output probes and stats
		// Queue sizes
		map<unsigned int, Probe<int> *> probe_gw_queue_size;
		map<unsigned int, Probe<int> *> probe_gw_queue_size_kb;
		// Queue loss
		map<unsigned int, Probe<int> *> probe_gw_queue_loss;
		map<unsigned int, Probe<int> *> probe_gw_queue_loss_kb;
		// Rates
		map<unsigned int, Probe<int> *> probe_gw_l2_to_sat_before_sched;
		map<unsigned int, int> l2_to_sat_bytes_before_sched;
		map<unsigned int, Probe<int> *> probe_gw_l2_to_sat_after_sched;
		Probe<int> *probe_gw_l2_to_sat_total;
		int l2_to_sat_total_bytes;
		// Frame interval
		Probe<float> *probe_frame_interval;
		// Physical layer information
		Probe<int> *probe_used_modcod;

		// Output logs and events
		OutputLog *log_request_simulation;

		/// logon response sent
		OutputEvent *event_logon_resp;
};

#endif

#!/usr/bin/env python
# -*- coding: utf8 -*-

#
#
# OpenSAND is an emulation testbed aiming to represent in a cost effective way a
# satellite telecommunication system for research and engineering activities.
#
#
# Copyright © 2012 CNES
#
#
# This file is part of the OpenSAND testbed.
#
#
# OpenSAND is free software : you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY, without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program. If not, see http://www.gnu.org/licenses/.
#
#

# Author: Vincent Duvert / Viveris Technologies <vduvert@toulouse.viveris.com>


from opensand_manager_core.model.environment_plane import Program
from tempfile import TemporaryFile
from zipfile import ZipFile
import gobject
import socket
import struct

MAGIC_NUMBER = 0x5A7D0001
MSG_MGR_REGISTER = 21
MSG_MGR_REGISTER_PROGRAM = 22
MSG_MGR_UNREGISTER_PROGRAM = 23
MSG_MGR_SET_PROBE_STATUS = 24
MSG_MGR_SEND_PROBES = 25
MSG_MGR_SEND_EVENT = 26
MSG_MGR_UNREGISTER = 27
MSG_MGR_REGISTER_ACK = 28


class EnvironmentPlaneController(object):
    """
    Controller for the environment plane.
    """
    
    def __init__(self, model, manager_log):
        self._model = model
        self._log = manager_log
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._sock.bind(('', 0))
        self._tag = gobject.io_add_watch(self._sock, gobject.IO_IN,
            self._data_received)
        self._collector_addr = None
        
        self._transfer_port = 0
        self._transfer_dest = None
        self._transfer_dial = None
        self._transfer_cb = None
        self._transfer_file = None
        self._transfer_remaining = 0
        
        self._programs = {}
        self._observer = None
    
    def set_observer(self, observer):
        """
        Sets the observer object which will be notified of changes on the
        program list.
        """
        
        self._observer = observer
    
    def register_on_collector(self, ipaddr, port, transfer_port):
        """
        Register the probe controller on the specified collector.
        """
                
        addr = (ipaddr, port)
        
        if self._collector_addr:
            self._log.info("Found a duplicate collector on %s:%d, ignoring" %
                addr)
            return
    
        self._collector_addr = addr
        self._transfer_port = transfer_port
        self._log.info("Registering on collector %s:%d" % addr)
        self._sock.sendto(struct.pack("!LB", MAGIC_NUMBER, MSG_MGR_REGISTER),
            addr)
    
    def unregister_on_collector(self):
        """
        Unregisters on the specified collector.
        """
        
        if not self._collector_addr:
            return
        
        self._log.info("Unregistering on collector %s:%d" %
            self._collector_addr)
        
        self._sock.sendto(struct.pack("!LB", MAGIC_NUMBER, MSG_MGR_UNREGISTER),
            self._collector_addr)
        
        self._collector_addr = None
    
    def cleanup(self):
        """
        Shut down the probe controller.
        """

        self.unregister_on_collector()

        gobject.source_remove(self._tag)

        try:
            self._sock.shutdown(socket.SHUT_RDWR)
        except socket.error:
            pass

        self._sock.close()
    
    def get_programs(self):
        """
        Returns a list of all known programs.
        """
        
        return self._programs.values()
    
    def get_program(self, ident):
        """
        Returns the program identified by ident
        """
        
        return self._programs[ident]
    
    def transfer_from_collector(self, destination, prog_dialog, comp_callback):
        """
        Gets the probe data from the collector and puts the files in the
        destination folder.
        """
        
        if not self._transfer_port:
            raise RuntimeError("Collector transfer port not known")
        
        self._log.debug("Initiating probe transfer from collector")
        
        if prog_dialog:
            prog_dialog.show()
        
        transfer_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        transfer_socket.connect((self._collector_addr[0], self._transfer_port))
        
        self._transfer_dest = destination
        self._transfer_dial = prog_dialog
        self._transfer_cb = comp_callback
        self._transfer_file = TemporaryFile()
        
        gobject.io_add_watch(transfer_socket, gobject.IO_IN,
            self._transfer_header)
        
    def _transfer_header(self, transfer_socket, _tag):
        """
        Handles the transfer of the probe data header
        """
    
        header = ""
        while len(header) < 4:
            header += transfer_socket.recv(4 - len(header))
        
        length = struct.unpack("!L", header)[0]
        self._transfer_remaining = length
        
        self._log.debug("Probe transfer: got length = %d" % length)
        
        gobject.io_add_watch(transfer_socket, gobject.IO_IN,
            self._transfer_data)
    
        if self._transfer_dial:
            self._transfer_dial.ping()
    
        return False
    
    def _transfer_data(self, transfer_socket, _tag):
        """
        Handles the transfer of the probe data
        """
    
        if self._transfer_remaining == 0:
            transfer_socket.close()
            gobject.idle_add(self._transfer_unzip)
            return False
        
        to_read = min(self._transfer_remaining, 4096)
        data = transfer_socket.recv(to_read)
        self._transfer_remaining -= len(data)
        
        self._transfer_file.write(data)
        
        if self._transfer_dial:
            self._transfer_dial.ping()
        
        self._log.debug("Got data, %d remaining" % self._transfer_remaining)
        
        return True
    
    def _transfer_unzip(self):
        """
        Handles the decompression of the probe data
        """
    
        self._log.debug("Extracting")
    
        self._transfer_file.seek(0)
        zip_file = ZipFile(self._transfer_file, "r")
        zip_file.extractall(self._transfer_dest)
        zip_file.close()
        
        self._log.debug("Done")
        
        if self._transfer_dial:
            self._transfer_dial.close()
        
        gobject.idle_add(self._transfer_cb)
    
        return False
        
    def _data_received(self, _sock, _tag):
        """
        Called when a packet is received on the socket. Decodes and interprets
        the message.
        """
    
        packet, addr = self._sock.recvfrom(4096)
            
        if addr != self._collector_addr:
            self._log.error("Received data from unknown host %s:%d." % addr)
            return True
        
        if len(packet) < 5:
            self._log.error("Received short packet from the collector.")
            return True
        
        magic, cmd = struct.unpack("!LB", packet[0:5])
        if magic != MAGIC_NUMBER:
            self._log.error("Received bad magic number from the collector.")
            return True
        
        if cmd == MSG_MGR_REGISTER_ACK:
            self._model.set_collector_functional(True)
            return True
        
        if cmd == MSG_MGR_REGISTER_PROGRAM:
            try:
                success = self._handle_register_program(packet[5:])
            except struct.error:
                success = False

            if not success:
                self._log.error("Bad data received for REGISTER_PROGRAM "
                    "command.")
            
            return True
        
        if cmd == MSG_MGR_UNREGISTER_PROGRAM:
            try:
                success = self._handle_unregister_program(packet[5:])
            except struct.error:
                success = False

            if not success:
                self._log.error("Bad data received for UNREGISTER_PROGRAM "
                    "command.")
            
            return True
        
        if cmd == MSG_MGR_SEND_PROBES:
            try:
                success = self._handle_send_probes(packet[5:])
            except struct.error:
                success = False
            
            if not success:
                self._log.error("Bad data received for SEND_PROBES command.")
            
            return True
        
        if cmd == MSG_MGR_SEND_EVENT:
            try:
                success = self._handle_send_event(packet[5:])
            except struct.error:
                success = False
            
            if not success:
                self._log.error("Bad data received for SEND_EVENT command.")
            
            return True
        
        self._log.error("Unknown message id %d received from the collector." % 
            cmd)
    
        return True
    
    def _handle_register_program(self, data):
        """
        Handles a registration message.
        """
                
        host_id, prog_id, num_probes, num_events, name_length = struct.unpack(
            "!BBBBB", data[0:5])
        prog_name = data[5:5 + name_length]
        full_prog_id = (host_id << 8) | prog_id

        if len(prog_name) != name_length:
            return False


        pos = 5 + name_length
        
        probe_list = []
        for _ in xrange(num_probes):
            storage_type, name_length, unit_length = struct.unpack("!BBB",
                data[pos:pos + 3])
            enabled = bool(storage_type & (1 << 7))
            displayed = bool(storage_type & (1 << 6))
            storage_type = storage_type & ~(3 << 6)
            pos += 3

            name = data[pos:pos + name_length]
            if len(name) != name_length:
                return False
            
            pos += name_length
            
            unit = data[pos:pos + unit_length]
            if len(unit) != unit_length:
                return False

            pos += unit_length

            probe_list.append((name, unit, storage_type, enabled, displayed))

        event_list = []
        for _ in xrange(num_events):
            level, ident_length = struct.unpack("!BB", data[pos:pos+2])
            pos += 2

            ident = data[pos:pos + ident_length]
            if len(ident) != ident_length:
                return False
            
            event_list.append((ident, level))
            pos += ident_length

        if data[pos:] != "":
            return False
           
        self._log.debug("Registration of [%d:%d] %s %r %r" % (host_id, prog_id,
            prog_name, probe_list, event_list))
        
        
        program = Program(self, full_prog_id, prog_name, probe_list, event_list)
        self._programs[full_prog_id] = program
        
        if self._observer:
            self._observer.program_list_changed()
        
        return True
    
    def _handle_unregister_program(self, data):
        """
        Handles an unregistration message.
        """
        
        host_id, prog_id = struct.unpack("!BB", data)
        full_prog_id = (host_id << 8) | prog_id
        
        self._log.debug("Unregistration of [%d:%d]" % (host_id, prog_id))
        
        try:
            del self._programs[full_prog_id]
        except KeyError:
            self._log.error("Unregistering program [%d:%d] not found" %
                (host_id, prog_id))
        
        if self._observer:
            self._observer.program_list_changed()
        
        return True
    
    def _handle_send_probes(self, data):
        """
        Handles probes transmission.
        """
        
        host_id, prog_id, timestamp = struct.unpack("!BBL", data[0:6])
        full_prog_id = (host_id << 8) | prog_id
        
        try:
            program = self._programs[full_prog_id]
        except KeyError:
            self._log.error("Program [%d:%d] which sent probe data is not "
                "found" % (host_id, prog_id))
            return False
        
        pos = 6
        total_length = len(data)
        
        while pos < total_length:
            probe_id = struct.unpack("!B", data[pos])[0]
            pos += 1

            try:
                probe = program.get_probe(probe_id)
            except IndexError:
                self._log.error("Unknown probe ID %d" % probe_id)
                return False

            value, pos = probe.read_value(data, pos)
            
            if self._observer:
                self._observer.new_probe_value(probe, timestamp, value)
        
        return True
    
    def _handle_send_event(self, data):
        """
        Handles events transmission.
        """
        
        host_id, prog_id, event_id = struct.unpack("!BBB", data[0:3])
        message = data[3:]
        full_prog_id = (host_id << 8) | prog_id
        
        try:
            program = self._programs[full_prog_id]
        except KeyError:
            self._log.error("Program [%d:%d] which sent event data is not "
                "found" % (host_id, prog_id))
            return False
        
        try:
            name, level = program.get_event(event_id)
        except IndexError:
            self._log.error("Incorrect event ID %d for program [%d:%d] "
                "received" % (event_id, host_id, prog_id))
            
        if self._observer:
            self._observer.new_event(program, name, level, message)
        
        return True
    
    def update_probe_status(self, probe):
        """
        Notifies the collector that the status of a given probe has changed.
        """
        
        if not self._collector_addr:
            raise RuntimeError("Unable to update probe status: collector "
                "unknown.")
        
        host_id = (probe.program.ident >> 8) & 0xFF
        program_id = probe.program.ident & 0xFF
        probe_id = probe.ident
        
        self._log.debug("Updating status of probe %d on program %d:%d: "
            "enabled = %s, displayed = %s" % (probe_id, host_id, program_id,
            probe._enabled, probe._displayed))
        
        state = 2 if probe._displayed else (1 if probe._enabled else 0)
        
        message = struct.pack("!LBBBBB", MAGIC_NUMBER, MSG_MGR_SET_PROBE_STATUS,
            host_id, program_id, probe_id, state)
        
        self._sock.sendto(message, self._collector_addr)        

if __name__ == '__main__':
    import logging
    import sys

    logging.basicConfig(level=logging.DEBUG)
    MAIN_LOOP = gobject.MainLoop()
    CONTROLLER = EnvironmentPlaneController()
    try:
        CONTROLLER.register_on_collector("127.0.0.1", int(sys.argv[1]), 0)
        MAIN_LOOP.run()
    finally:
        CONTROLLER.cleanup()
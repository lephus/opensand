#!/usr/bin/env python 
# -*- coding: utf-8 -*-

#
#
# Platine is an emulation testbed aiming to represent in a cost effective way a
# satellite telecommunication system for research and engineering activities.
#
#
# Copyright © 2011 TAS
#
#
# This file is part of the Platine testbed.
#
#
# Platine is free software : you can redistribute it and/or modify it under the
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

# Author: Julien BERNARD / <jbernard@toulouse.viveris.com>

"""
platine_xml_parser.py - the XML parser and builder for Platine configuration

  The Platine configuration XML format is:
  <?xml version="1.0" encoding="UTF-8"?>
  <configuration component='COMPO'>
    <SECTION>
      <KEY>VAL</KEY>
      <TABLES>
        <TABLE ATTRIBUTE1="VAL1" ATTRIBUTE2="VAL2" />
        <TABLE ATTRIBUTE1="VAL1'" ATTRIBUTE2="VAL2'" />
      </TABLES>
    </SECTION>
  </configuration>
"""

from copy import deepcopy

from lxml import etree
from platine_manager_core.my_exceptions import XmlException


class XmlParser:
    """ XML parser for Platine configuration """
    def __init__ (self, xml, xsd):
        self._tree = None
        self._filename = xml
        self._xsd = xsd
        self._schema = None
        try:
            self._tree = etree.parse(xml)
            self._schema = etree.XMLSchema(etree.parse(xsd))
        except IOError, err:
            raise
        except etree.XMLSyntaxError, err:
            raise XmlException(err.error_log)

        if not self._schema.validate(self._tree):
            raise XmlException(self._schema.error_log)

        root = self._tree.getroot()
        if(root.tag != "configuration"):
            raise XmlException("Not a configuration file, root element is %s" %
                               root.tag)

    def get_sections(self):
        """ get all the sections in the configuration file """
        root = self._tree.getroot()
        return [sect for sect in root.iterchildren()
                     if sect.tag is not etree.Comment]

    def get_name(self, node):
        """ get the name of any XML node """
        return node.tag

    def get_keys(self, section):
        """ get all the keys in a section """
        return [key for key in section.iterchildren()
                    if key.tag is not etree.Comment]

    def is_table(self, key):
        """ check if the key is a table one """
        return len(key) > 0

    def get_value(self, key):
        """ get the value stored in a key """
        if self.is_table(key):
            raise XmlException("Key is a table, cannot get its value")
        return key.text

    def get_table_elements(self, key):
        """ get the list of element in a table """
        return [elt for elt in key.iterchildren()
                    if elt.tag is not etree.Comment]

    def get_element_content(self, element):
        """ get a dictionnary containing the attributes and values of a list
            element """
        return element.attrib

    def get_path(self, elt):
        """ get the xpath of an XML attribute or key """
        return self._tree.getpath(elt)

    def set_value(self, val, path, att=None):
        """ set the value of a key """
        elt = self._tree.xpath(path)
        if len(elt) != 1:
            raise XmlException("wrong path %s" % path)

        if att is not None:
            if not self.is_table(elt):
                raise XmlException("wrong path: %s is not a table" % elt.tag)
            att_list = self.get_element_content(elt[0])
            if not att in att_list:
                raise XmlException("wrong path: %s is not a valid attribute" %
                                   att)

            att_list[att] = val
        else:
            elt[0].text = val

    def get(self, xpath):
        """ get a XML element with its path """
        elts = self._tree.xpath(xpath)
        if len(elts) == 1:
            return elts[0]
        return None

    def get_all(self, xpath):
        """ get all matching elements """
        return self._tree.xpath(xpath)

    def del_element(self, xpath):
        """ delete an element from its path """
        elts = self._tree.xpath(xpath)
        if len(elts) == 1:
            elt = elts[0]
            elt.getparent().remove(elt)

    def add_line(self, key):
        tables = self._tree.xpath(key)
        if len(tables) != 1:
            raise XmlException("wrong path: %s is not valid" % key)
        table = tables[0]
        children = table.getchildren()
        if len(children) == 0:
            raise XmlException("wrong path: %s is not a table" % key)
        child = children[0]
        new = deepcopy(child)
        for att in new.attrib.keys():
            new.attrib[att] = ''
        table.append(new)

    def write(self):
        """ write the new configuration in file """
        if not self._schema.validate(self._tree):
            error = self._schema.error_log.last_error.message
            self.__init__(self._filename, self._xsd) 
            raise XmlException("the new values are incorrect",
                               error)

        with open(self._filename, 'w') as conf:
            conf.write(etree.tostring(self._tree, pretty_print=True,
                                      encoding=self._tree.docinfo.encoding,
                                      xml_declaration=True))



if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print "Usage: %s xml_file xsd_file" % sys.argv[0]
        sys.exit(1)
    print "use schema: %s and xml: %s" % (sys.argv[2],
                                          sys.argv[1])

    PARSER = XmlParser(sys.argv[1], sys.argv[2])

    for SECTION in PARSER.get_sections():
        print PARSER.get_name(SECTION)
        for KEY in PARSER.get_keys(SECTION):
            if not PARSER.is_table(KEY):
                print "\t%s=%s" % (PARSER.get_name(KEY),
                                   PARSER.get_value(KEY))
            else:
                print "\t%s" % PARSER.get_name(KEY)
                for ELT in PARSER.get_table_elements(KEY):
                    print "\t\t%s -> %s" % (PARSER.get_name(ELT),
                                            PARSER.get_element_content(ELT))
    sys.exit(0)






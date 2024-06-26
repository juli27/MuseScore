#!/usr/bin/env python3

# If you get Unicode errors on Windows, try setting the environment variable
# PYTHONIOENCODING=utf-8. More info at https://stackoverflow.com/a/12834315

import xml.etree.ElementTree as ET
import os

def addMessage(f, text, comment='', category='InstrumentsXML'):
    if (comment):
        f.write('QT_TRANSLATE_NOOP3("'+category+'", "' + text + '", "' + comment + '"),\n')
    else:
        f.write('QT_TRANSLATE_NOOP("'+category+'", "' + text + '"),\n')


# Create instrumentsxml.h (must specify encoding and line ending on Windows)
f = open('instrumentsxml.h', 'w', newline='\n', encoding='utf-8')

f.write("/*\n\
 * SPDX-License-Identifier: GPL-3.0-only\n\
 * MuseScore-CLA-applies\n\
 *\n\
 * MuseScore\n\
 * Music Composition & Notation\n\
 *\n\
 * Copyright (C) 2021 MuseScore BVBA and others\n\
 *\n\
 * This program is free software: you can redistribute it and/or modify\n\
 * it under the terms of the GNU General Public License version 3 as\n\
 * published by the Free Software Foundation.\n\
 *\n\
 * This program is distributed in the hope that it will be useful,\n\
 * but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
 * GNU General Public License for more details.\n\
 *\n\
 * You should have received a copy of the GNU General Public License\n\
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.\n\
 */\n")

#include template names and template categories
d = "../templates"
for o in sorted(os.listdir(d)): # sort to get same ordering on all platforms
    ofullPath = os.path.join(d, o)
    if os.path.isdir(ofullPath):
        templateCategory = o.split("-")[1].replace("_", " ")
        addMessage(f, templateCategory, '', 'Templates')
        print(templateCategory)
        for t in sorted(os.listdir(ofullPath)): # sort to get same ordering on all platforms
            if (os.path.isfile(os.path.join(ofullPath, t))):
                templateName = os.path.splitext(t)[0].split("-")[1].replace("_", " ")
                addMessage(f, templateName, '', 'Templates')
                print("    " + templateName)

#instruments.xml
tree = ET.parse('instruments.xml')
root = tree.getroot()

previousLongName = ""
for child in root:
    if child.tag == "Genre":
        genre = child.find("name")
        print("Genre " + genre.text)
        addMessage(f, genre.text)
    if child.tag == "Family":
        family = child.find("name")
        print("Family " + family.text)
        addMessage(f, family.text)
    elif child.tag == "InstrumentGroup":
        instrGroup = child.find("name")
        print("Instr Group : " + instrGroup.text)
        addMessage(f, instrGroup.text)
        instruments = child.findall("Instrument")
        for instrument in instruments:
            description = instrument.find("description")
            if description is not None:
                print("  description : " + description.text)
                addMessage(f, description.text)

            longName = instrument.find("longName")
            if longName is not None:
                print("  longName : " + longName.text)
                addMessage(f, longName.text)
                previousLongName = longName.text

            shortName = instrument.find("shortName")
            if shortName is not None:
                print("  shortName : " + shortName.text)
                addMessage(f, shortName.text, previousLongName)
                previousLongName = ""

            trackName = instrument.find("trackName")
            if trackName is not None:
                print("  trackName " + trackName.text)
                addMessage(f, trackName.text)
                previousLongName = ""

            channels = instrument.findall("Channel")
            for channel in channels:
                channelName = channel.get("name")
                if channelName is not None:
                    print("  Channel name : " + channelName)
                    addMessage(f, channelName)
                cMidiActions = channel.findall("MidiAction")
                for cma in cMidiActions:
                    cmaName = cma.get("name")
                    if cmaName is not None:
                        print("    Channel, MidiAction name :" + cmaName)
                        addMessage(f, cma)

            iMidiActions = instrument.findall("MidiAction")
            for ima in iMidiActions:
                imaName = ima.get("name")
                if imaName is not None:
                    print("  Instrument, MidiAction name :" + imaName)
                    addMessage(f, ima)

#orders.xml
tree = ET.parse('orders.xml')
root = tree.getroot()

for child in root:
    if child.tag == "Order":
        order = child.find("name")
        print("Order " + order.text)
        addMessage(f, order.text, '', "OrderXML")

f.close()

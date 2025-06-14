#!/usr/bin/env python

"""Convert Standard MIDI Files to human readable TOML"""

from argparse import ArgumentParser
import sys

from mido import MidiFile

def main(args):
  for midi_file in args.midifile:
    print(midi_file)
    midi = MidiFile(midi_file)
    print(midi)

if '__main__' == __name__:
  arg_parser = ArgumentParser(description='Convert Standard MIDI Files to human readable TOML')
  arg_parser.add_argument('midifile', nargs='+')

  sys.exit(main(arg_parser.parse_args()))

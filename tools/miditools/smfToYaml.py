#!/usr/bin/env python

"""Convert Standard MIDI Files to human readable YAML"""

from pathlib import Path

import yaml

from argparse import ArgumentParser
from mido import MidiFile, MidiTrack

def event_to_dict(event) -> dict:
  data = event.dict()
  time = data.pop('time')

  return {'deltatime': time, 'message': data}

def track_to_list(track: MidiTrack) -> list:
  return [event_to_dict(e) for e in track]

def to_yaml(midifile: MidiFile, outfile) -> None:
  yaml.dump(
    {'type': midifile.type,
     'division': midifile.ticks_per_beat,
     'tracks': [track_to_list(track) for track in midifile.tracks]},
     outfile, default_flow_style=False)

def main(args):
  for midifilepath in args.midifile:
    midi_file = MidiFile(midifilepath)

    r = dict()


    yaml_path = Path(midifilepath).with_suffix('.yaml')
    with open(yaml_path, 'w') as outfile:
      to_yaml(midi_file, outfile)

if '__main__' == __name__:
  arg_parser = ArgumentParser(description='Convert Standard MIDI Files to human readable YAML')
  arg_parser.add_argument('midifile', nargs='+')

  main(arg_parser.parse_args())

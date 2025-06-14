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
  yaml_path = Path(args.midi_file).with_suffix('.yaml')
  with open(yaml_path, 'w') as outfile:
    to_yaml(MidiFile(args.midi_file), outfile)

if '__main__' == __name__:
  arg_parser = ArgumentParser(description='Convert Standard MIDI Files to human readable YAML')
  arg_parser.add_argument('midi_file')

  main(arg_parser.parse_args())

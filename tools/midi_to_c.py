#!/usr/bin/env python3
"""
MIDI to C Array Converter for Dodepan Demo Mode

Converts Standard MIDI Files (.mid) to C header files with embedded byte arrays.
Handles Type 0 (single track) and Type 1 (multi-track) files by merging all tracks
into a single event stream sorted by absolute time.

Usage:
    python midi_to_c.py song1.mid song2.mid ...
    python midi_to_c.py *.mid -o demo_songs.h

Output: demo_songs.h with const uint8_t arrays and a song index.
"""

import argparse
import struct
import os
from dataclasses import dataclass
from pathlib import Path
from typing import List, Tuple


@dataclass
class MidiEvent:
    """A MIDI event with absolute timestamp in ticks."""
    abs_time: int
    data: bytes  # Raw MIDI bytes for this event


def read_variable_length(data: bytes, offset: int) -> Tuple[int, int]:
    """Read a MIDI variable-length quantity. Returns (value, bytes_consumed)."""
    value = 0
    bytes_read = 0
    while True:
        byte = data[offset + bytes_read]
        bytes_read += 1
        value = (value << 7) | (byte & 0x7F)
        if not (byte & 0x80):
            break
        if bytes_read > 4:
            raise ValueError("Variable length quantity too long")
    return value, bytes_read


def write_variable_length(value: int) -> bytes:
    """Encode an integer as MIDI variable-length quantity."""
    if value == 0:
        return bytes([0])
    
    result = []
    while value:
        result.append(value & 0x7F)
        value >>= 7
    
    # Reverse and set continuation bits
    result = result[::-1]
    for i in range(len(result) - 1):
        result[i] |= 0x80
    
    return bytes(result)


def parse_midi_file(filepath: str) -> Tuple[int, int, List[List[MidiEvent]]]:
    """
    Parse a MIDI file and extract events from all tracks.
    Returns: (format_type, ticks_per_quarter, list_of_tracks)
    Each track is a list of MidiEvent with absolute timestamps.
    """
    with open(filepath, 'rb') as f:
        data = f.read()
    
    # Parse header chunk
    if data[0:4] != b'MThd':
        raise ValueError(f"Not a valid MIDI file: {filepath}")
    
    header_len = struct.unpack('>I', data[4:8])[0]
    format_type, num_tracks, time_div = struct.unpack('>HHH', data[8:14])
    
    if time_div & 0x8000:
        raise ValueError("SMPTE time division not supported")
    
    ticks_per_quarter = time_div
    
    # Parse track chunks
    tracks = []
    offset = 8 + header_len
    
    for track_idx in range(num_tracks):
        if data[offset:offset+4] != b'MTrk':
            raise ValueError(f"Invalid track chunk at offset {offset}")
        
        track_len = struct.unpack('>I', data[offset+4:offset+8])[0]
        track_data = data[offset+8:offset+8+track_len]
        
        events = parse_track(track_data)
        tracks.append(events)
        
        offset += 8 + track_len
    
    return format_type, ticks_per_quarter, tracks


def parse_track(track_data: bytes) -> List[MidiEvent]:
    """Parse a single MIDI track, returning events with absolute timestamps."""
    events = []
    offset = 0
    abs_time = 0
    running_status = 0
    
    while offset < len(track_data):
        # Read delta time
        delta, consumed = read_variable_length(track_data, offset)
        offset += consumed
        abs_time += delta
        
        if offset >= len(track_data):
            break
            
        status = track_data[offset]
        
        # Handle running status
        if status & 0x80:
            running_status = status
            offset += 1
        else:
            status = running_status
        
        if status == 0xFF:
            # Meta event
            if offset >= len(track_data):
                break
            meta_type = track_data[offset]
            offset += 1
            length, consumed = read_variable_length(track_data, offset)
            offset += consumed
            meta_data = track_data[offset:offset+length]
            offset += length
            
            # Keep tempo events (type 0x51) for timing calculation
            if meta_type == 0x51:
                events.append(MidiEvent(abs_time, bytes([0xFF, meta_type]) + write_variable_length(length) + meta_data))
            # End of track
            elif meta_type == 0x2F:
                events.append(MidiEvent(abs_time, bytes([0xFF, 0x2F, 0x00])))
            # Skip other meta events
            
        elif status == 0xF0 or status == 0xF7:
            # SysEx event - skip
            length, consumed = read_variable_length(track_data, offset)
            offset += consumed + length
            
        elif status >= 0x80:
            # Channel message
            msg_type = status & 0xF0
            
            if msg_type in (0x80, 0x90, 0xA0, 0xB0, 0xE0):
                # Two data bytes: Note Off, Note On, Aftertouch, Control Change, Pitch Bend
                if offset + 1 < len(track_data):
                    data1 = track_data[offset]
                    data2 = track_data[offset + 1]
                    offset += 2
                    events.append(MidiEvent(abs_time, bytes([status, data1, data2])))
            elif msg_type in (0xC0, 0xD0):
                # One data byte: Program Change, Channel Pressure
                if offset < len(track_data):
                    data1 = track_data[offset]
                    offset += 1
                    events.append(MidiEvent(abs_time, bytes([status, data1])))
    
    return events


def merge_tracks(tracks: List[List[MidiEvent]]) -> List[MidiEvent]:
    """Merge multiple tracks into a single track sorted by absolute time."""
    all_events = []
    for track in tracks:
        all_events.extend(track)
    
    # Sort by time, keeping original order for same-time events
    all_events.sort(key=lambda e: e.abs_time)
    
    # Remove duplicate end-of-track events, keep only the last one
    end_of_track = None
    filtered = []
    for event in all_events:
        if event.data == bytes([0xFF, 0x2F, 0x00]):
            end_of_track = event
        else:
            filtered.append(event)
    
    if end_of_track:
        filtered.append(end_of_track)
    
    return filtered


def events_to_midi_bytes(events: List[MidiEvent], ticks_per_quarter: int) -> bytes:
    """Convert merged events back to MIDI track format with delta times."""
    result = bytearray()
    prev_time = 0
    
    for event in events:
        delta = event.abs_time - prev_time
        prev_time = event.abs_time
        
        result.extend(write_variable_length(delta))
        result.extend(event.data)
    
    return bytes(result)


def create_merged_midi(events: List[MidiEvent], ticks_per_quarter: int) -> bytes:
    """Create a complete Type 0 MIDI file from merged events."""
    track_data = events_to_midi_bytes(events, ticks_per_quarter)
    
    # Build MIDI file
    result = bytearray()
    
    # Header chunk: MThd, length=6, format=0, num_tracks=1, time_div
    result.extend(b'MThd')
    result.extend(struct.pack('>I', 6))
    result.extend(struct.pack('>HHH', 0, 1, ticks_per_quarter))
    
    # Track chunk
    result.extend(b'MTrk')
    result.extend(struct.pack('>I', len(track_data)))
    result.extend(track_data)
    
    return bytes(result)


def midi_to_c_array(filepath: str) -> Tuple[str, bytes]:
    """
    Convert a MIDI file to a merged Type 0 format as raw bytes.
    Returns: (song_name, midi_bytes)
    """
    # Get song name from filename (without extension)
    name = Path(filepath).stem
    # Sanitize for C identifier
    c_name = ''.join(c if c.isalnum() else '_' for c in name)
    if c_name[0].isdigit():
        c_name = '_' + c_name
    
    # Parse and merge
    format_type, ticks_per_quarter, tracks = parse_midi_file(filepath)
    merged = merge_tracks(tracks)
    midi_bytes = create_merged_midi(merged, ticks_per_quarter)
    
    return c_name, midi_bytes


def bytes_to_c_array(name: str, data: bytes, line_width: int = 16) -> str:
    """Format bytes as a C array declaration."""
    lines = [f"static const uint8_t {name}_data[] = {{"]
    
    for i in range(0, len(data), line_width):
        chunk = data[i:i+line_width]
        hex_values = ', '.join(f'0x{b:02X}' for b in chunk)
        lines.append(f"    {hex_values},")
    
    lines.append("};")
    return '\n'.join(lines)


def generate_header(songs: List[Tuple[str, bytes]], max_songs: int = 10) -> str:
    """Generate the complete demo_songs.h header file."""
    lines = [
        "/* Auto-generated by midi_to_c.py - DO NOT EDIT */",
        "#ifndef DEMO_SONGS_H",
        "#define DEMO_SONGS_H",
        "",
        "#include <stdint.h>",
        "#include <stddef.h>",
        "",
        f"#define DEMO_MAX_SONGS {max_songs}",
        f"#define DEMO_SONG_COUNT {len(songs)}",
        "",
        "typedef struct {",
        "    const char *name;",
        "    const uint8_t *data;",
        "    size_t size;",
        "} demo_song_t;",
        "",
    ]
    
    # Add each song's byte array
    for name, data in songs:
        lines.append(f"/* {name}: {len(data)} bytes */")
        lines.append(bytes_to_c_array(name, data))
        lines.append("")
    
    # Create song index array
    lines.append("static const demo_song_t demo_songs[DEMO_SONG_COUNT] = {")
    for name, data in songs:
        # Convert C identifier back to display name (replace _ with space)
        display_name = name.replace('_', ' ')
        lines.append(f'    {{ "{display_name}", {name}_data, sizeof({name}_data) }},')
    lines.append("};")
    lines.append("")
    lines.append("#endif /* DEMO_SONGS_H */")
    
    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(
        description='Convert MIDI files to C byte arrays for Dodepan demo mode'
    )
    parser.add_argument('files', nargs='+', help='MIDI files to convert')
    parser.add_argument('-o', '--output', default='demo_songs.h',
                        help='Output header file (default: demo_songs.h)')
    parser.add_argument('-m', '--max-songs', type=int, default=10,
                        help='Maximum song slots (default: 10)')
    
    args = parser.parse_args()
    
    songs = []
    for filepath in args.files:
        if not os.path.exists(filepath):
            print(f"Warning: File not found: {filepath}")
            continue
        
        try:
            name, data = midi_to_c_array(filepath)
            songs.append((name, data))
            print(f"Converted: {filepath} -> {name} ({len(data)} bytes)")
        except Exception as e:
            print(f"Error processing {filepath}: {e}")
    
    if not songs:
        print("No songs converted!")
        return 1
    
    if len(songs) > args.max_songs:
        print(f"Warning: {len(songs)} songs exceeds max ({args.max_songs}), truncating")
        songs = songs[:args.max_songs]
    
    header = generate_header(songs, args.max_songs)
    
    with open(args.output, 'w') as f:
        f.write(header)
    
    print(f"\nGenerated: {args.output}")
    print(f"Total songs: {len(songs)}")
    total_size = sum(len(data) for _, data in songs)
    print(f"Total size: {total_size} bytes ({total_size/1024:.1f} KB)")
    
    return 0


if __name__ == '__main__':
    exit(main())

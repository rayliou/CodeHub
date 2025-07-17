#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import glob
from pathlib import Path

def detect_encoding(filename):
    """
    Try to detect if a filename is in CP936 encoding
    Returns True if it appears to be CP936, False if it's already UTF-8
    """
    try:
        # Try to decode as UTF-8
        filename.encode('utf-8').decode('utf-8')
        return False  # Already UTF-8
    except UnicodeError:
        try:
            # Try to decode as CP936
            filename.encode('cp936').decode('cp936')
            return True  # Appears to be CP936
        except UnicodeError:
            # If neither works, assume it's already UTF-8
            return False

def convert_cp936_to_utf8(filename):
    """
    Convert a filename from CP936 to UTF-8 encoding
    """
    try:
        # First, try to encode as CP936 and then decode as UTF-8
        # This is a common approach for converting between encodings
        cp936_bytes = filename.encode('cp936', errors='ignore')
        utf8_filename = cp936_bytes.decode('utf-8', errors='ignore')
        return utf8_filename
    except Exception as e:
        print(f"Error converting {filename}: {e}")
        return filename

def rename_mp3_files():
    """
    Rename all MP3 files from CP936 to UTF-8 encoding
    """
    # Get all MP3 files in current directory
    mp3_files = glob.glob("*.mp3")
    
    if not mp3_files:
        print("No MP3 files found in current directory")
        return
    
    print(f"Found {len(mp3_files)} MP3 files")
    
    renamed_count = 0
    
    for filename in mp3_files:
        print(f"Processing: {filename}")
        
        # Check if filename needs conversion
        if detect_encoding(filename):
            print(f"  Detected CP936 encoding, converting to UTF-8...")
            
            # Convert filename
            new_filename = convert_cp936_to_utf8(filename)
            
            if new_filename != filename:
                try:
                    # Rename the file
                    os.rename(filename, new_filename)
                    print(f"  Renamed: {filename} -> {new_filename}")
                    renamed_count += 1
                except OSError as e:
                    print(f"  Error renaming {filename}: {e}")
            else:
                print(f"  No conversion needed for {filename}")
        else:
            print(f"  Already in UTF-8 encoding: {filename}")
    
    print(f"\nRenamed {renamed_count} files")

if __name__ == "__main__":
    print("MP3 Filename Encoding Converter (CP936 to UTF-8)")
    print("=" * 50)
    
    # Check if we're in the right directory
    current_dir = os.getcwd()
    print(f"Current directory: {current_dir}")
    
    # Confirm before proceeding
    response = input("\nDo you want to proceed with renaming MP3 files? (y/N): ")
    if response.lower() in ['y', 'yes']:
        rename_mp3_files()
    else:
        print("Operation cancelled.") 
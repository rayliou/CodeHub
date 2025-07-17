#!/bin/bash

#	sudo apt -y install ffmpeg cuetools shntool lame

# Check if required tools are installed and install if missing
function check_dependencies() {
    if ! command -v cuebreakpoints &> /dev/null; then
        echo "Missing required packages, installing..."
        sudo apt update
        sudo apt -y install ffmpeg cuetools shntool lame
        
        if [ $? -ne 0 ]; then
            echo "Error: Failed to install required packages"
            exit 1
        fi
        
        echo "Successfully installed packages"
    fi
}

# Check dependencies at startup
check_dependencies

function clean_all {
    fdfind --hidden  --no-ignore  '.*\.(mp3|wav)$' | xargs rm -vf
}

function ape_to_wav() {
    # Detect and standardize APE and CUE filenames
    local ape_file=$(find . -maxdepth 1 -name "*.ape" | head -1)
    local cue_file=$(find . -maxdepth 1 -name "*.cue" | head -1)

    if [ -z "$ape_file" ]; then
        echo "Error: No APE file found in current directory"
        exit 1
    fi
    
    if [ -z "$cue_file" ]; then
        echo "Error: No CUE file found in current directory"
        exit 1
    fi
    
    # Rename APE file to CDImage.ape if needed
    if [ "$ape_file" != "./CDImage.ape" ]; then
        echo "Renaming $(basename "$ape_file") to CDImage.ape"
        mv "$ape_file" CDImage.ape
    fi
    
    # Update CUE file to reference CDImage.ape and rename to CDImage.cue
    if [ "$cue_file" != "./CDImage.cue" ]; then
        echo "Updating CUE file references and renaming to CDImage.cue"
        # Update the FILE line in the CUE file to reference CDImage.ape
        sed -i 's/^FILE ".*\.ape"/FILE "CDImage.ape"/' "$cue_file"
        mv "$cue_file" CDImage.cue
    else
        # Even if the CUE file is already named correctly, update its contents
        echo "Updating CUE file references to CDImage.ape"
        sed -i 's/^FILE ".*\.ape"/FILE "CDImage.ape"/' CDImage.cue
    fi
    ffmpeg -i CDImage.ape CDImage.wav
    if [ $? -ne 0 ]; then
        echo "Error: Failed to convert APE to WAV"
        exit 1
    fi
}

function split_wav() {
    shnsplit -o wav -f CDImage.cue -t "%n-%t-%a" CDImage.wav
    if [ $? -ne 0 ]; then
        echo "Error: Failed to split WAV"
        exit 1
    fi
}


function wav_to_mp3() {
    find . -name '[0-9]*.wav' -print0 \
        | xargs -0 -I{} sh -c 'basename_file=$(basename "{}"); lame --preset extreme "{}" "${basename_file%.wav}.mp3"' 

    if [ $? -ne 0 ]; then
        echo "Error: Failed to convert WAV to MP3"
        exit 1
    fi
}

function cuetag() {
    find . -name '[0-9]*.mp3' -print0 \
        | xargs -0 -I{} basename "{}" \
        | sort -n \
        | xargs -I{} find . -name "{}" -print0 \
        | xargs -0 cuetag CDImage.cue
    if [ $? -ne 0 ]; then
        echo "Error: Failed to apply CUE tags"
        exit 1
    fi
}

function create_m3u8_playlist() {
    PLAYLIST_NAME="$(basename "$(pwd)").m3u8"
    echo "#EXTM3U" > "$PLAYLIST_NAME"
    find . -name '[0-9]*.mp3' -print0 \
        | xargs -0 -I{} basename "{}" \
        | sort -n  >> "$PLAYLIST_NAME"
}
function cleanup_files {
    # Check if there are MP3 files and M3U8 playlist before cleaning
    if [ ! -f "$(basename "$(pwd)").m3u8" ]; then
        echo "Warning: M3U8 playlist not found, skipping cleanup"
        exit 1
    fi

    if ! find . -name '[0-9]*.mp3' -print -quit | grep -q .; then
        echo "Warning: No MP3 files found, skipping cleanup"
        exit 1
    fi
    
    echo "MP3 files and M3U8 playlist found, proceeding with cleanup"
    fdfind --hidden  --no-ignore  '.*\.(jpg|cue|ape|doc|wav)$' | xargs rm -vf
}
# Parse command line arguments to determine which steps to run
steps_to_run="$1"

# Show usage if no argument provided
if [ -z "$steps_to_run" ]; then
    echo "Usage: $0 <steps>"
    echo "  steps: Step range (e.g., '0-4'), comma-separated (e.g., '2,3'), or single step (e.g., '2')"
    echo "  Step 0: Clean all files"
    echo "  Step 1: Convert APE to WAV"
    echo "  Step 2: Split WAV using CUE"
    echo "  Step 3: Convert WAV to MP3"
    echo "  Step 4: Apply CUE tags"
    echo "  Step 5: Create M3U8 playlist"
    echo "  Step 6: Cleanup files"
    exit 1
fi

# Function to check if a step should be run
should_run_step() {
    local step=$1
    
    # Handle range format (e.g., "0-4", "3-4")
    if [[ "$steps_to_run" =~ ^[0-9]+-[0-9]+$ ]]; then
        local start=$(echo "$steps_to_run" | cut -d'-' -f1)
        local end=$(echo "$steps_to_run" | cut -d'-' -f2)
        if [ "$step" -ge "$start" ] && [ "$step" -le "$end" ]; then
            return 0
        fi
    # Handle comma-separated format (e.g., "2,3")
    elif [[ "$steps_to_run" =~ , ]]; then
        IFS=',' read -ra STEPS <<< "$steps_to_run"
        for s in "${STEPS[@]}"; do
            if [ "$s" -eq "$step" ]; then
                return 0
            fi
        done
    # Handle single step
    elif [ "$steps_to_run" -eq "$step" ] 2>/dev/null; then
        return 0
    fi
    
    return 1
}

# Step 0: Clean all
if should_run_step 0; then
    echo "Running step 0: clean_all"
    clean_all
fi

# Step 1: Convert APE to WAV
if should_run_step 1; then
    echo "Running step 1: ape_to_wav"
    ape_to_wav
fi

# Step 2: Split WAV
if should_run_step 2; then
    echo "Running step 2: split_wav"
    split_wav
fi

# Step 3: Convert WAV to MP3
if should_run_step 3; then
    echo "Running step 3: wav_to_mp3"
    wav_to_mp3
fi

# Step 4: Apply cue tags
if should_run_step 4; then
    echo "Running step 4: cuetag"
    cuetag
fi

# Step 5: Create M3U8 playlist
if should_run_step 5; then
    echo "Running step 5: create_m3u8_playlist"
    create_m3u8_playlist
fi

# Step 6: Cleanup files
if should_run_step 6; then
    echo "Running step 6: cleanup_files"
    cleanup_files
fi
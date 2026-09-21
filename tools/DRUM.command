#!/bin/bash
#
# DRUM.command — hands-free DRUM factory flashing for the production bench
#
# Double-click on macOS, or use the Desktop launcher on the Linux bench (the
# production-setup repo's setup.sh / setup-linux.sh create those). Works on
# both; only the BOOTSEL volume detection differs per OS.
#
# Watches for an RP2350 in BOOTSEL mode and acts on its volume label:
#   RP2350    blank chip  → program the white-label OTP (drum/white-label.json,
#                           label becomes DRUMBOOT) and reboot it back into BOOTSEL
#   DRUMBOOT  white-labelled → flash + verify the factory image, then run it
#
# So a blank board goes RP2350 → DRUMBOOT → running firmware without any
# key presses. After each step the script waits for the volume to disappear
# before looking again, so a board is never handled twice (and a failed
# board is not retried until it is unplugged and plugged in again).
#
# Requires picotool on PATH (and on Linux, udev rules granting USB access).

# Firmware: the factory image is a GitHub release asset, not tracked in git.
# The bench setup script (setup.sh / setup-linux.sh in the production-setup
# repo) downloads the latest release into releases/ and writes
# releases/latest.env with DRUM_FIRMWARE_VERSION and DRUM_FIRMWARE.
FIRMWARE_ENV="releases/latest.env"

# Any failure before the loop starts must hold the window open, otherwise
# a double-click just flashes a Terminal window and closes it.
die() {
    echo "Error: $1" >&2
    shift
    for line in "$@"; do echo "$line" >&2; done
    echo ""
    read -s -n 1 -p "Press any key to close this window."
    echo ""
    exit 1
}

# This script lives in the repo it needs (tools/), so locate the repo root
# from its own path rather than a hardcoded checkout location.
cd "$(dirname "${BASH_SOURCE[0]}")/.." || die "Could not enter $(dirname "${BASH_SOURCE[0]}")/.."

command -v picotool >/dev/null || die "picotool is not installed or not on PATH."

[ -f "$FIRMWARE_ENV" ] || die "$FIRMWARE_ENV not found in $(pwd)." \
    "Run the bench setup script to download the latest release."
. "$FIRMWARE_ENV"
FIRMWARE="releases/$DRUM_FIRMWARE"
[ -f "$FIRMWARE" ] || die "Firmware $FIRMWARE not found in $(pwd)." \
    "Re-run the bench setup script to download the latest release."

# BOOTSEL volume detection. macOS mounts the volume by label under /Volumes;
# INFO_UF2.TXT is served by the bootrom, so checking for it rules out a stale
# empty mount-point directory. On Linux udev symlinks the block device by FAT
# label as soon as the board enumerates, whether or not the desktop mounts it.
case "$(uname -s)" in
    Darwin)
        BLANK_VOLUME="/Volumes/RP2350"
        LABELLED_VOLUME="/Volumes/DRUMBOOT"
        volume_present() { [ -f "$1/INFO_UF2.TXT" ]; }
        ;;
    Linux)
        BLANK_VOLUME="/dev/disk/by-label/RP2350"
        LABELLED_VOLUME="/dev/disk/by-label/DRUMBOOT"
        volume_present() { [ -e "$1" ]; }
        [ -f /etc/udev/rules.d/70-dato-bench.rules ] || \
            echo "Warning: bench udev rules not found — picotool may get 'access denied'. Run setup-linux.sh."
        ;;
    *)
        die "Unsupported OS: $(uname -s)"
        ;;
esac

# Quit (Q or Ctrl-C). On macOS close the Terminal window like the other
# bench scripts; on Linux the launcher's terminal closes on exit by itself.
finish() {
    echo ""
    command -v osascript >/dev/null && osascript -e 'tell application "Terminal" to close first window' &
    exit 0
}
trap finish INT

# Wait ~1 s. Also lets the operator quit with Q.
pause() {
    local key=""
    if [ -t 0 ]; then
        read -s -n 1 -t 1 key
        if [[ $key == "q" || $key == "Q" ]]; then
            finish
        fi
    else
        sleep 1
    fi
}

# wait_for_removal <path> — block until the BOOTSEL volume is gone
wait_for_removal() {
    while volume_present "$1"; do
        pause
    done
}

# Main Loop
clear
echo "Drum Factory Flashing Tool ($DRUM_FIRMWARE_VERSION)"
echo "Plug in boards in BOOTSEL mode. Press Q to quit."

FLASHED=0
while true; do
    if volume_present "$BLANK_VOLUME"; then
        echo ""
        echo "==> Blank RP2350 detected — programming white-label"
        sleep 1    # let enumeration settle before picotool claims the device
        if ./drum/build.sh --white-label && picotool reboot -f -u; then
            echo "    White-label done, rebooting into BOOTSEL as DRUMBOOT…"
        else
            printf '\a'
            echo "    !! White-label FAILED — unplug the board and try again."
        fi
        wait_for_removal "$BLANK_VOLUME"

    elif volume_present "$LABELLED_VOLUME"; then
        echo ""
        echo "==> DRUMBOOT detected — flashing factory image"
        sleep 1
        if picotool load -f --ignore-partitions -x -v "$FIRMWARE"; then
            FLASHED=$((FLASHED + 1))
            echo "    Flashed and verified. Boards done this session: $FLASHED"
        else
            printf '\a'
            echo "    !! Flash FAILED — unplug the board and try again."
        fi
        wait_for_removal "$LABELLED_VOLUME"
        echo "    Waiting for next board…"

    else
        pause
    fi
done

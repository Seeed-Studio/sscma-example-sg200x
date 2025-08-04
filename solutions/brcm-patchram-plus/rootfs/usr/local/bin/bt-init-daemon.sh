#!/bin/sh

DESC="Bluetooth Background Init"
HCI_DEV=ttyS1
HCI_TYPE=any
BAUDRATE=115200
PATCHRAM=/usr/local/bin/brcm_patchram_plus
FIRMWARE=/usr/lib/firmware/brcm/CYW43012C0_003.001.015.0196.0269.hcd

echo "[$DESC] Starting background init..."

# Step 1: Load firmware
echo "[$DESC] Loading firmware..."
$PATCHRAM --no2bytes --patchram "$FIRMWARE" /dev/$HCI_DEV
echo "[$DESC] Firmware loaded."

# Step 2: hciattach
echo "[$DESC] Attaching HCI..."
hciattach /dev/$HCI_DEV $HCI_TYPE $BAUDRATE flow 

# Step 3: Bring up
echo "[$DESC] Powering on HCI..."
hciconfig hci0 up
echo "[$DESC] Done."

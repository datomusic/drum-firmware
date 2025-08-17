# Partitioning

## Create the partition table for a fresh device
`picotool partition create partitions.json partitions.uf2`
`picotool load partitions.uf2`

## Upgrade the firmware
`picotool load new_firmware.uf2`
The new firmware automatically ends up in the non-active partition
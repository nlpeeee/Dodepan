/* Retain the legacy record for migration/rollback and reserve the last two
   physical 4 KiB sectors. The main FLASH region is also reduced by 8192. */
ASSERT(__flash_binary_end <= 0x101ff000, "Firmware overlaps legacy settings sector")
ASSERT(__flash_binary_end <= 0x103fe000, "Firmware overlaps settings journal")

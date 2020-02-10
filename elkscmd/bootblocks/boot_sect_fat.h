//------------------------------------------------------------------------------
// Code and data structures for booting ELKS from a FAT12/16 filesystem
//------------------------------------------------------------------------------

// boot_sect.S will use these macros if configured to be a FAT12/16 bootloader

.macro FAT_BPB
#if defined(CONFIG_IMG_FD360)
	.set FAT_SEC_PER_CLUS, 2
	.set FAT_ROOT_ENT_CNT, 112
	.set FAT_TOT_SEC, 720
	.set FAT_MEDIA_BYTE, 0xfd
	.set FAT_TABLE_SIZE, 2
	.set FAT_SEC_PER_TRK, 9
	.set FAT_NUM_HEADS, 2
#elif defined(CONFIG_IMG_FD720)
	.set FAT_SEC_PER_CLUS, 2
	.set FAT_ROOT_ENT_CNT, 112
	.set FAT_TOT_SEC, 1440
	.set FAT_MEDIA_BYTE, 0xf9
	.set FAT_TABLE_SIZE, 3
	.set FAT_SEC_PER_TRK, 9
	.set FAT_NUM_HEADS, 2
#elif defined(CONFIG_IMG_FD1200)
	.set FAT_SEC_PER_CLUS, 1
	.set FAT_ROOT_ENT_CNT, 224
	.set FAT_TOT_SEC, 2400
	.set FAT_MEDIA_BYTE, 0xf9
	.set FAT_TABLE_SIZE, 7
	.set FAT_SEC_PER_TRK, 15
	.set FAT_NUM_HEADS, 2
#elif defined(CONFIG_IMG_FD1440)
	.set FAT_SEC_PER_CLUS, 1
	.set FAT_ROOT_ENT_CNT, 224
	.set FAT_TOT_SEC, 2880
	.set FAT_MEDIA_BYTE, 0xf0
	.set FAT_TABLE_SIZE, 9
	.set FAT_SEC_PER_TRK, 18
	.set FAT_NUM_HEADS, 2
#else
	.error "Unknown disk medium!"
#endif

// BIOS Parameter Block (BPB) for FAT filesystems, as laid out in
//
//	Microsoft Corporation.  _Microsoft Extensible Firmware Initiative
//	FAT32 File System Specification: FAT: General Overview of On-Disk
//	Format_.  2000.
//
// or alternatively
//
//	Microsoft Corporation.  _Microsoft FAT Specification_.  2004.

	.global sect_max
	.global head_max

bs_oem_name:				// OEM name
	.ascii "ELKSFAT1"
bpb_byts_per_sec:			// Bytes per sector
	.word 0x200
bpb_sec_per_clus:			// Sectors per cluster
	.byte FAT_SEC_PER_CLUS
bpb_rsvd_sec_cnt:			// Reserved sectors, including this one
	.word 1
bpb_num_fats:				// Number of duplicate file allocation
	.byte 2				// tables
bpb_root_ent_cnt:			// Number of root directory entries
	.word FAT_ROOT_ENT_CNT
bpb_tot_sec_16:				// Total number of sectors, 16-bit
	.word FAT_TOT_SEC
bpb_media:				// Media byte
	.byte FAT_MEDIA_BYTE
bpb_fat_sz_16:				// Sectors per FAT
	.word FAT_TABLE_SIZE
bpb_sec_per_trk:			// Sectors per track
sect_max:
	.word FAT_SEC_PER_TRK
bpb_num_heads:				// Number of heads
head_max:
	.word FAT_NUM_HEADS
bpb_hidd_sec:				// Hidden sectors
	.long 0
bpb_tot_sec_32:				// Total number of sectors, 32-bit
	.long 0
.endm

.macro FAT_LOAD_AND_RUN_KERNEL
	.set buf, entry + 0x200

	// Load the first sector of the root directory
	movb bpb_num_fats,%al
	cbtw
	mulw bpb_fat_sz_16
	addw bpb_rsvd_sec_cnt,%ax
	push %ax
	inc %dx				// Assume DX was 0 (from mulw); if
					// DX != 0 then we are in trouble
					// anyway
	mov $buf,%cx
	mov %cx,%si
	push %ss
	call disk_read

	// See if the first directory entry is /linux; bail out if not
	mov $kernel_name,%di		// SI = buf
	mov $0xb,%cx
	repz
	cmpsb
	jnz no_system

	// Load consecutive sectors starting from the first file cluster
	// Calculate starting sector
	mov (0x1a-0xb)(%si),%ax		// First cluster number
	dec %ax				// Compute no. of sectors from the
	dec %ax				// disk data area start
	mov bpb_sec_per_clus,%cl	// CH = 0
	mul %cx
	pop %bx				// BX = root directory logical sector
					// we calculated before
	add %ax,%bx
	mov bpb_root_ent_cnt,%ax	// Then account for the sectors
	add $0xf,%ax			// holding the root directory
	mov $4,%cl
	shr %cl,%ax
	add %bx,%ax
	push %ax

	// Load 1 sector first --- this has metadata on the sector counts for
	// the setup code (to be at ELKS_INITSEG) and for the kernel (at
	// ELKS_SYSSEG)
	inc %dx				// Again assume DX was 0 (from mul)
	mov $ELKS_INITSEG,%bx
	mov %bx,%es
	call _disk_read_at_bx_0

	// Check for ELKS magic number
	mov $0x1E6,%di
	mov $0x4C45,%ax
	scasw
	jnz not_elks
	mov $0x534B,%ax
	scasw
	jz boot_it

not_elks:
	mov $ERR_BAD_SYSTEM,%al
	jmp _except

boot_it:
	// Load the setup code
	cwtd				// DX = 0
	pop %ax
	inc %ax
	mov %ax,%si			// SI = starting sector of setup code
	mov %es:(0x1F1-0x1E6-4)(%di),%dl
	add %dx,%si			// Precompute starting sector of kernel
	mov $ELKS_INITSEG+0x20,%bx
	call _disk_read_at_bx_0

	// Load the kernel
	xchg %ax,%si			// AX = starting sector of kernel
	mov %es:(0x1F4-0x1E6-4)(%di),%dx
	add $31,%dx
	mov $5,%cl
	shr %cl,%dx
	mov $ELKS_SYSSEG,%bx
	call _disk_read_at_bx_0

	// w00t!
	push %es
	pop %ds
	push %es
	pop %ss
	mov $0x4000-12,%sp
	ljmp $ELKS_INITSEG+0x20,$0

kernel_name:
	.ascii "LINUX      "
.endm

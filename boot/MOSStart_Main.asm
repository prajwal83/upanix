;	Upanix - An x86 based Operating System
;	Copyright (C) 2011 'Prajwala Prabhakar' 'srinivasa_prajwal@yahoo.co.in'
;	                                                                        
;	This program is free software: you can redistribute it and/or modify
;	it under the terms of the GNU General Public License as published by
;	the Free Software Foundation, either version 3 of the License, or
;	(at your option) any later version.
;	                                                                        
;	This program is distributed in the hope that it will be useful,
;	but WITHOUT ANY WARRANTY; without even the implied warranty of
;	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;	GNU General Public License for more details.
;	                                                                        
;	You should have received a copy of the GNU General Public License
;	along with this program.  If not, see <http://www.gnu.org/licenses/

[BITS 32]
JMP _start

GDT_BASE EQU 520 * 1024
LDT_BASE EQU 524 * 1024

SYS_TSS_BASE	EQU 536 * 1024
USER_TSS_BASE	EQU 540 * 1024
INT_TSS_BASE_SV	EQU 544 * 1024
INT_TSS_BASE_PF	EQU 548 * 1024

GDT_DESC_SIZE	EQU		8
SYS_LINEAR_SEL	EQU		GDT_DESC_SIZE * 1
SYS_CODE_SEL	EQU		GDT_DESC_SIZE * 2
SYS_DATA_SEL	EQU		GDT_DESC_SIZE * 3
SYS_TSS_SEL		EQU		GDT_DESC_SIZE * 4
USER_TSS_SEL	EQU		GDT_DESC_SIZE * 5
INT_TSS_SEL_SV	EQU		GDT_DESC_SIZE * 13
INT_TSS_SEL_PF	EQU		GDT_DESC_SIZE * 14
CALL_GATE_SEL	EQU		GDT_DESC_SIZE * 15
INT_GATE_SEL	EQU		GDT_DESC_SIZE * 16

MOS_KERNEL_BASE_ADDRESS	EQU	0 ; 1 * 1024 * 1024
KERNEL_STACK			EQU 3 * 1024 * 1024 

MB_MAGIC EQU 0x1BADB002
MB_FLAGS EQU 0x00010003
MB_CHECKSUM EQU -(MB_MAGIC + MB_FLAGS)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; GRUB MULTIBOOT HEADER
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ALIGN 4
MULTIBOOT_HEADER:
MAGIC:			DD		MB_MAGIC
FLAGS			DD		MB_FLAGS
CHECKSUM		DD		MB_CHECKSUM
HEADER_ADDR		DD		MOS_KERNEL_BASE_ADDRESS + MULTIBOOT_HEADER
LOAD_ADDR		DD		MOS_KERNEL_BASE_ADDRESS
LOAD_END_ADDR	DD		0
BSS_END_ADDR	DD		0
ENTRY_ADDR		DD		MOS_KERNEL_BASE_ADDRESS + _start

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

[GLOBAL _start]

[GLOBAL GLOBAL_DATA_SEGMENT_BASE]
[GLOBAL SYS_CODE_SELECTOR]
[GLOBAL SYS_LINEAR_SELECTOR]
[GLOBAL SYS_DATA_SELECTOR]
[GLOBAL SYS_TSS_SELECTOR]
[GLOBAL USER_TSS_SELECTOR]
[GLOBAL INT_TSS_SELECTOR_SV]
[GLOBAL INT_TSS_SELECTOR_PF]
[GLOBAL CALL_GATE_SELECTOR]
[GLOBAL INT_GATE_SELECTOR]
[GLOBAL SYS_TSS_LINEAR_ADDRESS]
[GLOBAL USER_TSS_LINEAR_ADDRESS]
[GLOBAL INT_TSS_LINEAR_ADDRESS_SV]
[GLOBAL INT_TSS_LINEAR_ADDRESS_PF]
[GLOBAL MULTIBOOT_INFO_ADDR]
[GLOBAL CR0_CONTENT]
[GLOBAL CO_PROC_FPU_TYPE]

[EXTERN FPU_INIT]
[EXTERN MOSMain]

_start:

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	; COPY MULTI BOOT INFO TO KERNEL ADDRESS
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	MOV ESI, EBX
	MOV EDI, MOS_KERNEL_BASE_ADDRESS + MULTIBOOT_INFO_ADDR
	MOV ECX, 40
	CLD
	REP MOVSB

	; SET Temporary GDT

	LGDT [MOS_KERNEL_BASE_ADDRESS + GDTR_TEMP]

	MOV AL, 11111111B               ; SELECT TO MASK OF ALL IRQ'S
	OUT 0X21, AL                    ; WRITE IT TO THE PIC CONTROLLER

	MOV AX, SYS_DATA_SEL_TEMP
	MOV DS, AX
	MOV FS, AX
	MOV GS, AX
	MOV SS, AX
	MOV ESP, KERNEL_STACK
	MOV AX, SYS_LINEAR_SEL_TEMP
	MOV ES, AX

	; SET UP REAL GDT
		
	XOR EBX,EBX
	MOV EBX, MOS_KERNEL_BASE_ADDRESS ; BX=SEGMENT
	
	MOV EAX,EBX
	
	MOV WORD [GDT_CS + 2],AX               ; SET BASE ADDRESS OF 32-BIT SEGMENTS
	MOV WORD [GDT_CS1 + 2],AX               ; SET BASE ADDRESS OF 32-BIT SEGMENTS
	MOV WORD [GDT_DS + 2],AX
	MOV WORD [GDT_DS1 + 2],AX
	
	SHR EAX,16
	
	MOV BYTE [GDT_CS + 4],AL
	MOV BYTE [GDT_CS1 + 4],AL
	MOV BYTE [GDT_DS + 4],AL
	MOV WORD [GDT_DS1 + 2],AX

	MOV BYTE [GDT_CS + 7],AH
	MOV BYTE [GDT_CS1 + 7],AH
	MOV BYTE [GDT_DS + 7],AH
	MOV WORD [GDT_DS1 + 2],AX

	; SYS_TSS INIT
	MOV EAX, SYS_TSS_BASE
	MOV WORD [GDT_SYS_TSS + 2], AX
	SHR EAX, 16
	MOV BYTE [GDT_SYS_TSS + 4], AL
	MOV BYTE [GDT_SYS_TSS + 7], AH

	; USER_TSS INIT
	MOV EAX, USER_TSS_BASE
	MOV WORD [GDT_USER_TSS + 2], AX
	SHR EAX, 16
	MOV BYTE [GDT_USER_TSS + 4], AL
	MOV BYTE [GDT_USER_TSS + 7], AH

	; INT_TSS INIT FOR INVALID TSS EX
	MOV EAX, INT_TSS_BASE_SV
	MOV WORD [GDT_INT_TSS_SV + 2], AX
	SHR EAX, 16
	MOV BYTE [GDT_INT_TSS_SV + 4], AL
	MOV BYTE [GDT_INT_TSS_SV + 7], AH

	; INT_TSS INIT FOR PAGE FAULT EX
	MOV EAX, INT_TSS_BASE_PF
	MOV WORD [GDT_INT_TSS_PF + 2], AX
	SHR EAX, 16
	MOV BYTE [GDT_INT_TSS_PF + 4], AL
	MOV BYTE [GDT_INT_TSS_PF + 7], AH

	XOR EBX,EBX
	MOV EBX,LDT_BASE
	MOV EAX,EBX
	MOV WORD [GDT_LDT_DESC + 2],AX               ; SET BASE ADDRESS OF 32-BIT SEGMENTS
	SHR EAX,16
	MOV BYTE [GDT_LDT_DESC + 4],AL
	MOV BYTE [GDT_LDT_DESC + 7],AH

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	; COPY REAL GDT TO PROPER POSITION
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	MOV ESI, GDT
	MOV EDI, GDT_BASE
	MOV ECX, GDT_LENGTH
	CLD
	REP MOVSB

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	; LOAD MOS KERNEL TO 1MB LOCATION IN RAM
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	LGDT [GDTR]

	JMP SYS_CODE_SEL:_MosMain

_MosMain:

	CLI

	MOV AX, SYS_DATA_SEL
	MOV DS, AX
	MOV SS, AX
	MOV FS, AX
	MOV GS, AX
	MOV ESP, KERNEL_STACK
	MOV AX, SYS_LINEAR_SEL
	MOV ES, AX

	MOV DWORD [GLOBAL_DATA_SEGMENT_BASE], MOS_KERNEL_BASE_ADDRESS

	MOV WORD [SYS_CODE_SELECTOR], SYS_CODE_SEL
	MOV WORD [SYS_LINEAR_SELECTOR], SYS_LINEAR_SEL
	MOV WORD [SYS_DATA_SELECTOR], SYS_DATA_SEL
	MOV WORD [SYS_TSS_SELECTOR], SYS_TSS_SEL
	MOV WORD [USER_TSS_SELECTOR], USER_TSS_SEL
	MOV WORD [INT_TSS_SELECTOR_SV], INT_TSS_SEL_SV
	MOV WORD [INT_TSS_SELECTOR_PF], INT_TSS_SEL_PF
	MOV WORD [CALL_GATE_SELECTOR], CALL_GATE_SEL
	MOV WORD [INT_GATE_SELECTOR], INT_GATE_SEL

	MOV DWORD [SYS_TSS_LINEAR_ADDRESS], SYS_TSS_BASE
	MOV DWORD [USER_TSS_LINEAR_ADDRESS], USER_TSS_BASE
	MOV DWORD [INT_TSS_LINEAR_ADDRESS_SV], INT_TSS_BASE_SV
	MOV DWORD [INT_TSS_LINEAR_ADDRESS_PF], INT_TSS_BASE_PF

	MOV EAX,CR0
	MOV DWORD [CR0_CONTENT], EAX

	CALL FPU_INIT

	MOV AX, SYS_TSS_SEL
	LTR AX

	;***********************************
	CALL MOSMain
	;***********************************

	HLT

GLOBAL_DATA_SEGMENT_BASE:
		DD 0
SYS_CODE_SELECTOR:
		DD 0
SYS_LINEAR_SELECTOR:
		DD 0
SYS_DATA_SELECTOR:
		DD 0
SYS_TSS_SELECTOR:
		DD 0
USER_TSS_SELECTOR:
		DD 0
INT_TSS_SELECTOR_SV:
		DD 0
INT_TSS_SELECTOR_PF:
		DD 0
CALL_GATE_SELECTOR:
		DD 0
INT_GATE_SELECTOR:
		DD 0

SYS_TSS_LINEAR_ADDRESS:
		DD 0
USER_TSS_LINEAR_ADDRESS:
		DD 0
INT_TSS_LINEAR_ADDRESS_SV:
		DD 0
INT_TSS_LINEAR_ADDRESS_PF:
		DD 0
CR0_CONTENT:
		DD 0
CO_PROC_FPU_TYPE:
		DB 0

MULTIBOOT_INFO_ADDR:
MULTIBOOT_FLAGS:			DD 0
MEM_LOWER:					DD 0
MEM_UPPER:					DD 0
BOOT_DEVICE:				DD 0
CMDLINE:					DD 0
MODS_COUNT:					DD 0
MODS_ADDR:					DD 0

AOUT_TABSIZE_ELF_NUM:		DD 0
AOUT_STRSIZE_ELF_SIZE:		DD 0
AOUT_ADDR_ELF_ADDR:			DD 0
AOUT_RESERVED_ELF_SHNDX:	DD 0

MMAP_LENGTH:				DD 0
MMAP_ADDR:					DD 0

;;;;;;;;;;;;;;;;;;;;
; REAL GDT 
;;;;;;;;;;;;;;;;;;;;

GDTR:	DW GDT_END - GDT - 1		; GDT LIMIT
		DD GDT_BASE

GDTR_TEMP:	DW GDT_END_TEMP - GDT_TEMP - 1			; GDT LIMIT
			DD MOS_KERNEL_BASE_ADDRESS + GDT_TEMP	; (GDT BASE GETS SET ABOVE)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;	GLOBAL DESCRIPTOR TABLE (GDT)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; NULL DESCRIPTOR
GDT:	DW 0			; LIMIT 15:0
		DW 0			; BASE 15:0
		DB 0			; BASE 23:16
		DB 0			; TYPE
		DB 0			; LIMIT 19:16, FLAGS
		DB 0			; BASE 31:24

; LINEAR DATA SEGMENT DESCRIPTOR
	DW 0xFFFF		; LIMIT 0XFFFFF
	DW 0			; BASE 0
	DB 0
	DB 0x92			; PRESENT, RING 0, DATA, EXPAND-UP, WRITABLE
	DB 0xCF                 ; PAGE-GRANULAR, 32-BIT
	DB 0

; CODE SEGMENT DESCRIPTOR
GDT_CS:  
	DW 0xFFFF               ; LIMIT 0XFFFFF
	DW 0			; (BASE GETS SET ABOVE)
	DB 0
	DB 0x9A			; PRESENT, RING 0, CODE, NON-CONFORMING, READABLE
	DB 0xCF                 ; PAGE-GRANULAR, 32-BIT
	DB 0

; DATA SEGMENT DESCRIPTOR
GDT_DS:
	DW 0xFFFF               ; LIMIT 0XFFFFF
	DW 0			; (BASE GETS SET ABOVE)
	DB 0
	DB 0x92			; PRESENT, RING 0, DATA, EXPAND-UP, WRITABLE
	DB 0xCF                 ; PAGE-GRANULAR, 32-BIT
	DB 0

GDT_SYS_TSS:		; 0x20
	DW 103
	DW 0			; SET ABOVE
	DB 0
	DB 0x89			; PRESENT, RING 0, 32-BIT AVAILABLE TSS
	DB 0
	DB 0

GDT_USER_TSS:		;0x28
	DW 103
	DW 0			; SET ABOVE
	DB 0
	DB 10001001b	; PRESENT, RING 0, 32-BIT AVAILABLE TSS
	DB 0
	DB 0

GDT_TASK_GATE1:		;0x30
	DW 0
	DW 0x28			; SET ABOVE
	DB 0
	DB 11100101b	; PRESENT, RING 0, 32-BIT AVAILABLE TSS
	DB 0
	DB 0

; CODE SEGMENT DESCRIPTOR
GDT_CS1:  
	DW 0xFFFF               ; LIMIT 0XFFFFF
	DW 0			; (BASE GETS SET ABOVE)
	DB 0
	DB 0xFA			; PRESENT, RING 0, CODE, NON-CONFORMING, READABLE
	DB 0xCF                 ; PAGE-GRANULAR, 32-BIT
	DB 0

; DATA SEGMENT DESCRIPTOR
GDT_DS1:
	DW 0xFFFF               ; LIMIT 0XFFFFF
	DW 0			; (BASE GETS SET ABOVE)
	DB 0
	DB 0xF2			; PRESENT, RING 0, DATA, EXPAND-UP, WRITABLE
	DB 0xCF                 ; PAGE-GRANULAR, 32-BIT
	DB 0

; LINEAR DATA SEGMENT DESCRIPTOR
	DW 0xFFFF		; LIMIT 0XFFFFF
	DW 0			; BASE 0
	DB 0
	DB 0xF2			; PRESENT, RING 0, DATA, EXPAND-UP, WRITABLE
	DB 0xCF                 ; PAGE-GRANULAR, 32-BIT
	DB 0

GDT_LDT_DESC:
	DW 0xFF		; LIMIT 0XFFFFF
	DW 0			; BASE 0
	DB 0
	DB 0x82			; PRESENT, RING 0, DATA, EXPAND-UP, WRITABLE
	DB 0xC0                ; PAGE-GRANULAR, 32-BIT
	DB 0

; DATA SEGMENT DESCRIPTOR
GDT_DS_P1:
	DW 0xFFFF               ; LIMIT 0XFFFFF
	DW 0			; (BASE GETS SET ABOVE)
	DB 0
	DB 0xB2			; PRESENT, RING 0, DATA, EXPAND-UP, WRITABLE
	DB 0xCF                 ; PAGE-GRANULAR, 32-BIT
	DB 0

; DATA SEGMENT DESCRIPTOR
GDT_DS_P2:
	DW 0xFFFF               ; LIMIT 0XFFFFF
	DW 0			; (BASE GETS SET ABOVE)
	DB 0
	DB 0xD2			; PRESENT, RING 0, DATA, EXPAND-UP, WRITABLE
	DB 0xCF                 ; PAGE-GRANULAR, 32-BIT
	DB 0

GDT_INT_TSS_SV:		;
	DW 103
	DW 0			; SET ABOVE
	DB 0
	DB 10001001b	; PRESENT, RING 0, 32-BIT AVAILABLE TSS
	DB 0
	DB 0

GDT_INT_TSS_PF:		;
	DW 103
	DW 0			; SET ABOVE
	DB 0
	DB 10001001b	; PRESENT, RING 0, 32-BIT AVAILABLE TSS
	DB 0
	DB 0

GDT_CALL_GATE:
	DW 0
	DW 0			; SET ABOVE
	DB 0x00
	DB 11101100b	; PRESENT, RING 0, 32-BIT AVAILABLE TSS
	DB 0
	DB 0

GDT_INT_GATE:
	DW 0
	DW 0			; SET ABOVE
	DB 0x00
	DB 11101110b	; PRESENT, RING 0, 32-BIT AVAILABLE TSS
	DB 0
	DB 0

; CODE SEGMENT DESCRIPTOR
GDT_CS_16:  
	DW 0xFFFF               ; LIMIT 0XFFFFF
	DW 0			; 
	DB 0
	DB 0x9A			; PRESENT, RING 0, CODE, NON-CONFORMING, READABLE
	DB 0x00                 ; PAGE-GRANULAR, 32-BIT
	DB 0

; DATA SEGMENT DESCRIPTOR
GDT_DS_16:
	DW 0xFFFF               ; LIMIT 0XFFFFF
	DW 0			; (BASE GETS SET ABOVE)
	DB 0
	DB 0x92			; PRESENT, RING 0, DATA, EXPAND-UP, WRITABLE
	DB 0x00                 ; PAGE-GRANULAR, 32-BIT
	DB 0

GDT_END:

GDT_LENGTH EQU GDT_END - GDT


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;	TEMP GLOBAL DESCRIPTOR TABLE (GDT)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; NULL DESCRIPTOR
GDT_TEMP:	
		DW 0			; LIMIT 15:0
		DW 0			; BASE 15:0
		DB 0			; BASE 23:16
		DB 0			; TYPE
		DB 0			; LIMIT 19:16, FLAGS
		DB 0			; BASE 31:24

; LINEAR DATA SEGMENT DESCRIPTOR
SYS_LINEAR_SEL_TEMP	EQU	$-GDT_TEMP
	DW 0XFFFF		; LIMIT 0XFFFFF
	DW 0			; BASE 0
	DB 0
	DB 0X92			; PRESENT, RING 0, DATA, EXPAND-UP, WRITABLE
	DB 0XCF                 ; PAGE-GRANULAR, 32-BIT
	DB 0

; CODE SEGMENT DESCRIPTOR
SYS_CODE_SEL_TEMP	EQU	$-GDT_TEMP
GDT_TEMP_CS:
	DW 0XFFFF               ; LIMIT 0XFFFFF
	DW 0			; (BASE 1MB)
	DB 0x0
	DB 0X9A			; PRESENT, RING 0, CODE, NON-CONFORMING, READABLE
	DB 0XCF                 ; PAGE-GRANULAR, 32-BIT
	DB 0

; DATA SEGMENT DESCRIPTOR
SYS_DATA_SEL_TEMP	EQU	$-GDT_TEMP
GDT_TEMP_DS:
	DW 0XFFFF               ; LIMIT 0XFFFFF
	DW 0			; (BASE 1MB)
	DB 0x0
	DB 0X92			; PRESENT, RING 0, DATA, EXPAND-UP, WRITABLE
	DB 0XCF                 ; PAGE-GRANULAR, 32-BIT
	DB 0

GDT_END_TEMP:


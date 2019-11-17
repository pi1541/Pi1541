; --------------------
; - Constants
; --------------------

; - joystick port address

!if target = 20 {
joyport = $9111         ; joystick port 
ScreenCols = 22         ; Number of screen columns
curpos = $d3            ; Position of cursor on line (used to avoid screen scroll when print "quit")

}

!if target = 64 {
joyport = $dc00         ; joystick port 2 ($dc01 for port 1)
ScreenCols = 40         ; Number of screen columns
}


!if target = 16 { 
joyport = $FF08         ; joystick port (set #$fa before for port 1 or #$fd for port 2)
ScreenCols = 40         ; Number of screen columns
}

!if target = 128 {
joyport = $dc00         ; change me,  joystick port 
ScreenCols = 40         ; Number of screen columns
}


; - max. length for stored entries
entrylen = 17
; - max. length for shown entries
namelen = 16

; - some PETSCII chars
quotechar = $22
leftarrowchar = 95

; - Vic-20 Mega-Cart used locations and constants
MC_NVRamCheck        = $9c00  ; value #214 (CheckByte) means that prefences are stored from $9C01 to $9C3F
MC_CheckByte         = 214    ; Value to check if preference are stored on NVRAM
MC_ExternalBootDelay = $9c3f  ; 2 to 50 [default 5] (cycles to wait before to check for external boot program)
MC_SJLOAD_init       = $9c40  ; SJLOAD quick start jump (SYS40000)
MC_NvRAM_state       = $9d80  ; Set 3k/NvRAM (bit0 = 0, enabled) but it could be used to set 32k too in someway.
MC_LastCartEprom1    = $9c08
MC_LastCartEprom2    = $9c09
MC_LastCartExp3k     = $9c0a
MC_LastConfig        = $9c0b

; --------------------
; - kernal functions
; --------------------

!if target = 20 | target = 64 | target = 16 | target = 128 {
SETLFS = $ffba          ; a = filenr, x = unitnr, y = sec.addr/command (255|1)
SETNAM = $ffbd          ; a = namelen, x:y->name
OPEN = $ffc0            ; Open Vector [F40A] Open File
                        ; Preparing: SETLFS, SETNAM
CLOSE = $ffc3           ; x = filenr
LOAD = $ffd5            ; a = 0
SCNKEY = $ff9f
GETIN = $ffe4           ; ret: a = 0: no keys pressed, otherwise a = ASCII code
CHROUT = $ffd2          ; a = output char
PLOT = $fff0            ; x<->y
}


!if target = 20 {
CLRLINE = $ea8d     ; Clear Screen Line X

CHKIN   = $ffc6     ; Set Input [F2C7] Set Input Device
                    ; Preparing: OPEN
CHRIN   = $ffcf     ; Input Vector, chrin [F20E] Input a byte
                    ; Preparing: OPEN, CHKIN
CINT1   = $e518     ; Initialize I/O

PRNSTR  = $cb1e     ; print string in A/Y, 0 terminated
PRNINT  = $ddcd     ; print integer in A/X
CLEARSCREEN = $e55f
SHFLAG =  $28D      ; ($00 = None, $01 = Shift, $02 = CBM, $04 = CTRL or a sum of them). 
                    ; not used for the Vic-20 (auto or manual startmode method)
}

!if target = 64 {
PRNSTR  = $ab1e     ; print string in A/Y, 0 terminated
PRNINT  = $bdcd     ; print integer in A/X
CLEARSCREEN = $e544
SHFLAG =  $28d      ; ($00 = None, $01 = Shift, $02 = CBM, $04 = CTRL or a sum of them). 

CINT1   = $e518     ; Initialize I/O
INITV   = $e45b     ; initv Initialize Vectors
INTTCZ  = $e3a4     ; initcz Initialize BASIC RAM
SCRTCHF = $a644     ; scrtch Perform [new] FORCED and return

}

!if target = 128 {
;PRNSTR  = $ab1e     ; print string in A/Y, 0 terminated
;PRNINT  = $8e32     ; print integer in A/X
CLEARSCREEN = $c142
SHFLAG =  $d3      ; ($00 = None, $01 = Shift, $02 = CBM, $04 = CTRL or a sum of them). 

CINT    = $ff81     ; CINT - $ff81 - Initializes screen editor 
IOINIT  = $ff84     ; IOINIT - $ff84 - Initializes I/O devices

}

!if target = 16 {
PRNSTR  = $9088     ; print string in A/Y, 0 terminated
PRNINT  = $8b40     ; print integer in A/X
CLEARSCREEN = $d888
SHFLAG =  $543      ; ($00 = None, $01 = Shift, $02 = CBM, $04 = CTRL or a sum of them). 

CINT    = $ff81     ; CINT - $ff81 - Initializes screen editor 
IOINIT  = $ff84     ; IOINIT - $ff84 - Initializes I/O devices

SCRTCHF = $8a7b     ; scrtch Perform [new] FORCED and return
}

!if target = 20 | target = 64 | target = 16 | target = 128 {
LISTN   = $ffb1     ; Command Serial Bus LISTEN
UNLSN   = $ffae     ; Command Serial Bus UNLISTEN
SECOND	= $ff93		; Send secondary address for LISTEN
}



; --------------------
; - hw addresses
; --------------------

!if target = 20 {

!if vicmemcfg = 0 | vicmemcfg = 3 {
screen = $1E00          ; screen address
color = $9600           ; color ram address
}

!if vicmemcfg = 8 {
screen = $1000          ; screen address
color = $9400           ; color ram address
}

vicborder = $900F       ; border color register     <<--- The vic uses just one register for both border and background
vicbackgnd = $900F      ; background color register <<--- The vic uses just one register for both border and background
vicraster = $9004       ; raster compare register ???

start_memory_hi = $282  ; Start of memory (hi-byte) 
top_memory_hi = $284    ; Top of memory (hi-byte)
screen_mem_page = $288  ; Screen memory page start (hi-byte)

output_control = $9d    ;Direct=$80/RUN=0 

}

!if target = 64 | target = 128 {
screen = $0400          ; screen address
color = $d800           ; color ram address
vicborder = $d020       ; border color register
vicbackgnd = $d021      ; background color register
vicraster = $d012       ; raster compare register
}

!if target = 16 {
screen = $0C00          ; screen address
color = $0800           ; color ram address

vicborder = $FF19       ; border color register     
vicbackgnd = $FF15      ; background color register
vicraster = $FF0B       ; raster compare register ???

output_control = $9a    ;Direct=$80/RUN=0 

}

!if target = 128 {
	basicstartaddress_lo = $2d ;Pointer: Start of Basic lo byte
	basicstartaddress_hi = $2e ;Pointer: Start of Basic hi byte
;	variablesstartaddress_lo = $2d ;Pointer: Start of Basic lo byte
;	variablesstartaddress_hi = $2e ;Pointer: Start of Basic hi byte
} else {
;common for C64/Vic20/C16
	basicstartaddress_lo = $2b ;Pointer: Start of Basic lo byte
	basicstartaddress_hi = $2c ;Pointer: Start of Basic hi byte
	variablesstartaddress_lo = $2d ;Pointer: Start of Basic lo byte
	variablesstartaddress_hi = $2e ;Pointer: Start of Basic hi byte
}

status_word_st = $90    ; Status word ST 

!if target = 20 | target = 64 {
keybuf = $0277          ; keyboard buffer
keybuflen = $c6         ; keyboard buffer byte count
lastdevice = $ba        ; last used device number
}

!if target = 16 {
keybuf = $0527          ; keyboard buffer
keybuflen = $ef         ; keyboard buffer byte count
lastdevice = $ae        ; current device number
}

!if target = 128 {
keybuf = $034a          ; keyboard buffer
keybuflen = $d0         ; keyboard buffer byte count
lastdevice = $ba        ; last used device number
}




; --------------------
; - Zero page variables
; --------------------

!if target = 20 | target = 64 | target = 16 | target = 128 {

; current joystick state (1 = falling edge)
curjoy = $22

; temp variables
tmp  = $23
tmp2 = $24  ; it must be continuous with tmp Example $FD and $FE (used as pointer in some cases)
tmp3 = $25

; pointer to last of the table
tbllst  = $26
tbllsth = $27

; menustate, bits:
; 0 = top reached (disable up & page up)
; 1 = bottom reached
; 2 = last reached (disable down & page down)
; 7 = inside d64
menustate = $28

; length of diskname
disknamepetlen = $29

!if rememberpos = 1 {
; previous dir position
prevdirpos  = $57
prevdirposh = $58
}
}

!if target = 20 | target = 64 | target = 128 {

; - pointer to selected entry
selected  = $fb
selectedh = $fc

; - temp pointer
tmpptr = $fd
tmpptrh = $fe
}

!if target = 16 {

; - pointer to selected entry
selected  = $d8
selectedh = $d9

; - temp pointer
tmpptr = $da
tmpptrh = $db
}


; --------------------
; - Miscellaneus
; --------------------

!if target = 16 {
FkeyStoreArea = $332    ; 16 bytes ($332-341)  ; 264 series cassette buffer ($332-$3f2)
}

!if target = 128 {
FkeyStoreArea = $110    ; 20 bytes ($110-123)  ; DOS / Print Using work area ($110-$136)
}


; --- Main

!ct pet     ; petscii

; start of program

!if target = 20 {
!if variant = 0 {

!if vicmemcfg = 0 {
*=$1001

entry:
; BASIC stub: "2010 SYS 4109"
!by $0b,$10,$DA,$07,$9E,"4","1","0","9",0,0,0    

}

!if vicmemcfg = 3 {
*=$401

entry:
; BASIC stub: "2010 SYS 1037"
!by $0b,$04,$DA,$07,$9E,"1","0","3","7",0,0,0    

}


!if vicmemcfg = 8 {
*=$1201

entry:
; BASIC stub: "2010 SYS 4621"
!by $0b,$12,$DA,$07,$9E,"4","6","2","1",0,0,0    

}

}

!if variant = 1 {


;
; MC code installer based on sys.asm sources of mega-utils by Daniel Kahlin <daniel@kahlin.net>
;


;DASM SYNTAX
;#org $401

;ACME SYNTAX
*=$401

entry:

mc_installer_begin:

;
; Basic line
; 2010 SYSPEEK(44)*256+21
;

;DASM SYNTAX
;	byte $13,$04,$da,$07,$9e,$c2,"(44)",$ac,"256",$aa,"21",0,0,0     

;ACME SYNTAX
!tx $13,$04,$da,$07,$9e,$c2,"(44)",$ac,"256",$aa,"21",0,0,0     

;
; Move the mc-installer code to the tape buffer ($334) and execute it
;

	sei

MC_INSTALL_OFFS_LSB = mc_installer_start - mc_installer_begin
MC_INSTALL_END_LSB  = mc_installer_end - mc_installer_begin

	ldy	#MC_INSTALL_OFFS_LSB
mc_soc_lp1:
	lda	($2b),y		; basic start ($1201, $1001 or $0401)
	sta	mc_installer-MC_INSTALL_OFFS_LSB,y
	iny
	cpy	#MC_INSTALL_END_LSB
	bne	mc_soc_lp1
; C=1
	jmp	mc_installer_entry




;
; Mega-Cart Installer code start here
;

mc_installer_start:

;DASM SYNTAX
;#rorg $0334

;ACME SYNTAX
!pseudopc $0334 {

mc_installer:

mc_installer_no_mc:
	lda	#<mc_installer_no_mc_msg
	ldy	#>mc_installer_no_mc_msg
	jsr	$cb1e
	jmp	$c474

mc_installer_no_mc_msg:

;DASM SYNTAX
;	dc.b	"MEGA-CART NOT FOUND",0

;ACME SYNTAX
!tx "mega-cart not found",0


mc_installer_entry:

; Check for mega cart by toggling NvRAM on and off
	ldx	#$f8
mc_ie_lp1:
	stx	MC_NvRAM_state	; $9d80	- Change NvRAM state (bit0 = 0, enabled)
	stx	MC_NvRAM_state	; $9d80	- Try to write to the NvRAM (same value)
	cpx	MC_NvRAM_state	; $9d80	- Check if the value sticks
	beq	mc_ie_skp1
	clc
mc_ie_skp1:
	ror
	inx
	bmi	mc_ie_lp1
; Last store was $ff -> $9d80 (+32Kb)
	cmp	#$55
	bne	mc_installer_no_mc
	dex
	dex
	stx	MC_NvRAM_state	;$9d80 - $fe -> $9d80

; Ok, Mega-Cart detected properly.  We now have +32Kb +3Kb configured
; Proceed to copy data (in reverse to allow overlap)
	ldy	#mc_program_Pages
	clc
	tya
	adc	$2c
	sta	mc_ie_sm1+2

	ldx	#0
mc_ie_lp2:
	dec	mc_ie_sm1+2
	dec	mc_ie_sm2+2
mc_ie_lp3:

mc_ie_sm1:

;DASM SYNTAX
;	lda	mc_program_start+[mc_program_Pages<<8],x

;ACME SYNTAX
	lda	mc_program_start+(mc_program_Pages<<8),x

mc_ie_sm2:
;DASM SYNTAX
;	sta	mc_program_start+[mc_program_Pages<<8],x

;ACME SYNTAX
	sta	mc_program_start+(mc_program_Pages<<8),x

	inx
	bne	mc_ie_lp3
	dey
	bne	mc_ie_lp2

; Data copied to target, start code
	jmp	mc_program_start

;DASM SYNTAX
;#rend

;ACME SYNTAX (Close code #rorg for $0334)
}
mc_installer_end:





;
; Mega-Cart program code starts here ($47e) 
; SYS1150
;

mc_program_start:



}
}

!if target = 64 {
*=$0801

entry:
; BASIC stub: "1 SYS 2061"
!by $0b,$08,$01,$00,$9e,$32,$30,$36,$31,$00,$00,$00


!if variant = 2 {
;C64-DTV

; Copy the JiffyDOS-DTV detection & install routine
;
; It works with the 6.01 version patched for DTV soft-kernal 
; (http://dtvforge.ath.cx/jiffydtv/)
; Just flash the JIFFYDTV.PRG on C64-DTV and call it JIFFYDTV
; for dtvmkfs just use this txt structure:
;
; JIFFYDTV.PRG,JIFFYDTV,,256,PRG
;

        ldy #0
        ldx #jiffydtv_end-jiffydtv_start
JiffyRoutineLoop: 
	lda jiffydtv_start,y
        sta loadrunpos,y
	iny
	dex
	bne JiffyRoutineLoop

	jsr loadrunpos  ; detect & install & use jiffyDOS for C64-DTV
}

}

!if target = 16 {
*=$1001

entry:
; BASIC stub: "2012 SYS4109"
!by $0B,$10,$DC,$07,$9E,"4","1","0","9",0,0,0    

}

!if target = 128 {
*=$1c01

entry:
!by $13,$1C,$DC,$07 ;line link and #
!by $de, $9c, $3a ;GRAPHIC CLR
!by $fe, $02, "1", "5", $3a ;BANK 15
!by $9E,"7","1","8","9" ;SYS
!by 0,0,0    ;end
}

mlcodeentry:

!if target = 20 {

!if variant = 1 {

;
; Mega-Cart start point from file $047e - (SYS1150)
;

; Set Vic+32k+3k soft reset no auto as last selection for easy restart
; just hold CBM key on reset and type SYS1150 to restart
;
; This codes assumes that NVRAM is enabled because it may starts 
; 1) from the MC-istaller that enables NVRAM
; 2) with a SYS1150 call so 3K thus NVRAM must be enabled before 


;VIC+32K+3K (NO AUTOSTART) - #127, #40

lda #127
sta MC_LastCartEprom1

lda #40
sta MC_LastCartEprom2

lda #0
sta MC_LastCartExp3k

lda #255
sta MC_LastConfig

;
; Mega-Cart start point from Menu in rom $0492 - (SYS1170)
;


; wait a bit to finish the Mega-Cart reset cycle (Not required when restart browser as start mode)

	ldx #$05                   ; default wait loop cycles if value on NVRAM is not valid (2 to 50 allowed).
	
	lda MC_NVRamCheck
	cmp #MC_CheckByte
	bne MC_wait_loop           ; Mega-Cart preferences not setted, use default wait loop cycles

	lda MC_ExternalBootDelay   ; read ExternalBootDelay on NVRAM
	cmp #2
	bcc MC_wait_loop1          ; use default wait loop cycles if value on NVRAM is < 2.
	cmp #51   
	bcs MC_wait_loop1          ; use default wait loop cycles if value on NVRAM is > 50.

	ldx MC_ExternalBootDelay   ; use wait loop cycles stored on Mega-cart NV-RAM 

MC_wait_loop1:
	stx MC_ExternalBootDelay   ; ExternalBootDelay on NVRAM (save current or Default value on NVRAM)

MC_wait_loop:   ;@@@
	txa
	ldx #$ff
	jsr Wait
	tax
	dex
	bne MC_wait_loop


program_restart:

; set proper Mega-cart Memory Configuration for the program (Vic +32K R/W +3K R/W)
ldx #192    
stx MC_NvRAM_state   ; $9d80 - POKE40320,192 (Vic +32K R/W +3K R/W)      ;11000000



;copy the Program Table to $1203
CopyProgramTableBaseStructure:
tbl = $1203

; copy Program table base structure to $1200
ldy #0
ldx #tblbaseend-tblbasestart
- lda tblbasestart,y
sta $1200,y
iny
dex
bne -


} else {

program_restart:

}


; **********************************************************************************************
;
; THIS CODE IS REQUIRED IF THE BROWSER RESTART ITSELF AFTER SETTING THE VIC MEMORY CONFIGURATION
;
; DIFFERENT FROM THE WORKING MEMORY CONFIGURATION
;
; **********************************************************************************************


;!if vicmemcfg = 0 {   *** NOT REQUIRED SINCE THE MEMORY CONFIGURATION IS NOT CHANGED FOR VIC UNEXPANDED
;
;lda start_memory_hi     ; Start of memory (hi-byte)
;cmp #$10
;beq set_output_control  ; SKIP forced mem configu & NEW if current mem config is unexpanded
;
;; Force Vic+3k pointers if program loaded/restarted 
;; from another Vic memory configuration (this code is required if a manual memory configuration is setted)
;
;lda #$1e		 ; Screen memory page start $1e00 (hi-byte)
;ldx #$10                 ; Start of memory $1000 (hi-byte) 
;ldy #$1e                 ; Top of memory $1e00 (hi-byte)    
;
;jsr unexpand            ; Set the forced memory configuration and Perform NEW
;
;}


!if vicmemcfg = 3 {

lda start_memory_hi     ; Start of memory (hi-byte)
cmp #$04
beq set_output_control  ; SKIP forced mem configu & NEW if current mem config is 3k

; Force Vic+3k pointers if program loaded/restarted 
; from another Vic memory configuration (normally Unexpanded only)

lda #$1e		 ; Screen memory page start $1e00 (hi-byte)
ldx #$04                 ; Start of memory $0400 (hi-byte) 
ldy #$1e                 ; Top of memory $1e00 (hi-byte)    

jsr unexpand            ; Set the forced memory configuration and Perform NEW

}

!if vicmemcfg = 8 {

lda start_memory_hi     ; Start of memory (hi-byte)
cmp #$12
beq set_output_control  ; SKIP forced mem configu & NEW if current mem config is 8k+

; Force Vic+32k pointers if program loaded/restarted 
; from another Vic memory configuration (Unexpanded or +3k)

lda #$10		; Screen memory page start $1000 (hi-byte)
ldx #$12	 	; Start of memory $1200 (hi-byte) 
ldy #$80		; Top of memory $8000 (hi-byte)    

jsr unexpand            ; Set the forced memory configuration and Perform NEW

}

}

; **********************************************************************************************


!if target = 20 | target = 16 {

set_output_control:

; this code prevent "LOADING FILENAME" message
; if program started with SYS instead of RUN

lda #$0
sta output_control ; Direct=$80/RUN=0 output control

}

!if target = 128 {
;set appropriate memory config (BANK)
lda #$02		;RAM 0, KERNAL ROM, BASIC High ROM, I/O
sta $ff00		;set mem config
lda #0		;bank 0 (for file, not needed yet)
tax			;(for filename, needed now for OPEN and later for LOAD)
jsr $ff68	;set bank
}

!if forecolor>4 {
lda #forecolor
jsr CHROUT
}

; clear screen
!if clearscrena = 1 {
jsr CLEARSCREEN

; Set text color for C64 with V2 KERNAL 
!if target = 64 { 
    lda #forecolor2 
    ldx #$00 
loop 
    sta color,x 
    sta color+256,x 
    sta color+512,x 
    sta color+768,x 
    dex 
    bne loop 
}

!if target = 20 & forecolor2 <> 1 & forecolor2 < 8 {
;Set foreground char color if not white (NOT REQUIRED FOR C64)
	LDX #$FF
        LDA #forecolor2     

clear_screen_color:
	sta color-1,X
	sta color+$100-2,X
	DEX
        BNE clear_screen_color
}
}


; set colors 

!if target = 20 {

!if bordercolor<8 & backcolor<16 {

lda #backcolor*16+8+bordercolor
sta vicborder
}

}


!if target = 64 {

!if bordercolor<16 {
lda #bordercolor
sta vicborder
}
!if backcolor<16 {
lda #backcolor
sta vicbackgnd
}

}

!if target = 16 {

!if bordercolor<255 {
lda #bordercolor
sta vicborder
}
!if backcolor<255 {
lda #backcolor
sta vicbackgnd
}

;store the current fkeys definition area and set a custom one for the program
jsr set_custom_fkeys_def_area

}

!if target = 128 {

!if bordercolor<16 {
lda #bordercolor
sta vicborder
}
!if backcolor<16 {
ldx #backcolor
stx vicbackgnd	;VIC-II background
lda #$1a		;VDC background register
sta $d600		;access register
lda $d601		;read register
and #$f0		;clear low nibble (color)
ora $ce5c,x		;look-up VDC color nibble (VIC -> VDC translation)
sta $d601		;update register
}

;store the current fkeys definition area and set a custom one for the program
jsr set_custom_fkeys_def_area

}



!if hwdevicenum = 0 & autodetectm = 0 {
; store last used device number
lda lastdevice
sta device

}

!if hwdevicenum = 0 & autodetectm = 1 {
; store last used device number
lda lastdevice
beq SearchDevice ; If current drive is zero search the first active drive

cmp #1
bne StoreDevice  ; If current drive is not zero and not 1 use it

SearchDevice:
jsr SearchActiveDrive ; else search the first active drive

bne StoreDevice ; drive present, store it, A=detected drive

lda #8 ; Set Drive 8 as Default if no drive found

StoreDevice:
sta device

}




!if printbars = 1 {

ldx #listtopy-1    ; row
Jsr PrintLine      ; print the top line

ldx #listbottomy   ; row
Jsr PrintLine      ; print the bottom line


!if key_quit<255 {

;print quit
ldx #listbottomy+1 ;row to print

!if target = 20 {
jsr CLRLINE      ; clear line and set cursor to row listbottomy+1 to avoid screen scroll if listbottomy+1 = 22
}

ldy #listx+17    ; col to print

!if target = 20 {
sty curpos       ; set cursor position and avoid screen scroll
}

!if target = 64 | target = 16 | target = 128 {
clc              ; function: set cursor
jsr PLOT         ; set cursor position
} 

ldy #>quit_text  ; string start point hi-byte
lda #<quit_text  ; string start point lo-byte

jsr PRNSTR       ; print string in A/Y, 0 terminated

}



!if target = 20 & startmode = 1 {
jsr PrintVicMode
}

!if sortdir = 1 {
jsr PrintSortMode
}

!if drivechange = 1 {

ldx #listbottomy+1 ; row to print
!if variant = 2 {
;C64-DTV
ldy #listx+14      ; col to print
} else{
ldy #listx-1       ; col to print
}
clc                ; function: set cursor
jsr PLOT           ; set cursor position

ldy #>drive_text   ; string start point hi-byte
lda #<drive_text   ; string start point lo-byte

jsr PRNSTR       ; print string in A/Y, 0 terminated

jsr PrintDriveNr
}

!if variant = 2 {

;C64-DTV 

ldx #listbottomy+1 ; row to print
ldy #listx-1       ; col to print
clc                ; function: set cursor
jsr PLOT           ; set cursor position

ldy #>cdback_text  ; string start point hi-byte
lda #<cdback_text  ; string start point lo-byte

jsr PRNSTR       ; print string in A/Y, 0 terminated

}


}



OpenNewDrive:


!if showcursors = 1 {
; draw arrows
!if arrowcolor>4 {
lda #arrowcolor
jsr CHROUT
}
!if listx > 0 {
ldy #listx-1
ldx #listtopy
clc
jsr PLOT
lda #'>'
jsr CHROUT
}
!if listx+namelen < ScreenCols {
ldy #listx+namelen
ldx #listtopy
clc
jsr PLOT
lda #'<'
jsr CHROUT
}
!if arrowcolor>4 {
!if forecolor>4 {
; set foreground color again
lda #forecolor
jsr CHROUT
} ; forecolor
} ; arrowcolor
} ; showcursors

!if rootonstart = 1 {
; exit to root: (Also from DNP - HD file images) "CD//", "CD:_", "CD//"    
ldx #<diskcmdroot
ldy #>diskcmdroot
lda #1
jsr openclose
ldx #<diskcmdexit
ldy #>diskcmdexit
lda #1
jsr openclose
ldx #<diskcmdroot
ldy #>diskcmdroot
lda #1
jsr openclose
}

!if rememberpos = 1 {
lda #0
sta prevdirposh
}


!if rootonstart = 0 {

lda #0        ;disknamepetlen - using this value the diskcmdexit command will be just "CD:" without left arrow.
beq loadprev2


; - previous directory load (if rootonstart = 0)
;
loadprev:
lda #1

loadprev2:
sta disknamepetlen

lda #0
sta menustate
sta tmp
} else {

; - previous directory load (if rootonstart = 1)
;
loadprev:
lda #0
sta menustate
sta tmp
lda #1
sta disknamepetlen

}

loadprev3:
lda #<tmp
sta selected
lda #>tmp
sta selectedh
lda #<diskcmdexit
sta tmpptr
lda #>diskcmdexit
sta tmpptrh
jsr loadlist
; set selected to tbl (or remembered pos)
!if rememberpos = 1 {
lda prevdirposh
beq +           ; skip if not available
sta selectedh
lda prevdirpos
sta selected
lda #0
sta prevdirposh ; disable next remembered pos
+
}


; - menu
;
menu:
jsr clearlist

!if sortdir = 1 {

lda sortflag
beq ++

jsr sortlist
++
}

jsr redrawlist  ; SLOW

menuchanged:
!if statusenabl = 1 {
; print file status
jsr printstatus
}

menuwait:

lda #rastercmp
- cmp vicraster
bne -           ; wait until raster = rastercmp

iny             ; y++
cpy #joyrepeat  ; check if y >= joyrepeat

bcc +           ; if not, skip
ldy #0          ; reset joyrepeat counter
lda #$ff        ; reset lastjoy for repeat
sta lastjoy

!if target = 20 | target = 64 | target = 128 {
; read joystick
+ lda joyport   ; a = joy
}


!if target = 16 {
; read joystick
+ jsr GetJoyValueC16

;create a c64 joy structure to use same joy routines

;c16 joy bit switches:

;bit   switch
;0     Up     (Same for C64)
;1     Down   (Same for C64)
;2     Left   (Same for C64)
;3     Right  (Same for C64)
;4     --
;5     --
;6     Fire for Port 1 (Unused for 2)
;7     Fire for Port 2 (Unused for 1)

rol               ; Rotate one bit left (carry = bit 7 now)
;rol              ; add this instructions to check Fire port 1

bcs +             ; button not pressed 

and #%11011111    ; Button pressed, turn off bit 5 (off = pressed) 
;and #%10111111   ; use this instructions instead of previous to check Fire port 1
bcc ++

+ ora #%00100000  ; Button not pressed, turn on bit 5 (on = not pressed) 
;ora #%01000000   ; use this instructions instead of previous to check Fire port 1

++ lsr            ; now fire bit is the same as c64 (bit 4)
; lsr  ; add this instructions to check Fire port 1

ora #%11100000    ; turn on unused bits

}


!if target = 20 {

;create a c64 joy structure to use same joy routines

lsr                ; %11100001  
and joyport        ; %11000001
ora #%00010000     ; %11010001
lsr                ; %11101000 ; now up, down, left and fire bits are like the c64
pha

ldx #127           ; set DDR of VIA #2
stx $9122          ; on VIA #2 port B
lda $9120          ; read I/O port B
ldx #255           ; restore DDR of via #2
stx $9122
and #128           ; filter JOY right (Bit 7)

bne +

pla
and #%11110111      ; %11100000 ; now right bit is like the c64
pha

+ pla
}

;c64 joy bit switches structure:

;bit   switch
;0     Up
;1     Down
;2     Left
;3     Right
;4     Fire   
;5     --
;6     --     
;7     --     



!if target = 20 | target = 64 | target = 16 | target = 128 {
;joystick read
tax             ; save to x
eor lastjoy     ; a = joy xor lastjoy
and lastjoy     ; a = a and lastjoy
stx lastjoy     ; update lastjoy
}

!if target = 20 | target = 64 | target = 128 {
sta curjoy
and #$1f        ; test if anything is pressed
bne +           ; if is, process input
}

!if target = 16 {
sta curjoy
and #$1f        ; test if fire is pressed (otherwise use getin to use kernal key repeat delay)
;pha
bne +           ; if is, process input
}


jsr GETIN       ; read keyboard
tax             ; x = pressed key (or 0)
;!if target = 16 {
; cpx #0         
; bne +          ; key pressed (skip joy input)
; pla            ; joy status
; bne ++         ; joy pressed
;}

beq menuwait    ; if no keys pressed, no input -> wait


; process input

;!if target = 20 | target = 64 {
+ lda curjoy

;}

;!if target = 16 {
;+ pla
;lda #0
;++ 
;}
;processinput:

lsr             ; check if up
bcs +           ; jump if pressed
cpx #key_preve  ; check if key_preve
bne ++          ; jump if not
+ lda menustate ; a = menustate
and #1          ; check if top reached
bne menuwait    ; if it is, jump back
; prev entry
sec
lda selected    ; selected -= entrylen
sbc #entrylen
sta selected
bcs +
dec selectedh
+ 
!if fastscrolle = 1 {
jsr scrollup
jmp menuchanged
} else {
jmp menu
}

++ lsr          ; check if down
bcs +           ; jump if pressed
cpx #key_nexte  ; check if key_nexte
bne ++          ; jump if not
+ lda menustate ; a = menustate
and #4          ; check if last reached
bne menuwait    ; if it is, jump back
; next entry
clc
lda selected    ; selected += entrylen
adc #entrylen
sta selected
bcc +
inc selectedh
+ 
!if fastscrolle = 1 {
jsr scrolldown
jmp menuchanged
} else {
jmp menu
}


++ cpx #key_exit    ; check if key_exit
bne ++              ; jump if not
jmp loadprev        ; load prev dir

!if (target = 64 | target = 128) & key_quit < 255 {
++ cpx #key_quit    ; check if key_quit
bne ++              ; jump if not

; quit to basic
;  rts              ; old method
!if target = 128 {
	lda #0		;RAM 0, all ROMs, I/O
	sta $ff00	;set mem config
; restore default background/border/cursor colors
!if colorenable = 1 {
	jsr Default_Color
}
	jsr restore_fkeys_def_area
	jmp $fa59	; reset screen then Warm Start Basic (Stop+Restore)
} else {
	jmp $fe66	; Warm Start Basic [BRK]
}
}

!if (target = 64 | target = 128) & key_reset < 255 {
++ cpx #key_reset
bne ++              ; jump if not
;jmp $FCE2           ; Reset the C64 SYS64738
jmp ($fffc)			;reset CPU
}




!if target = 20 & key_quit < 255 {
++ cpx #key_quit    ; check if key_quit
bne ++
jmp ExitToBasic
}

!if target = 20 & key_reset < 255 {
++ cpx #key_reset
bne ++              ; jump if not

!if variant = 0 {
   jmp StartMode2   ; Reset the vic (Clean Mode)
} else {
   sta $9e00        ; Restart the Mega-Cart Menu
}

}

!if target = 16 & key_quit < 255 {
++ cpx #key_quit    ; check if key_quit
bne ++              ; jump if not

; quit to basic
jsr restore_fkeys_def_area
jsr $FF8A            ; RESTOR - Restore vectors to initial values
jsr $FF81            ; CINT   - Initialize screen editor
jsr $FF84            ; IOINIT - Initialize I/O devices
jmp $8003            ; Warm Start Basic [BRK]
}

!if target = 16 & key_reset < 255 {
++ cpx #key_reset
bne ++              ; jump if not

jmp $fff6           ; Reset the C16/Plus4 SYS65526
}


++ cpx #key_top     ; check if key_top
bne ++              ; jump if not

movetop:
lda #<tbl
sta selected        ; selected = table
lda #>tbl
sta selectedh
jmp menu

++ cpx #key_bottom  ; check if key_bottom
bne ++              ; jump if not
lda tbllst
sta selected        ; selected = table
lda tbllsth
sta selectedh
jmp menu

++ lsr          ; check if left
bcs +           ; jump if pressed

!if key_prevp2=255 {

cpx #key_prevp  ; check if key_prevp (only)
bne ++          ; jump if not
+ jmp prevpage  ; previous page

} else {

cpx #key_prevp  ; check if key_prevp
beq prevpage20
cpx #key_prevp2 ; check if key_prevp2
bne ++          ; jump if not
prevpage20:
+ jmp prevpage  ; previous page
}


++ lsr          ; check if right
bcs +           ; jump if pressed

!if key_nextp2=255 {

cpx #key_nextp  ; check if key_nextp (only)
bne ++          ; jump if not

} else {

cpx #key_nextp  ; check if key_nextp
beq nextpage2
cpx #key_nextp2 ; check if key_nextp2
bne ++          ; jump if not
nextpage2:
}

+ jmp nextpage  ; next page

++ lsr          ; check if fire
bcs firepressed ; jump if pressed
cpx #key_fire   ; check if key_fire
beq firepressed ; jump if pressed

!if target = 64 | target = 16 | target = 128 {

!if startmode = 1 {
cpx #key_fire2  ; check if key_fire2 (CBM+ENTER)
beq firepressed ; jump if pressed
}

}


!if sortdir = 1 { 
cpx #key_sort    ; check if key_sort
beq keysortpressed 
}


!if drivechange = 1 { 
cpx #key_drive  ; check if key_drive is pressed
bne ++          ; jump if not
jmp ChangeDrive ; change Drive
++
}

!if target = 20 {

!if startmode = 1 {
cpx #key_startmode ; check if key_startmode is pressed
bne ++             ; jump if not
jsr ChangeVicMode  ; change Vic Mode 

++
}

}

jmp menuwait


!if sortdir = 1 {

; - key_sort pressed
keysortpressed:

lda sortflag
eor #$ff
sta sortflag

!if printbars = 1 {
jsr PrintSortMode
}


lda #0
sta menustate
sta tmp
sta disknamepetlen

!if rememberpos = 1 {
sta prevdirposh       ;Reset previous dir position because it remembers the position of the list sorted in another way. 
}


lda sortflag
beq unSort


!if pleasewaite = 1 {
lda #<tmp
sta selected
lda #>tmp
sta selectedh

; print info...
jsr showpleasewait
}

jsr sortlist
jmp movetop

unSort:        ;Read again the current directory
jmp loadprev3




!if printbars = 1 {
PrintSortMode:

ldx #listbottomy+1 ; row
!if variant = 2 {
;C64-DTV
ldy #listx+8       ; col to print
} else{
ldy #listx+13      ; col to print
}
clc                ; function: set cursor
jsr PLOT

lda sortflag
beq sortOFF

lda #<sortON_text
ldy #>sortON_text
jmp PrintSortString

sortOFF:
lda #<sortOFF_text
ldy #>sortOFF_text

PrintSortString:
jsr PRNSTR
rts
}


}

; - fire pressed
firepressed:

; - check if first file ("_")
lda selectedh
cmp #>tbl
bne ++          ; jump if not
lda selected
cmp #<tbl
bne ++          ; jump if not
jmp loadprev    ; was "_", load prev dir

!if rootentry = 1 {
; - check if 2nd entry ("//") exit to root
++
lda selectedh
cmp #>tbl+entrylen
bne +++          ; jump if not
lda selected
cmp #<tbl+entrylen
bne +++          ; jump if not

!if rootonstart = 0 {

;*** this routine is not present on OpenNewDrive if rootonstart = 0 ***
;*** so I replicate it here ****
;
;a JSR call is better but, at the moment, I leave it as is for old C64 version comparison

; exit to root: "CD:_", "CD//"
ldx #<diskcmdexit
ldy #>diskcmdexit
lda #1
jsr openclose
ldx #<diskcmdroot
ldy #>diskcmdroot
lda #1
jsr openclose
}

JMP OpenNewDrive  ; reload current drive dir from root
+++
} else {
++
}
; copy selected to disknamepet, save length of name
ldy #namelen
sty disknamepetlen
lda #<disknamepet
sta tmpptr
lda #>disknamepet
sta tmpptrh
- dey
php
lda (selected),y
bne +
sty disknamepetlen
+ sta (tmpptr),y
plp
bne -

; check if inside d64
bit menustate
bmi prselected  ; if in d64, load selected program

; check if selected is dir
ldy #(entrylen-1)
lda (selected),y
cmp #3          ; FIXME magic value for "dir"
bne search      ; jump if not dir

loaddir:
!if rememberpos = 1 {
lda selected
sta prevdirpos
lda selectedh
sta prevdirposh
}
lda #<diskcmdcd
sta tmpptr
lda #>diskcmdcd
sta tmpptrh
jsr loadlist
jmp menu

; determine if selected file is d64(/d71/d81/m2i/d41/dnp/tap)
;  - if d64 -> change dir & set d64 flag

; seach for last .
search:
!if tap_support = 1 {
lda #$0
sta tmp3            ; tmp3 used as flag for TAP detection (0=NO TAP)
}
+ ldy #namelen-1
- lda (tmpptr),y
cmp #'.'
beq +
dey
bne -

; . not found, assuming entry is program
beq prselected

; . found on y (>0), check known extensions
+ iny
lda #<extensions
sta extension_list
lda #>extensions
sta extension_list+1
lda #4               ; extension len
ldx #extensions_max  ; extension max
jsr parseext
cpx #0
beq prselected      ; jump not d64(/d71/d81/m2i/d41/dnp/tap)

lda #$0
sta menustate       ; Clear menustate bits
cpx #4
beq loaddir         ; inside dnp, just load directory, do not set flag "inside d64" to allow subfolders browsing  

!if tap_support = 1 {
cpx #8
beq tapselected     ; tap file selected
}

; d64(/d71/d81/m2i/d41) selected
lda #$80            ; set "inside d64" flag 
sta menustate
bne loaddir         ; load directory

!if tap_support = 1 {
tapselected:
lda #$1
sta tmp3            ; tmp3 used as flag for TAP detection (1=TAP file detected)
}

; - program selected, start loading
prselected:

!if pleasewaite = 1 {
; print info...
jsr showpleasewait
}

; setup load
setupload:

!if target = 16 | target = 128 {

jsr restore_fkeys_def_area

}

!if target = 20 {

!if startmode = 0 {
      ; for TAP files always uses the current machine memory Config (3k, 8k, unexpanded)
      ; there is no way to read first 2 bytes of prg file inside the TAP.
jmp SetAutoConfiguration

} else {

!if tap_support = 1 {

lda tmp3

!if variant = 1 {
bne MC_SJLOAD_Disable      ; TAP file selected so force Manual Start to select Mem Config
                           ; and disable SJLOAD Pointers 
} else {
bne SetManualConfiguration ; TAP file selected so force Manual Start to select Mem Config

}

}

lda VicMode
bne SetManualConfiguration

jmp SetAutoConfiguration


SetManualConfiguration:


!if variant = 1 {

lda MC_SJLOAD_init      ; check if SJLOAD (minimal) for Mega-Cart is installed

cmp #$4c

bne MC_SJLOAD_finish

;menu to enable/disable sjload speed-up
ldy #>SJLOADconfig_text ; string start point hi-byte
lda #<SJLOADconfig_text ; string start point lo-byte
jsr PRNSTR              ; print string in A/Y, 0 terminated

keyreadloop3:
jsr GETIN   ; read keyboard

ldx #0
cmp #133    ; F1 (Enabled)
beq setmanualsjloadconfig
inx
cmp #134    ; F3 (Disabled)
beq setmanualsjloadconfig

bne keyreadloop3

setmanualsjloadconfig:
cpx #1
beq MC_SJLOAD_Disable

jsr MC_SJLOAD_init  ; SYS40000, set SJLOAD pointers for LOAD
jmp MC_SJLOAD_finish

MC_SJLOAD_Disable:
jsr $fd52           ; $fd52 restor - Restore Kernal Vectors (at 0314) 
LDY #$03
STY $B3        ; restore Pointer of start of tape buffer ($033c) used by sjload to prevent
               ; ILLEGAL DEVICE NUMBER ERROR if you load a TAP file or use the Datassette
;LDX #$3C
;STX $B2       ; low byte not used save routine space

MC_SJLOAD_finish:
}



!if vicmemcfg = 3 | vicmemcfg = 8 {
ldy #>memconfig_text ; string start point hi-byte
lda #<memconfig_text ; string start point lo-byte
jsr PRNSTR           ; print string in A/Y, 0 terminated

keyreadloop1:
jsr GETIN   ; read keyboard

ldx #0
cmp #133    ; F1 (Unexpanded)
beq setmanualmemconfig
inx
cmp #134    ; F3 (+3K)
beq setmanualmemconfig

!if vicmemcfg = 8 {

inx
cmp #135    ; F5 (+32K)
beq setmanualmemconfig
inx
!if variant = 1 {
cmp #136    ; F7 (+32K Read Only)
beq setmanualmemconfig
}
inx
cmp #140    ; F8 (+32K+3K)
beq setmanualmemconfig
}

bne keyreadloop1
}


setmanualmemconfig:

!if vicmemcfg = 3 | vicmemcfg = 8 {

stx tmpptrh
}

!if tap_support = 1 {
ldx #0
lda tmp3
bne setmanualstartmode   ; TAP file selected so set Vic Prompt just to skip start mode that is always LOAD+RUN  
}

ldy #>startconfig_text ; string start point hi-byte
lda #<startconfig_text ; string start point lo-byte
jsr PRNSTR             ; print string in A/Y, 0 terminated

keyreadloop2:
jsr GETIN   ; read keyboard

ldx #0
cmp #133    ; F1  (Vic Prompt)
beq setmanualstartmode
inx
cmp #134    ; F3  (RUN)
beq setmanualstartmode
inx
cmp #135    ; F5  (RESET)
beq setmanualstartmode
inx
cmp #136    ; F7  (RESTART BROWSER)
beq setmanualstartmode

bne keyreadloop2

setmanualstartmode:
stx tmpptr

!if vicmemcfg = 3 | vicmemcfg = 8 {

jsr SetVicMemConfig

}


jmp startloading
}



!if vicmemcfg = 3 | vicmemcfg = 8 {

SetVicMemConfig: ;Change basic mem pointers according with selected Mem Config
                 ;This routine assumes that it works from a Vic +32K +3K

lda tmpptrh   ; read the Mem Config check byte (0 = Unexpanded) (1 = +3K)(2 = +32K)(3 = +32K Read Only)(4 = +32K +3K)

!if vicmemcfg = 3 {

beq SetVicUnexpanded

rts  ; Since it works from a Vic +3K no changes required for mem config 1 (+3K) 
     ; and also no changes for config 2 (+32K) and 4 (+32K +3K) just because there is no memory expansion
     ; so the program will fail anyway
}

!if vicmemcfg = 8 {

beq SetVicUnexpanded

cmp #$01 
beq SetVic3K

cmp #$03
beq SetVicUnexpanded     ; Set mem pointers as Unexpanded for +32K Read Only Mem Config

rts   ; Since it works from a Vic +32K +3K no changes required for mem config 2 (+32K) and 4 (+32K +3K)


SetVic3K:
lda #$1e		 ; Screen memory page start $1e00 (hi-byte)
ldx #$04                 ; Start of memory $0400 (hi-byte) 
ldy #$1e                 ; Top of memory $1e00 (hi-byte)    
bne unexpand

}


SetVicUnexpanded:
lda #$1e		 ; Screen memory page start $1e00 (hi-byte)
ldx #$10		 ; Start of memory $1000 (hi-byte) 
ldy #$1e		 ; Top of memory $1e00 (hi-byte) 

unexpand:
sta screen_mem_page      ; Screen memory page start (hi-byte)
stx start_memory_hi      ; Start of memory (hi-byte)
sty top_memory_hi        ; Top of memory (hi-byte)

jsr CINT1                ; Initialize I/O  
JSR $E45B                ; initv Initialize Vectors 
JSR $E3A4                ; initcz Initialize BASIC RAM

;initms Output Power-Up Message without message
;LDA $2B
;LDY $2C
;JSR $C408                ; reason Check Memory Overlap (it Does not seems to be required)

JMP $C644                ; scrtch Perform [new] (FORCED) and return

}



SetAutoConfiguration:

!if variant = 1 {

lda MC_SJLOAD_init

cmp #$4c

bne Check_MC_SJLOAD_Exit

jsr MC_SJLOAD_init  ; SYS40000, set SJLOAD pointers for LOAD

Check_MC_SJLOAD_Exit:

}

; detect the file start address to set the vic memory configuration
;
lda #0
sta status_word_st  ; set Status word ST as 0

ldx #$04            
stx tmpptrh         ; check byte to detect the Mem Config (Default is +32K +3K)
                    ; 0 = Unexpanded
		    ; 1 = +3K
		    ; 2 = +32K
		    ; 3 = +32K Read Only
		    ; 4 = +32K +3K

lda #3
sta tmpptr          ; check byte to detect the RUN mode (Default is Restart FileBrowser)
                    ; 0 = Do not RUN (BASIC PROMPT)
                    ; 1 = RUN
		    ; 2 = SYS64802
		    ; 3 = Restart FileBrowser

lda disknamepetlen  ; a = filenamelen
ldx #<disknamepet
ldy #>disknamepet   ; x:y->filename
jsr SETNAM          ; "filename"
       
lda #1      ; filenr
ldx device  ; unitnr
ldy #0      ; sec.address (,0) ; Open drive sa 0 for input operations

jsr SETLFS

jsr OPEN    ; open 1,8,0,"filename"

lda status_word_st        ; read Status word ST
bne VicConfigurationExit  ; if a <> 0 then there is an error so exit from autodetection

ldx #$01    
jsr CHKIN   ; set logical file number #1 as input
       
jsr CHRIN   ; get file address lo-byte
tay         ; and move it to Y (CHRIN uses A and X so Y is a safe place)
jsr CHRIN   ; get file address hi-byte in A


cpy #$00    ; check file address lo-byte
bne CheckBasicProgramAddress

!if vicmemcfg = 8 {

cmp #$20
beq VicConfigurationExit ; leave default values for BLK1 ($2000) cart image (+32K +3K & Restart FileBrowser)
cmp #$40
beq VicConfigurationExit ; leave default values for BLK2 ($4000) cart image (+32K +3K & Restart FileBrowser)
cmp #$60
beq VicConfigurationExit ; leave default values for BLK3 ($6000) cart image (+32K +3K & Restart FileBrowser)

cmp #$a0
beq SetA0CartImage       ; Set A0 Cart image parameters

}

SetNotCartImage:
lda #0
sta tmpptr               ; Set Do not RUN (BASIC PROMPT) as Start mode and leave (+32K +3K) for NOT cart image and NOT basic programs
jmp VicConfigurationExit 


!if vicmemcfg = 8 {

SetA0CartImage:
lda #$02    ; Set Reset as Start mode for A0 cart image
ldx #$03    ; set +32K Read Only for A0 cart image
stx tmpptrh 
bne VicSetRunMode2

}


CheckBasicProgramAddress:
cpy #$01
bne SetNotCartImage      ; Set NOT Cart image parameters for NOT Basic Programs


ldx #$0

cmp #$10
beq setautomemconfig     ;  set Vic Unexpanded for unexpanded Basic programs

inx
cmp #$04
beq setautomemconfig     ; set +3K for 3k Basic programs
n
inx
cmp #$12
beq setautomemconfig     ; set +32K for 8k+ Basic programs

bne SetNotCartImage      ; Set NOT Cart image parameters for NOT Basic Programs


setautomemconfig:
stx tmpptrh  

!if vicmemcfg = 3 | vicmemcfg = 8 {

jsr SetVicMemConfig

}

VicSetRunMode:
lda #1
VicSetRunMode2:
sta tmpptr               ; check byte to detect the RUN mode (0 = NONE) (1 = RUN)(2 = SYS64802)

VicConfigurationExit:
lda #1
jsr CLOSE   ;close 1
ldx #0      
jsr CHKIN   ; restore standard input



startloading:
}



 

!if tap_support = 1 {

lda tmp3
beq +             ; No TAP file selected

; setup TAP name
ldx disknamepetlen
inx     ; x = filenamelen
ldy #00
lo_1:
lda disknamepet,y                 
sta tapname+3,y
iny
dex
bne lo_1

ldx #<tapname
ldy #>tapname
lda disknamepetlen
jsr openclose

!if target = 64 {

ldx #$ff
sei
txs
cld
ldx #$0
stx $d016
JSR $FDA3         ; ioinit - Initialise I/O
;JSR $FD50         ; ramtas - Initialise System Constants (skip Memory check, Fast boot)
JSR $FD15         ; restor - Restore Kernal Vectors (at 0314)
JSR $FF5B         ; cint   - Initialize screen editor
cli

JSR $E453         ; initv  - Initialize Vectors
JSR $E3BF         ; initcz - Initialize BASIC RAM
jsr $e422         ; initms - Output Power-Up Message (DO NOT SKIP, otherwise there are problems with some loaders)
ldx #$fb
txs

; LOAD from Tape (Shift + Run Stop)
lda #131
sta $277
lda #1
sta $c6

JMP $A7AE    ; BASIC Warm Start (RUN)

}

!if target = 20 {
;JSR $FD8D
;JSR $FD52
JSR $FDF9      ; ioinit Initialise I/O
jsr CINT1      ; Initialize I/O  
JSR $E45B      ; initv Initialize Vectors
JSR $E3A4      ; initcz Initialize BASIC RAM
JSR $E404      ; initms Output Power-Up Message
LDX #$FB
TXS

; LOAD from Tape (Shift + Run Stop)
lda #131
sta $277
lda #1
sta $c6

JSR $c474      ;ready Restart BASIC

}

!if target = 16 {
jsr CINT                 ; CINT - $ff81 - Initializes screen editor 
jsr IOINIT               ; IOINIT - $ff84 - Initializes I/O devices

; LOAD from Tape (L[O]+ENTER)
lda #76
sta $527
lda #207
sta $528
lda #13
sta $529
lda #82
sta $52A
lda #213
sta $52B
lda #13
sta $52C
lda #6
sta $ef

rts
}

!if target = 128 {
jsr CINT                 ; CINT - $ff81 - Initializes screen editor 
jsr IOINIT               ; IOINIT - $ff84 - Initializes I/O devices

; LOAD from Tape (L[O]:R[U]+ENTER)
lda #76
sta $34A
lda #207
sta $34B
lda #13
sta $34C
lda #82
sta $34D
lda #213
sta $34E
lda #13
sta $34F
lda #6
sta $D0

rts
}
+
}



!if target = 64 | target = 16 {

!if startmode = 0 {
;old method,  LOAD"FILE",8,1 + (System auto RUN)
lda #1      ; filenr
ldx device  ; unitnr
ldy #1      ; sec.address (,1)
}


!if startmode = 1 {

LDA SHFLAG    ; 1 shift pressed, 2 CBM pressed, 4 CTRL pressed 
and #$02
cmp #$02      ; check cbm key  
beq +         ; start mode 2 (LOAD "FILE",8 + RUN) 
        
ldy #1        ; sec.address (,1) File start address LOAD
lda #0        ; start mode 0 (LOAD "FILE",8,1 + RUN) 
beq ++

+ ldy #0      ; sec.address (,0) Basic start address LOAD

++ sta tmpptr ; start mode (0 or <> 0)

lda #1        ; filenr
ldx device    ; unitnr
}
}


!if target = 128 {

!if colorenable = 1 {
	jsr Default_Color	; restore default background/border/cursor colors
}

!if startmode = 0 {
;old method,  DLOAD"FILE"
lda #1      ; filenr
ldx device  ; unitnr
ldy #0      ; sec.address
}

!if startmode = 1 {

lda #2        ; start mode 2 (DLOAD "FILE" + RUN)
ldy SHFLAG    ; 1 shift pressed, 2 CBM pressed, 4 CTRL pressed 
beq + 
        
lda #0        ; start mode 0 (BLOAD "FILE" + SYS) 

+ sta tmpptr ; start mode (0 or <> 0)

lda #1        ; filenr
ldx device    ; unitnr
}
}

!if target = 20 {

ldy #1      ; sec.address (,1)

lda tmpptr
cmp #1             ;is RUN the start mode ? (1 = RUN)
bne startloadingb

ldy #0      ; sec.address (,0) for RUN start mode
startloadingb:
lda #1      ; filenr
ldx device  ; unitnr

}


jsr SETLFS
lda disknamepetlen  ; a = filenamelen
ldx #<disknamepet
ldy #>disknamepet   ; x:y->filename
jsr SETNAM

; copy LOAD & RUN code to loadrunpos
ldy #0
ldx #loadrunend-loadrunstart
- lda loadrunstart,y
sta loadrunpos,y
iny
dex
bne -
jmp loadrunpos  ; jump to loadrunpos

; (the following code is executed at loadrunpos)
loadrunstart    ; start of code to copy


!if target = 20 {
; load program for the Vic-20 
;
; the Vic-20 loadrun rutine do not use relative addresses so do not use direct JMP or JSR inside the routine but BEQ BNE etc
;

lda #0      ; a = 0: load
                          ;Load start address used only in RUN start mode (sec.address (,0) on SETLFS)
ldx basicstartaddress_lo  ;Pointer: Start of Basic lod  byte
ldy basicstartaddress_hi  ;Pointer: Start of Basic hi byte

jsr LOAD
; error detection would be nice :)

; save end address
stx variablesstartaddress_lo
sty variablesstartaddress_hi


!if variant = 1 {

; Set hardware Vic Memory Configuration  

lda tmpptrh   ; read the Mem Config check byte (0 = Unexpanded) (1 = +3K)(2 = +32K)(3 = +32K Read Only)(4 = +32K +3K)

;#129 is the correct value but it disables also NVRAM and SJLOAD routines 
;     so leave 3K enabled since Vic Memory pointers for screen/color are already changed above
;
;ldx #129    ; POKE40320,129 (Vic +32K R/O +3K DISABLED) ;10000001

ldx #128    ; POKE40320,128 (Vic +32K R/O +3K R/W)      ;10000000
cmp #0      ; F1 (Unexpanded)
beq set_megacart_memconfig
cmp #3      ; F7 (+32K Read Only)
beq set_megacart_memconfig

ldx #128    ; POKE40320,128 (Vic +32K R/O +3K R/W)      ;10000000
cmp #1      ; F3 (+3K)
beq set_megacart_memconfig

;#193 is the correct value but it disables also NVRAM and SJLOAD routines 
;     so leave 3K enabled since Vic Memory pointers for screen/color are already changed above
;
;ldx #193    ; POKE40320,193 (Vic +32K R/W +3K DISABLED) ;11000001

ldx #192    ; POKE40320,192 (Vic +32K R/W +3K R/W)      ;11000000
cmp #2      ; F5 (+32K)
beq set_megacart_memconfig

ldx #192    ; POKE40320,192 (Vic +32K R/W +3K R/W)      ;11000000
;save space Vic+32K+3K assumed at this point
;cmp #4      ; F8 (+32K +3K)
;beq set_megacart_memconfig


set_megacart_memconfig:
stx MC_NvRAM_state ;$9d80 - poke40320,X
}


lda tmpptr   ; read the RUN mode check byte (0 = NONE) (1 = RUN)(2 = SYS64802)(3 = RESTART BROWSER)

cmp #$01 
beq StartMode1

cmp #$02 
beq StartMode2

cmp #$03 
beq StartMode3

ExitToBasic:        ; Return to basic
JMP $FED5           ; Warm Start Basic [BRK] (without Restore Kernal Vectors (at 0314))


StartMode1:  ; Perform [RUN]

; clear screen
!if clearscrena = 1 {
jsr CLEARSCREEN
}

; restore default background/border/cursor colors
!if colorenable = 1 {
lda #27
sta vicborder           ;$900F default border/background colors (#27)

lda #31
jsr CHROUT              ;default coursor color (#31 blue)

}

JSR $C659    ; CLR
JSR $C533    ; Relinks BASIC Program from and to any address... 
JMP $C7AE    ; BASIC Warm Start (RUN)


StartMode2:  ; Perform a clear soft reset (It allows MS-PacMan and others to start)

; -- Reset IO --Start--
sei
lda #$00
sta $9112                        
sta $9113                        
sta $9122                        
sta $9123                        

lda #$7f
sta $911e                        
sta $912e                        
; -- Reset IO --End--

JMP $FD22    ; SoftReset

StartMode3:  ; Restart the file browser

JMP program_restart

}


!if target = 64 | target = 16 {

!pseudopc loadrunpos {
; load program
;
lda #0      ; a = 0: load

!if startmode = 1 {
                          ;Load start address used only in RUN start mode (sec.address (,0) on SETLFS)
ldx basicstartaddress_lo  ;Pointer: Start of Basic lo byte
ldy basicstartaddress_hi  ;Pointer: Start of Basic hi byte
}

jsr LOAD
; error detection would be nice :)

; save end address
stx variablesstartaddress_lo
sty variablesstartaddress_hi
}
}

!if target = 128 {
	lda #0		;0=load
	sta $ff00	;bank 15 (all ROMs and I/O)
	ldx basicstartaddress_lo  ;Pointer: Start of Basic lo byte
	ldy basicstartaddress_hi  ;Pointer: Start of Basic hi byte
	jsr LOAD
	; error detection would be nice :)
	stx $1210	;txt_top (end of program, low)
	sty $1211	;(high)
}


!if target = 64 {


; clear screen
!if clearscrena = 1 {

jsr CLEARSCREEN
}

; restore default background/border/cursor colors
!if colorenable = 1 {
lda #14
sta vicborder           ;$d020 default border color (#14)

lda #6
sta vicbackgnd          ;$d021 default background color (#6)

lda #154
jsr CHROUT              ;default coursor color (#154 Light Blue)


}




!if startmode = 0 {
; autostart program
; (from comp.sys.cbm)
;
jsr $a659   ; reset pointers etc...
jmp $a7ae   ; BASIC warm start
}


!if startmode = 1 {

JSR $A659    ; CLR

lda tmpptr   ; read the RUN mode check byte (0 = ,8,1 + RUN)(<> 0 = ,8 + RUN)
beq StartMode0

JSR $A533    ; Relinks BASIC Program from and to any address... 

StartMode0:  
JMP $A7AE    ; BASIC Warm Start (RUN)

;ExitToBasic: ; Return to basic
;JMP $FE66    ; Warm Start Basic [BRK]

}

}

!if target = 16 {

; clear screen
!if clearscrena = 1 {
jsr CLEARSCREEN
}

; restore default background/border/cursor colors
!if colorenable = 1 {
lda #238
sta vicborder           ;$FF19 default border color (#238)

lda #241
sta vicbackgnd          ;$FF15 default background color (#241)

lda #144
jsr CHROUT              ;default coursor color (#144 black)

}


!if startmode = 0 {

Jsr $8A98    ; Perform [CLR]
;JSR $8818    ; linkprg Rechain Lines - Relinks BASIC Program from and to any address... 
jsr $8bbe    ; perform run+clr
jmp $8bea    ; start

}


!if startmode = 1 {

Jsr $8A98    ; Perform [CLR]

lda tmpptr   ; read the RUN mode check byte (0 = ,8,1 + RUN)(<> 0 = ,8 + RUN)
beq StartMode0

JSR $8818    ; linkprg Rechain Lines - Relinks BASIC Program from and to any address... 

StartMode0:  
jsr $8bbe    ; perform run+clr
jmp $8bea    ; start


;ExitToBasic: ; Return to basic
;jsr $FF8A            ; RESTOR - Restore vectors to initial values
;jsr $FF81            ; CINT   - Initialize screen editor
;jsr $FF84            ; IOINIT - Initialize I/O devices
;jmp $8003            ; Warm Start Basic [BRK]

}


}


!if target = 128 {

; clear screen
!if clearscrena = 1 {

jsr CLEARSCREEN
}

!if startmode = 0 {
	jsr $51f3	;CLR
	jsr $f4f4	;relink program lines
	jsr $5a81	;setup RUN mode
	jmp $4af6	;advance to next (first) BASIC program line
}


!if startmode = 1 {

	lda tmpptr   ; read the RUN mode check byte (0 = machine language)(<> 0 = BASIC)
	beq StartMode0
	jsr $51f3	;CLR
	jsr $4f4f	;relink program lines
	jsr $5a81	;setup RUN mode
	jmp $4af6	;advance to next (first) BASIC program line

StartMode0:  
	jmp ($00ac)	;SYS to load address (bug with old ROMs and non-fast-serial disk drive)
}
}


loadrunend  ; end of code to copy


; --- Subroutines

; - nextpage
;
nextpage:
lda #<(listbottomy-listtopy)*entrylen
sta tmp
lda #>(listbottomy-listtopy)*entrylen
sta tmp2
clc
lda selected    ; selected += page
adc tmp
sta selected
lda selectedh
adc tmp2
sta selectedh
lda tbllsth     ; check if table last < selected
cmp selectedh
bcc +
bne ++
lda tbllst
cmp selected
bcs ++
+ lda tbllst
sta selected    ; table last < selected -> selected = table last
lda tbllsth
sta selectedh
++ jmp menu 


; - prevpage
;
prevpage:
lda #<(listbottomy-listtopy)*entrylen
sta tmp
lda #>(listbottomy-listtopy)*entrylen
sta tmp2
sec
lda selected    ; selected -= page
sbc tmp
sta selected
lda selectedh
sbc tmp2
sta selectedh
lda #>tbl       ; check if selected < table
cmp selectedh
bcc ++
bne +
lda #<tbl
cmp selected
bcc ++
+ lda #<tbl
sta selected    ; selected < table -> selected = table
lda #>tbl
sta selectedh
++ jmp menu 


!if printbars = 1 {

; parameters:
;  x->the row where to print the bar
; returns:
;  menustate

PrintLine:

ldy #listx-1       ; col
clc                ; function: set cursor
jsr PLOT           ; set place to print
 
ldy #0
- lda #$c0     ; "-" char
jsr CHROUT     ; print char
iny
cpy #namelen+6
bne -
rts


}


!if target = 20 {

!if startmode = 1 {

; - ChangeVicMode
;
ChangeVicMode:
lda VicMode
eor #$ff
sta VicMode

!if printbars = 1 {
PrintVicMode:
ldx #listbottomy+1 ; row
ldy #listx+7       ; col
clc                ; function: set cursor
jsr PLOT           ; set place to print

lda VicMode
beq manualOFF

lda #<manualON_text
ldy #>manualON_text
jmp PrintVicModeString

manualOFF:
lda #<manualOFF_text
ldy #>manualOFF_text

PrintVicModeString:
jsr PRNSTR
}

rts
}



!if variant = 1 {
Wait:	; wait X cycles and returns
	ldy #$ff
WaitLoop
	dey
	bne WaitLoop
	dex
	bne WaitLoop  
        rts
}



}



!if drivechange = 1 { 
ChangeDrive:  

lda #0
sta status_word_st      ; set Status word ST as 0

ldx device
; #8-#14  (Drive accepted for increasing auto-detection)
cpx #8
bmi SearchActiveDriveSTD   ;auto detect next drive from drive #8
cpx #14  
bpl SearchActiveDriveSTD   ;auto detect next drive from drive #8

inx
txa
sta lastdevice

jsr SearchActiveDriveLoop  ;auto detect next drive from current drive +1

bne StoreNewDevice ; drive present, store it, A=detected drive

SearchActiveDriveSTD:
jsr SearchActiveDrive
bne StoreNewDevice ; drive present, store it, A=detected drive
lda #8 ; Set Drive 8 as Default if no drive found

StoreNewDevice:
sta device

!if printbars = 1 {
jsr PrintDriveNr
}

JMP OpenNewDrive

!if printbars = 1 {
PrintDriveNr:
ldx #listbottomy+1 ; row

!if variant = 2 {
;C64-DTV
ldy #listx+19       ; col
} else {
ldy #listx+4       ; col
}
clc                ; function: set cursor
jsr PLOT

!if buttoncolor>4 {
lda #buttoncolor
jsr CHROUT       ; print Red
}

lda device
cmp #128
bpl PrintDriveNumberText
cmp #10
bpl PrintDriveNumberText
lda #"0"
jsr CHROUT       ; print "0" char for device 8 and 9
	
PrintDriveNumberText:
ldx device
lda #0
jsr PRNINT       ; print integer in A/X

!if forecolor>4 {
lda #forecolor
jsr CHROUT       ; print white
}

lda device
cmp #128
bpl PrintDriveNrExit
cmp #100
bpl PrintDriveNrExit

lda #$20
jsr CHROUT       ; print space

PrintDriveNrExit:
rts
}

}



!if  autodetectm = 1 | drivechange = 1 {

SearchActiveDrive:     
	lda #0
	sta status_word_st      ; set Status word ST as 0

	lda #8                
	sta lastdevice          ; check for first existing drive from #8 to #15

SearchActiveDriveLoop:          ; from drive #8 to drive #15 
	jsr LISTN		; send LISTEN command ($FFB1) to the drive #A
	jsr UNLSN               ; send UNLISTEN command ($FFAE)
	lda status_word_st      ; read Status word ST
	beq SearchActiveDriveExit ; if a = 0 then drive exist so check for file now

	lda #0
	sta status_word_st      ; set Status word ST as 0

	inc lastdevice
	lda lastdevice
       
	cmp #16                   
	bne SearchActiveDriveLoop ; if a <> #16 loop  
	
	;else device from #8 to #15 is not present so exit reporting 0 as current device

	lda #0
	sta lastdevice

SearchActiveDriveExit:
	lda lastdevice          ; A report Current device, 0 = not found
	rts
}




!if disknameena = 1 & drivechange = 1 {

; - cleardiskname
;
cleardiskname:

lda #18
jsr CHROUT
ldx #disknamey
ldy #disknamex
clc            ; function: set cursor
jsr PLOT       ; set place to print
ldy #0
- lda #$20     ; space char
jsr CHROUT     ; print char
iny
cpy #namelen
bne -
lda #146
jsr CHROUT
rts
}




; - clearlist
;
clearlist:
!if target = 128 {
	jsr PreScroll	;set window to clear
	jsr $ff7d		;Print Immediate
	!by $93			;clear window
	!by $13, $13	;full screen
	!by 0			;end of string
} else {
lda #<screen+listtopy*ScreenCols+listx
sta clearlistsl
lda #>screen+listtopy*ScreenCols+listx
sta clearlistsh

ldx #listbottomy-listtopy
-- ldy #0
lda #listbackgnd
- 
clearlistsl = *+1
clearlistsh = *+2
sta $1234,y
iny
cpy #namelen
bne -
clc
lda clearlistsl
adc #ScreenCols
sta clearlistsl
lda clearlistsh
adc #0
sta clearlistsh
dex
bne --
}
rts


; - redrawlist (SLOW)
; parameters:
;  selected:h->top entry
; returns:
;  menustate
;
redrawlist:
; check if top is reached
lda menustate
and #$f0
tay
lda #>tbl
cmp selectedh
bne +
lda #<tbl
cmp selected
bne +
; selected is the first entry
iny                 ; y bit 0 = 1
+ sty menustate     ; menustate = y
; store selected:h
lda selected
sta tmpptr
lda selectedh
sta tmpptrh
ldx #listtopy       ; set list y location
; print loop
- ldy #0
lda (selected),y    ; check if end reached
beq ++              ; jump if end
; print entry
ldy #listx          ; set list x location
jsr printentry      ; print
cpx #listbottomy    ; is bottom reached
beq +++             ; jump if it is
clc
lda selected        ; selected += entrylen
adc #entrylen
sta selected
bcc -
inc selectedh
bne -               ; loop
; end reached
++ lda menustate
ora #2              ; end reached, bit 1 = 1
sta menustate
cpx #listtopy+1     ; check if last reached
bne +               ; skip if not
ora #4              ; last reached, bit 2 = 1
+ sta menustate     ; menustate |= a
+++ lda tmpptr      ; restore selected:h
sta selected
lda tmpptrh
sta selectedh
rts


; - printentry
; parameters:
;  x = y-coordinate (row)
;  y = x-coordinate (column)
;  selected:h->text to print
; returns:
;  x,y = coord. of next line
;
printentry:
txa
pha
tya
pha         ; store x,y
clc         ; function: set cursor
jsr PLOT    ; set place to print
ldy #0
- lda (selected),y  ; load char
beq +       ; jump if 0
jsr CHROUT  ; print char
iny
cpy #namelen
bne -       ; loop entrylen times
+ pla
tay
pla
tax     ; restore x,y
inx     ; x++ (next row)
rts


!if disknameena + fastscrolle != 0 {
; - printoverentry
; parameters:
;  x = y-coordinate (row)
;  y = x-coordinate (column)
;  tmpptr:h->text to print
; returns:
;  x,y = coord. of line end
;
printoverentry:
clc         ; function: set cursor
jsr PLOT    ; set place to print
ldy #0
- lda (tmpptr),y    ; load char
beq +       ; jump if 0
jsr CHROUT  ; print char
iny
cpy #namelen
bne -
rts
+ lda #listbackgnd
- jsr CHROUT
iny
cpy #namelen
bne -
sec
jsr PLOT
rts
} ; disknameena + fastscrolle != 0


!if statusenabl = 1 {
; - printstatus
;
printstatus:
ldy #statusx
ldx #statusy
clc         ; function: set cursor
jsr PLOT    ; set place to print
ldy #(entrylen-1)
lda (selected),y
tax
ldy #0
- lda filetypes_print,x
jsr CHROUT
inx
iny
cpy #3
bne -
rts
}


!if fastscrolle = 1 {
; - scrolldown
;
scrolldown:
lda menustate
and #$f0
sta menustate
!if target = 128 {
	jsr PreScroll	;set window for scrolling
	jsr $ff7d	;Print Immediate
	!by $1d, $13	;right, home (fix pointers)
	!by $1b, $56	;ESC, V (scroll up)
	!by $13, $13	;home, home (restore full screen)
	!by 0		;end of string
} else {
lda #<screen+(listtopy+1)*ScreenCols+listx
sta scrolldownll
lda #>screen+(listtopy+1)*ScreenCols+listx
sta scrolldownlh
lda #<screen+(listtopy)*ScreenCols+listx
sta scrolldownsl
lda #>screen+(listtopy)*ScreenCols+listx
sta scrolldownsh
ldx #listbottomy-listtopy-1
-- ldy #namelen-1
- 
scrolldownll = *+1
scrolldownlh = *+2
lda $1234,y
scrolldownsl = *+1
scrolldownsh = *+2
sta $1234,y
dey
cpy #$ff
bne -
clc
lda scrolldownll
sta scrolldownsl
adc #ScreenCols
sta scrolldownll
lda scrolldownlh
sta scrolldownsh
adc #0
sta scrolldownlh
dex
bne --
}
jsr scrollcheckend      ; check if end
lda menustate
and #2
beq ++
lda #0
sta tmp
lda #<tmp
sta tmpptr
lda #>tmp
sta tmpptrh
++ lda tbllsth          ; check if selected = table last
cmp selectedh
bne +
lda tbllst
cmp selected
bne +
lda menustate
ora #4                  ; last reached, bit 2 = 1
sta menustate
bne +++                 ; if last, don't print
+ ldx #listbottomy-1
ldy #listx
jsr printoverentry
+++ ldy #0
rts


; - scrollup
;
scrollup:
lda menustate
and #$f0
sta menustate
!if target = 128 {
	jsr PreScroll
	jsr $ff7d	;Print Immediate
	!by $1d, $13	;right, home (fix pointers)
	!by $1b, $57	;ESC, W (scroll down)
	!by $13, $13	;home, home (restore full screen)
	!by 0		;end of string
} else {
lda #<screen+(listbottomy-2)*ScreenCols+listx
sta scrollupll
lda #>screen+(listbottomy-2)*ScreenCols+listx
sta scrolluplh
lda #<screen+(listbottomy-1)*ScreenCols+listx
sta scrollupsl
lda #>screen+(listbottomy-1)*ScreenCols+listx
sta scrollupsh
ldx #listbottomy-listtopy-1
-- ldy #namelen-1
- 
scrollupll = *+1
scrolluplh = *+2
lda $1234,y
scrollupsl = *+1
scrollupsh = *+2
sta $1234,y
dey
cpy #$ff
bne -
sec
lda scrollupll
sta scrollupsl
sbc #ScreenCols
sta scrollupll
lda scrolluplh
sta scrollupsh
sbc #0
sta scrolluplh
dex
bne --
}
jsr scrollcheckend      ; check if end
lda #>tbl               ; check if selected = table first
cmp selectedh
bne +
lda #<tbl
cmp selected
bne +
lda menustate
ora #1                  ; top reached, bit 0 = 1
sta menustate
+ lda selected
sta tmpptr
lda selectedh
sta tmpptrh
ldx #listtopy
ldy #listx
jsr printoverentry
+++ ldy #0
rts


; - scrollcheckend
; parameters:
;  selected:h
; returns:
;  menustate ( = a )
;  tmpptr
;
scrollcheckend:
lda #<(listbottomy-listtopy-1)*entrylen
sta tmp
lda #>(listbottomy-listtopy-1)*entrylen
sta tmp2
clc
lda selected
adc tmp
sta tmpptr
lda selectedh
adc tmp2
sta tmpptrh
lda tbllsth             ; check if table last >= tmpptr
cmp tmpptrh
bcc +
bne ++
lda tbllst
cmp tmpptr
bcs ++
+ lda menustate
ora #2               ; end reached, bit 1 = 1
sta menustate
++ rts
} ; fastscrolle


!if pleasewaite = 1 {
; - showpleasewait
; parameters:
;  selected:h->text to be shown
;
showpleasewait:
jsr clearlist
ldx #listtopy
ldy #listx
jsr printentry	; print filename
lda selected
pha
lda selectedh
pha
lda #<pleasewait
sta selected
lda #>pleasewait
sta selectedh
jsr printentry	; print "loading"
pla
sta selectedh
pla
sta selected
rts
}


; - parseext  
; parameters:
;  tmpptr:h+y->first char of extension
;  extension_list->list of extensions
;  a = extension length
;  x = extension max
; returns:
;  x = offset to next extension (or 0 if not found)
;
parseext:
sty tmp     ; store offset to first char
sta extension_len   ; store len
stx extension_max   ; store max
ldx #0
stx extension_limit ; store offset to extension
-- ldy tmp  ; y->first char of extension
lda extension_limit
tax         ; x->filetypes
clc
extension_len = *+1
adc #3      ; a->end of filetypes
sta extension_limit ; x->filetypes
extension_max = *+1
cpx #$12
beq +       ; extension not found
- lda (tmpptr),y
and #%01111111    ; compare shifted chars too, turn off bit 7 (Tap,TAP,tAP etc, all recognized) 
extension_list = *+1
cmp $1234,x
bne --
iny
inx
extension_limit = *+1
cpx #0
bne -
; match found (x->next extension)
rts
; no match found
+ ldx #0
rts


; - openclose
; parameters:
;  x:y->filename
;  a = filenamelen (or cmdlen - 3)
;
openclose:
clc
adc #3      ; add "cd:"
jsr SETNAM
lda #1      ; filenr
ldx device  ; unitnr
ldy #15     ; sec.address (,15)
jsr SETLFS
jsr OPEN    ; OPEN1,device,15,""
; error detection would be nice :)
lda #1
jsr CLOSE   ; CLOSE1
rts


; - loadlist
; parameters:
;  selected:h->text to be shown
;  tmpptr:h->dir/diskname as petscii
;  disknamepetlen = length of dir/diskname - 1
; returns:
;  tbl = the list
;  tbllst:h->end of list
;  selected->top of list (or remembered position)
;
loadlist:
!if disknameena = 1 & drivechange = 1 {
jsr cleardiskname
}
!if pleasewaite = 1 {
; clear list & show info
jsr showpleasewait
}

; prepare for extension parsing
lda #<filetypes
sta extension_list
lda #>filetypes
sta extension_list+1

; send open command
ldx tmpptr
ldy tmpptrh
lda disknamepetlen
jsr openclose

;!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
; Pi1541 Changes
;!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
; Changed dir and mounted disk
; We now need to wait for the drive

.trydir
!if(1){
; We now need to wait until the disk image can be loaded of the SD card and the drive can boot.
; Whilst we are loading of the SD card we cannot do any IEC bus activity so we fudge a wait here.
; Shouldn't need to do this if the "device not present" code below worked 100% of the time?
; Seems to need a delay before reattempting.
ldx #0
.waitloop1
ldy #0
.waitloop2
dey
bne .waitloop2
dex
bne .waitloop1
}

!if(1){
lda #$00
sta $90       ; clear STATUS flags
lda device
jsr LISTN
lda #$6F      ; secondary address 15 (command channel)
jsr SECOND
jsr UNLSN
lda $90       ; get STATUS flags
bne .trydir	; device not present
}
;!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

; load the list
lda #1              ; filenr
ldx device          ; unitnr
ldy #0              ; sec.address (0 = reloc)
jsr SETLFS
lda #1              ; filenamelen
ldx #<disklist
ldy #>disklist      ; x:y->filename ("$")
jsr SETNAM
lda #0              ; a = 0: load
!if rootentry = 0 {
ldx #<tbl 
} else {
ldx #<tbl+entrylen
}
stx selected
stx tmpptr
!if rootentry = 0 {
ldy #>tbl           ; load address = tbl (leave <- menu entries only)
} else {
ldy #>tbl+entrylen  ; load address = tbl + 17 (leave <- and // menu entries)
}
sty selectedh       ; selected:h->2nd entry in list
iny                 ; load address = tbl+256
sty tmpptrh         ; tmpptr:h->load address

!if drivechange = 1 {

;lda #0              ; clear dir entry (lda #0 for load is not required, A is 0 at this point)
ldy #128
cleardir_loop:
sta (tmpptr),y      ; clear dir entry
dey
bne cleardir_loop
ldy tmpptrh         ; tmpptr:h->load address (Restore Y value)
}

jsr LOAD
;!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
; Pi1541 Changes
;!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
bcc .done
jmp .trydir
.done
;!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

; - parse list
; skip first 6 bytes
clc
lda tmpptr
adc #6
sta tmpptr
bcc +
inc tmpptrh

+
!if disknameena = 1 {
; print disk name
lda #18
jsr CHROUT
ldx #disknamey
ldy #disknamex
jsr printoverentry
lda #146
jsr CHROUT
}

loadlist_loop: 
; search 0
ldy #0
- lda (tmpptr),y
beq +       ; if 0 -> next line
inc tmpptr
bne -
inc tmpptrh
bne -       ; loop until zero

; next entry (skips "_" on first time), set tbllst = current
+ clc
lda selected
sta tbllst
adc #<entrylen
sta selected
lda selectedh
sta tbllsth
adc #>entrylen
sta selectedh

; skip 5 bytes (0 + filelen&type)
+ clc
lda tmpptr
adc #5
sta tmpptr
bcc +
inc tmpptrh

; search for " (or 0 for last)
+
- lda (tmpptr),y
beq +++     ; if 0 -> "BLOCKS FREE" -> finished
cmp #quotechar
beq ++      ; if " -> start of filename found
inc tmpptr
bne -
inc tmpptrh
bne -       ; loop until zero

; error - no " or 0 found.

; finished
+++
; put end mark
sta (selected),y
lda #<tbl
sta selected
lda #>tbl
sta selectedh
rts

; copy name until last "
++ iny       ; y = 1, skip first "
ldx #0
- lda (tmpptr),y
cmp #quotechar
beq +       ; if ", ok
dey         ; y--, for selected
sta (selected),y
iny         ; restore y
iny         ; next y
bne -

; error - no " found.

; 0-pad to entrylen
+ lda #0
sty tmp2    ; tmp2 -> "
dey
- cpy #entrylen
beq +
sta (selected),y
iny
bne -

; skip ' ''s
+ ldy tmp2
iny         ; y++, skip last "
ldx #0
- lda (tmpptr),y
cmp #' '
bne +       ; jump if not ' '
iny         ; next y
bne -

; skip possible '*'
+ cmp #'*'
bne +
iny

; parse filetype
+ lda #3      ; extension len
ldx #filetypes_max  ; extension max
jsr parseext

; save extension to entry
ldy #(entrylen-1)
txa
sta (selected),y

jmp loadlist_loop









!if sortdir = 1 {

;
; Sort code by Mike from Denial Forum.
;

sortlist:

!if rootentry = 0 {

lda #<tbl+entrylen ; start list to sort (skip the first item <-)

sta tmpptr
lda #>tbl+entrylen ; starth list to sort
sta tmpptr+1

} else {

lda #<tbl+entrylen+entrylen ; start list to sort (skip the first 2 items <- and //)
sta tmpptr
lda #>tbl+entrylen+entrylen ; starth list to sort
sta tmpptr+1

}

lda #0
sta tmp3

sort_00:
clc
lda tmpptr
sta tmp
adc #entrylen   ;17
sta tmpptr
lda tmpptr+1
sta tmp+1
adc #0
sta tmpptr+1
lda tmp
cmp tbllst
lda tmp+1
sbc tbllsth
bcc sort_01
lda tmp3
bne sortlist
rts

sort_01:
ldy #0

sort_02:
lda (tmpptr),y
cmp (tmp),y
bcc sort_03
bne sort_00
iny
cpy #entrylen   ;17
bne sort_02
beq sort_00

sort_03:
lda #1
sta tmp3
ldy #0

sort_04:
lda (tmp),y
pha
lda (tmpptr),y
sta (tmp),y
pla
sta (tmpptr),y
iny
cpy #entrylen   ;17
bne sort_04
beq sort_00


}




!if target = 16 {

set_custom_fkeys_def_area:

;save current fkeys definition area

ldx #16
save_fkeys_def_area_loop:
lda $055f-1,X
sta FkeyStoreArea-1,x       
dex
bne save_fkeys_def_area_loop

;set custom fkeys definition area

ldx #8
redefine_fkeys_loop:
lda #1     
sta $055f-1,X      ; redefine fkeys lenght (F1-F8)

lda fkeysdef-1,x
sta $0567-1,X      ; redefine fkeys string (F1-F8)

dex
bne redefine_fkeys_loop

rts


restore_fkeys_def_area:
ldx #16
restore_fkeys_def_area_loop:
lda FkeyStoreArea-1,x
sta $055f-1,X
dex
bne restore_fkeys_def_area_loop
rts


GetJoyValueC16:
LDX #$07

JoyLoop:
LDA joya,X
STA $FD30
STA joyport
LDA joyport
STA joyb,X
DEX
BPL JoyLoop

LDA #$FF
STA $FD30

LDA #$FD     ; Latch value for port 2. (Use #$FA for Port 1)
STA joyport
LDA joyport  ; report the JoyPort value
rts

}

!if target = 128 {
; *** Routines for C128 ***

PRNINT:
;print .AX unsigned
;compare with $8e32... but that calls BASIC Low ROM which is not available
	sta $64	;mantissa bits 31~24
	stx $65 ;mantissa bits 23~16
	ldx #$90 ;exponent + 128
	sec		;positive mantissa
	jsr $8c75 ;normalize FAC
	jsr $8e44 ;FAC to string
	lda #0	;floatBuf (low)
	ldy #1	;floatBuf (high)
	
;print A/Y
PRNSTR:
	sta $ce
	sty $cf
	ldy #0
pstrlp:
	lda ($ce),y
	beq pstrex
	jsr CHROUT
	iny
	bne pstrlp
pstrex:
	rts

PreScroll:	;setup window for scrolling
	lda #2		;top row
	ldx #22		;bottom row
	sta $e5
	stx $e4
	lda #listx	;left column
	ldx #listx+namelen-1	;right column
	sta $e6
	stx $e7
	rts

set_custom_fkeys_def_area:
;save current fkeys definition area

ldx #20
save_fkeys_def_area_loop:
lda $1000-1,X
sta FkeyStoreArea-1,x       
dex
bne save_fkeys_def_area_loop

;set custom fkeys definition area
ldx #10
redefine_fkeys_loop:
lda #1     
sta $1000-1,X      ; redefine fkeys lenght (F1-F8, RUN, HELP)
lda fkeysdef-1,x
sta $100a-1,X      ; redefine fkeys string (F1-F8, RUN, HELP)
dex
bne redefine_fkeys_loop
rts


restore_fkeys_def_area:
ldx #20
restore_fkeys_def_area_loop:
lda FkeyStoreArea-1,x
sta $1000-1,X
dex
bne restore_fkeys_def_area_loop
rts

!if colorenable > 0 {
Default_Color:
	lda #13					;light green
	sta vicborder           ;$d020 default border color

	lda #11					;dark gray
	sta vicbackgnd          ;$d021 default background color

	lda #$1a				;VDC color register
	sta $d600				;request register
	lda $d601				;read value
	and #$f0				;keep high nibble
	;ora #0					;black
	sta $d601				;set background color

	lda #$99				;light green (assume VIC-II)
	bit $d7					;test video mode
	bpl +					;good assumption
	lda #$9f				;cyan (for VDC)
+	jmp CHROUT              ;set cursor color
}
}



!if target = 64 {
!if variant = 2 {
;C64-DTV

;Copy the JiffyDOS detection & install routine

jiffydtv_start:

;DASM SYNTAX
;#rorg loadrunpos

;ACME SYNTAX
!pseudopc loadrunpos {


	jsr	check_for_jiffydtv_softkernal
	
	beq	load_JiffyDTV

	jsr     active_JiffyDTV
        rts

        
load_JiffyDTV:     ;load "JIFFYDTV" from flash if exist


; Store current program on $6000 so it could be restored after the "JIFFYDTV" load.
	LDX #$08      ; copy from F9-FA = $800 
	LDY #$60      ;        to FB-FC = $6000 
	jsr CopyBytes 



	lda #1      ; filenr
	ldx #1      ; unitnr
	ldy #1      ; sec.address (,1)
	jsr SETLFS

	lda #8              ; a = filenamelen ("jiffydtv")
	ldx #<jiffydtv_name
	ldy #>jiffydtv_name ; x:y->filename address
	jsr SETNAM


	lda #0              ; A: 0 = Load, 1-255 = Verify
        jsr LOAD

        bcs JiffyDTV_LoadError  ; Carry: 0 = No errors, 1 = Error; 
	
	lda #$60
	sta $0843         ; patch the JIFFY INSTALL code to avoid bank swap and reset

        jsr $080d         ; and start the INSTALL JIFFY Routine

	jsr active_JiffyDTV

        jmp RestoreProgram

JiffyDTV_LoadError:            
;       lda #$02
;       sta vicborder      ;   POKE 53280,02 (Red) 

RestoreProgram:
	LDX #$60      ; copy from F9-FA = $6000 
	LDY #$08      ;        to FB-FC = $800 
        
	jsr CopyBytes ; Restore program.

	rts


active_JiffyDTV:
	ldx #1
	stx $d03f
        LDA #$5E
        STA $D100   ; $5E %01-011110 01=RAM 011110 = $1e  (kernal in RAM $1E0000)
        LDA #$00
        STA $D03F
	JSR $FD15   ; set JiffyDOS pointers

        ;JiffyDOS actived
;       lda #$05
;       sta vicborder   ;   POKE 53280,05 (Green) 
	rts



CopyBytes:
	STX $FA     ; copy from/to FA-FC hi-byte
	STY $FC

	LDX #$20    ; blocks to copy ($20 = $2000 bytes)
	LDY #$00    
	STY $F9
	STY $FB     ; copy from/to F9/FB lo-byte fixed 0


CopyBytesLoop:
	LDA ($F9),Y
	STA ($FB),Y
	INY
	BNE CopyBytesLoop

	INC $FA
	INC $FC
	DEX
	BNE CopyBytesLoop
        RTS



jiffydtv_name:

;DASM SYNTAX
;	.byte "JIFFYDTV"

;ACME SYNTAX	
	!tx "jiffydtv"




;**************************************************************************
;*
;* NAME  check_for_kernal (based on DTVmenu from tlr) 
;*
;* DESCRIPTION
;*   Check if there is a kernal present at $1ee000.
;*   z=1, means no kernal
;*
;******
check_for_jiffydtv_softkernal:

;	sir	$e8
;DASM SYNTAX
;	.byte $42,$e8           ;sir	$e8
;ACME SYNTAX	
        !by   $42,$e8           ;sir	$e8

	ldy	#$1ee000/$4000
	ldx	#%01010101	;all ram
;	sir	$12
;DASM SYNTAX
;	.byte $42,$12           ;sir	$12
;ACME SYNTAX	
        !by   $42,$12           ;sir	$12


	
       


; is the vectors all $ff's?
	ldx	#5
	lda	#$ff


	ldy $bd4c
        cpy #$a5
        bne check_kernal_exit  ;fd4c  a5 f4 (JIFFYDOS load Address pointers in ROM lo-byte)

	ldy $bd4d
        cpy #$f4
        bne check_kernal_exit  ;fd4c  a5 f4 (JIFFYDOS load Address pointers in ROM hi-byte)


cfk_lp1:                       ; check also some vectors to be sure they are not all $ff 
	and	$bffa,x
	dex
	bpl	cfk_lp1

check_kernal_exit:

;	sir	$e8
;DASM SYNTAX
;	.byte $42,$e8           ;sir	$e8
;ACME SYNTAX	
        !by   $42,$e8           ;sir	$e8

	ldy	#$02
	ldx	#%01010101	;all ram
;	sir	$12
;DASM SYNTAX
;	.byte $42,$12           ;sir	$12
;ACME SYNTAX	
        !by   $42,$12           ;sir	$12

	cmp	#$ff		;Z=1 is all $ff's.
	rts
;DASM SYNTAX
;#rend

;ACME SYNTAX (Close code #rorg for loadrunpos[$0334])
}

jiffydtv_end:

}
}


; --- Variables

; device number (8,9,...)
device !by hwdevicenum

; last joystick state (for edge detection)
lastjoy !by $ff


!if sortdir = 1 {

; sort flag used for sortlist routine
sortflag !by $0

}


!if target = 20 {

!if startmode = 1 {

VicMode !by $0

}

}


; --- Strings

;    |---------0---------0---------0--------|

!if target = 16 {
fkeysdef:
!tx 133,137,134,138,135,139,136,140

joya:   
!tx $fe,$fd,$fb,$f7,$ef,$df,$bf,$7f

joyb:
!tx $0,$0,$0,$0,$0,$0,$0,$0

}

!if target = 128 {
fkeysdef:
!tx 133,137,134,138,135,139,136,140,131,132
}


!if printbars = 1 {


!if drivechange = 1 {
drive_text:
!if variant = 2 {

;C64-DTV (DTV_D) button

!tx $12,"d",$92,"rive",0
} else {

!tx $12,"d",$92,"rive",0
}

}


!if key_quit < 255 {
quit_text:
!tx $12,"q",$92,"uit",0
}

!if variant = 2 {

;C64-DTV 

cdback_text:
!tx $12,"rgbtn",$92,"cd",leftarrowchar,0

}



!if target = 20 & startmode = 1 {

manualOFF_text:
!tx $12,"m",$92,"anual",0

manualON_text:
!tx $12,"m",$92

!if buttoncolor>4 {
!tx buttoncolor
}

!tx "anual"

!if forecolor>4 {
!tx forecolor
}

!tx 0

}

!if sortdir = 1 {

!if variant = 2 {

;C64-DTV (DTV_C) button

sortOFF_text:
!tx $12,"c",$92,"sort",0

sortON_text:
!tx $12,"c",$92
!if buttoncolor>4 {
!tx buttoncolor
}

!tx "sort"

!if forecolor>4 {
!tx forecolor
}


} else {

sortOFF_text:
!tx $12,"s",$92,"ort",0

sortON_text:
!tx $12,"s",$92
!if buttoncolor>4 {
!tx buttoncolor
}

!tx "ort"

!if forecolor>4 {
!tx forecolor
}

}
!tx 0

}

}




!if target = 20 & startmode = 1 {


memconfig_text

!if vicmemcfg = 3 | vicmemcfg = 8 {

!if forecolor>4 {
!tx forecolor
}
!tx $0d,$0d,"mem config:",$0d

!if buttoncolor>4 {
!tx buttoncolor
}
!tx "f1"
!if forecolor>4 {
!tx forecolor
}
!tx " unexpanded",$0d

!if buttoncolor>4 {
!tx buttoncolor
}
!tx "f3"
!if forecolor>4 {
!tx forecolor
}
!tx " +3k",$0d
}

!if vicmemcfg = 8 {

!if buttoncolor>4 {
!tx buttoncolor
}

!tx "f5"
!if forecolor>4 {
!tx forecolor
}
!tx " +32k",$0d


!if variant = 1 {
!if buttoncolor>4 {
!tx buttoncolor
}
!tx "f7"
!if forecolor>4 {
!tx forecolor
}
!tx " +32k r/only",$0d
}
!if buttoncolor>4 {
!tx buttoncolor
}
!tx "f8"
!if forecolor>4 {
!tx forecolor
}
!tx " +32k +3k"
}
!tx $0

startconfig_text
!if forecolor>4 {
!tx forecolor
}
!tx $0d,$0d,"start mode:",$0d
!if buttoncolor>4 {
!tx buttoncolor
}
!tx "f1"
!if forecolor>4 {
!tx forecolor
}
!tx " prompt",$0d

!if buttoncolor>4 {
!tx buttoncolor
}
!tx "f3"
!if forecolor>4 {
!tx forecolor
}
!tx " run",$0d

!if buttoncolor>4 {
!tx buttoncolor
}
!tx "f5"
!if forecolor>4 {
!tx forecolor
}
!tx " reset",$0d

!if buttoncolor>4 {
!tx buttoncolor
}
!tx "f7"
!if forecolor>4 {
!tx forecolor
}
!tx " restart browser",$0

!if variant = 1 {

SJLOADconfig_text
!if forecolor>4 {
!tx forecolor
}
!tx $0d,$0d,"sjload:",$0d
!if buttoncolor>4 {
!tx buttoncolor
}
!tx "f1"
!if forecolor>4 {
!tx forecolor
}
!tx " yes",$0d
!if buttoncolor>4 {
!tx buttoncolor
}
!tx "f3"
!if forecolor>4 {
!tx forecolor
}
!tx " no",$0

}

}



extensions
!tx "dnp",0   ; this extension must be the first for submenu logic of this file image
!if tap_support = 1 {
!tx "tap",0   ; this extension must be the second for submenu logic of this file image
}
!tx "d64",0
!tx "d71",0
!tx "d81",0
!tx "m2i",0
!tx "d41",0
;!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
; Pi1541 Changes
;!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!tx "g64",0
!tx "nib",0
!tx "nbz",0
!tx "lst",0
!tx "t64",0
;!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
extensions_max = * - extensions

filetypes_print
!tx "cd",leftarrowchar
filetypes
!tx "dir"
!tx "del"
!tx "seq"
!tx "prg"
!tx "usr"
!tx "rel"
!tx "cbm"
filetypes_max = * - filetypes

!if pleasewaite = 1 {
pleasewait
!tx "loading...",0
}

!if tap_support = 1 {
tapname     ; "xt:0123456789abcdef",0
!tx "xt:",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
}

disklist
!tx "$"

diskcmdroot
!tx "cd//",0

diskcmdcd   ; "cd:0123456789abcdef",0
!tx "cd:"
disknamepet ; "0123456789abcdef",0
!tx 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0

diskcmdexit ; "cd:_"
!tx "cd:"


!if target = 20 {

!if variant = 1 {

!tx leftarrowchar

tblbasestart
!tx 0,0,0  ; remove any BASIC LINE on $1200 

;tbl = $1203

} else {

; --- Program table

tbl

}

} else {

; --- Program table

tbl
}

; "prev dir" entry is always present
!tx leftarrowchar,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 ; name
!by 0 ; type

!if rootentry = 1 {
; "exit to root" entry 
!tx "//",0,0,0,0,0,0,0,0,0,0,0,0,0,0 ; name
!by 0 ; type
}



tblbaseend

!if target = 20 {
!if variant = 1 {

mc_program_end:

mc_program_length = mc_program_end - mc_program_start

;DASM SYNTAX
;mc_program_Pages  = >[mc_program_length+255]

;ACME SYNTAX
mc_program_Pages  = >(mc_program_length+255)



}
}

progsize = * - entry

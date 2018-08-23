# Pi1541 supported IEC commands in browser mode.

inspired by sd2iec [https://www.sd2iec.de](https://www.sd2iec.de)


Introduction:
=============

Supported commands:
===================
- General notes:
  Any command not listed below is currently not supported.

- Directory filters:
  To show only directories, both =B (CMD-compatible) and =D can be used.
  On a real Commodore drive D matches everything.
  To include hidden files in the directory, use *=H - on a 1541 this doesn't
  do anything. sd2iec marks hidden files with an H after the lock mark,
  i.e. "PRG<H" or "PRG H".

  CMD-style "short" and "long" directory listings with timestamps are supported
  ("$=T"), including timestamp filters. Please read a CMD manual for the syntax
  until this file is updated.

- Partition directory:
  Partitions ARE NOT SUPPORTED by Pi1541.

- CD/MD/RD:
  Subdirectory access is compatible to the syntax used by the CMD drives,
  although drive/partition numbers are completely ignored.

  Quick syntax overview:
    CD:_         changes into the parent dir (_ is the left arrow on the C64)
    CD_          dito
    CD:foo       changes into foo
    CD/foo       dito
    CD//foo      changes into \foo
    CD/foo/:bar  changes into foo\bar
    CD/foo/bar   dito

  You can use wildcards anywhere in the path. To change into an M2I or D64
  image the image file must be the last component in the path, either
  after a slash or a colon character.

  MD uses a syntax similiar to CD and will create the directory listed
  after the colon (:) relative to any directory listed before it.

    MD/foo/:bar  creates bar in foo
    MD//foo/:bar creates bar in \foo

  RD can only remove subdirectories of the current directory.

    RD:foo       deletes foo

  CD is also used to mount/unmount image files. Just change into them
  as if they were a directory and use CD:_ (left arrow on the C64) to leave.
  Please note that image files are detected by file extension and file size
  and there is no reliable way to see if a file is a valid image file.

- CP, C<Shift-P>
  Partitions ARE NOT SUPPORTED by Pi1541.

- C:
  File copy command, should be CMD compatible. The syntax is
   C[partition][path]:targetname=[[partition][path]:]sourcename[,[[p][p]:]sourcename...]
  You can use this command to copy multiple files into a single target
  file in which case all source files will be appended into the target
  file. Parsing restarts for every source file name which means that
  every source name is assumed to be relative to the current directory.
  You can use wildcards in the source names, but only the first
  file matching will be copied.

  Copying REL files should work, but isn't tested well. Mixing REL and
  non-REL files in an append operation isn't supported.

- DI, DR, DW
  Direct sector access IS NOT SUPPORTED by Pi1541.

- G-P
  Partitions ARE NOT SUPPORTED by Pi1541.

- P
  Positioning doesn't just work for REL files but also for regular
  files on a FAT partition. When used for regular files the format
  is "P"+chr$(channel)+chr$(lo)+chr$(midlo)+chr$(midhi)+chr$(hi)
  which will seek to the 0-based offset hi*2^24+midhi*65536+256*midlo+lo
  in the file. If you send less than four bytes for the offset, the
  missing bytes are assumed to be zero.

- N:
  Format works by automatically creating a new D64 image.

- R
  Renaming files should work the same as it does on CMD drives, although
  the errors flagged for invalid characters in the name may differ.

- S:
  Name matching is fully supported, directories are ignored.
  Scratching of multiple files separated by , is also supported with no
  limit to the number of files except for the maximum command line length
  (usually 100 to 120 characters).

- T-R and T-W
  If your hardware features RTC support the commands T-R (time read) and T-W
  (time write) are available. If the RTC isn't present, both commands return
  30,SYNTAX ERROR,00,00; if the RTC is present but not set correctly T-R will
  return 31,SYNTAX ERROR,00,00.

  Both commands expect a fourth character that specifies the time format
  to be used. T-W expects that the new time follows that character
  with no space or other characters inbetween. For the A, B and D
  formats, the expected input format is exactly the same as returned
  by T-R with the same format character; for the I format the day of
  week is ignored and calculated based on the date instead.

  The possible formats are:
   - "A"SCII: "SUN. 01/20/08 01:23:45 PM"+CHR$(13)
      The day-of-week string can be any of "SUN.", "MON.", "TUES", "WED.",
      "THUR", "FRI.", "SAT.". The year field is modulo 100.

   - "B"CD or "D"ecimal:
     Both these formats use 9 bytes to specify the time. For BCD everything
     is BCD-encoded, for Decimal the numbers are sent/parsed as-is.
      Byte 0: Day of the week (0 for sunday)
           1: Year (modulo 100 for BCD; -1900 for Decimal, i.e. 108 for 2008)
           2: Month (1-based)
           3: Day (1-based)
           4: Hour   (1-12)
           5: Minute (0-59)
           6: Second (0-59)
           7: AM/PM-Flag (0 is AM, everything else is PM)
           8: CHR$(13)

      When the time is set a year less than 80 is interpreted as 20xx.

   - "I"SO 8601 subset: "2008-01-20T13:23:45 SUN"+CHR$(13)
     This format complies with ISO 8601 and adds a day of week
     abbreviation using the same table as the A format, but omitting
     the fourth character. When it is used with T-W, anything beyond
     the seconds field is ignored and the day of week is calculated
     based on the specified date. The year must always be specified
     including the century if this format is used to set the time.
     To save space, sd2iec only accepts this particular date/time
     representation when setting the time with T-WI and no other ISo
     8601-compliant representation.

- U0
  Device address changing with "U0>"+chr$(new address) is supported,
  other U0 commands are currently not implemented.

- U1/U2/B-R/B-W
  Block reading and writing is NOT SUPPORTED by Pi1541 in browser mode.

- B-P
  NOT SUPPORTED by Pi1541 in browser mode.
  
- UI+/UI-
  Switching the slightly faster bus protocol for the VC20 on and off works,
  it hasn't been tested much though.

- UI/UJ
  Soft/Hard reset - UI just sets the "73,..." message on the error channel,
  UJ closes all active buffers but doesn't reset the current directory,
  mounted image, swap list or anything else.

- U<Shift-J>
  Real hard reset - this command causes a restart of the Raspberry Pi processor
  <Shift-J> is character code 202.

- X: Extended commands.
  Extended commands are NOT SUPPORTED by Pi1541 in browser mode.

- M-R, M-W, M-E
  Memory read, write and execute are NOT SUPPORTED by Pi1541 in browser mode.

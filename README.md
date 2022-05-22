
# RetroFileTool
Utility for converting between various retro file formats.

# Input File Types

 - Intel HEX
 - Raw Binary

# Output File Types
- MOS Technology paper tape format (PAP) (KIM-1 and its clones)
- WDC binary file format (for use with the WDC simulator and debugger)

# Usage

> $ ./RetroFileTool.exe Retro file conversion utility, Timothy Alicie,
> 2017-2022, v1.0.
> 
> Usage: RetroFileTool [GLOBAL_OPTIONS] [-if{h | b} INPUT_FILE[,IN_FILE_OPTS] ...] -of{p | w} OUTPUT_FILE[,OUT_FILE_OPTS]

## GLOBAL_OPTIONS:
Currently none supported.

## Input Files

    -ifh              The input file is of type Intel HEX.
    -ifb              The input file is of type raw binary.

**INPUT_FILE**        The input file name.

## IN_FILE_OPTS
Options for this input file.

### For Intel HEX files:
No options currently supported.
    
### For raw binary files:
    A=ADDR         The starting address of the file.
The address may be specified in decimal, in hex by prepending `0x`, or in hex by prepending `$`.

## Output Files
	-ofp              The output file is of type MOS paper tape.
	-ofw              The output file is of type WDC binary. OUTPUT_FILE       The output file name.

## OUT_FILE_OPTS
Options for this output file.

### For MOS paper tape files: 
No options currently supported.

### For WDC binary files:
No options currently supported.

## Notes
Multiple input files are supported, and the types may be freely mixed. For example, you can input several different binary files into one output image, or you could load a binary file and an Intel HEX file.

Only one output file is supported.

## Examples:

`RetroFileTool -ifh inFile.hex -ofp outFile.pap`

`RetroFileTool -ifb inFile.bin,A=0x200 -ofw outFile.wdc.bin`

`RetroFileTool -ifb inFile1.bin,A=0x200 -ifb inFile2.bin,A=0x8000 -ifh inFile3.hex -ofw outFile.wdc.bin`

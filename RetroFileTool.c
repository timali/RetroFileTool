/*********************************************************************//** @file
Utility for converting between various retro file formats.
******************************************************************************/

/******************************************************************************
 Include Files
******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/******************************************************************************
 Defines
******************************************************************************/

/** The maximum number of bytes in each PAP record. */
#define PAP_REC_LEN                                               24

#define VER_STR                                                   "1.0"

/******************************************************************************
 Module Typedefs and Enums
******************************************************************************/

/** An unsigned 32-bit integer. Change this to match your platform. */
typedef unsigned int    U32;

/** An unsigned 16-bit integer. Change this to match your platform. */
typedef unsigned short  U16;

/** An unsigned 8-bit integer. Change this to match your platform. */
typedef unsigned char   U8;

/** The different file types supported. */
typedef enum
{
   FILE_TYPE_HEX,
   FILE_TYPE_BIN,
   FILE_TYPE_WDC,
   FILE_TYPE_PAP,

} FILE_TYPE;

/** The different error codes for the return values. */
typedef enum
{
   OK                      = 0,
   USAGE_SHOWN,
   UNSUPPORTED,
   INVALID_ARGUMENTS,
   CANNOT_OPEN_FILE,
   END_OF_FILE,
   IO_ERROR,
   INVALID_DATA,
   MIXED_ADDRESSING_MODES,
   INVALID_RECORD_TYPE,
   END_RECORD_ERROR,
   CHECKSUM_ERROR,
   NO_MEMORY,
   OVERLAPPING_SEGMENT,

} RESULT;

/** The different types of Intex HEX records. */
typedef enum
{
   REC_DATA                = 0,
   REC_EOF                 = 1,
   REC_EXT_SEG_ADDR        = 2,
   REC_START_SEG_ADDR      = 3,
   REC_EXT_LIN_ADDR        = 4,
   REC_START_LIN_ADDR      = 5,

} HEX_RECORD_TYPE;

/** File options for the raw binary file type. */
typedef struct _FILE_OPTS_BIN_ FILE_OPTS_BIN;
struct _FILE_OPTS_BIN_
{
   /** The starting address of the binary file. */
   U32                     startAddr;

   /** Whether or not the address was specified. */
   int                     addrSpecified;
};

/** Describes a file for conversion. */
typedef struct _DATA_FILE_ DATA_FILE;
struct _DATA_FILE_
{
   /** The type of file. */
   FILE_TYPE               type;

   /** The file name. */
   const char              *pName;

   /** Options for this file. */
   void                    *pOpts;

   /** The next file in the list of files. */
   struct _DATA_FILE_      *pNext;
};

/** Describes a single contiguous region of memory. */
typedef struct _SEGMENT_ SEGMENT;
struct _SEGMENT_
{
   /** The starting address of the segment. */
   U32                     addr;

   /** The length of the segment, in bytes. */
   U32                     len;

   /** The next segment is adjacent to this one. */
   SEGMENT*                pNext;

   /** The actual segment data. */
   U8                      data[];
};

/** A collection of SEGMENTS which make up a contiguous range in memory. */
typedef struct _RANGE_ RANGE;
struct _RANGE_
{
   /** The starting address of the range. */
   U32                     addr;

   /** The length of the range, in bytes. */
   U32                     len;

   /** The first segment in this range. */
   SEGMENT                 *pSegStart;

   /** The last segment in this range. */
   SEGMENT                 *pSegEnd;

   /** The next range, which is not necessarily adjacent. */
   RANGE                   *pNext;
};

/******************************************************************************
 Module Variables.
******************************************************************************/

/** All the ranges contained within the input files. */
static RANGE*              pAllRanges = NULL;

/** The number of contiguous ranges allocated so far. */
static U16                 numRanges = 0;

/** The number of data bytes in the image. */
static U32                 dataBytes = 0;

/** The program's execution starting address. */
static U32                 startAddr = 0;

/** The input files. */
static DATA_FILE           *pInFiles = NULL;

/** The output file. */
static DATA_FILE           *pOutFile = NULL;

/******************************************************************************
 Module Function Definitions
******************************************************************************/

/**************************************************************************//**
* Displays the program's usage.
*
* @return None.
******************************************************************************/
static void PrintUsage(void)
{
   printf("Supported input file formats:\n");
   printf("   * HEX: Intel HEX\n");
   printf("   * BIN: Raw binary\n");
   printf("\n");

   printf("Supported output file formats:\n");
   printf("   * PAP: MOS Technology paper tape (KIM-1)\n");
   printf("   * WDC: WDC binary\n");
   printf("\n");

   printf("Usage: RetroFileTool [GLOBAL_OPTIONS] \\\n");
   printf("   [-if{h | b} INPUT_FILE[,IN_FILE_OPTS] ...] \\\n");
   printf("   -of{p | w} OUTPUT_FILE[,OUT_FILE_OPTS]\n");
   printf("\n");

   printf("GLOBAL_OPTIONS    Currently none supported.\n");
   printf("\n");

   printf("-ifh              The input file is of type Intel HEX.\n");
   printf("-ifb              The input file is of type raw binary.\n");
   printf("INPUT_FILE        The input file name.\n");
   printf("IN_FILE_OPTS      Options for this input file.\n");
   printf("\n");

   printf("IN_FILE_OPTS\n");
   printf("\n");
   printf("For Intel HEX files:\n");
   printf("   No options currently supported.\n");
   printf("\n");
   printf("For raw binary files:\n");
   printf("   A=ADDR         The starting address of the file.\n");
   printf("\n");

   printf("-ofp              The output file is of type MOS paper tape.\n");
   printf("-ofw              The output file is of type WDC binary.\n");
   printf("OUTPUT_FILE       The output file name.\n");
   printf("\n");

   printf("OUT_FILE_OPTS     Options for this output file.\n");
   printf("\n");

   printf("For MOS paper tape files:\n");
   printf("   No options currently supported.\n");
   printf("\n");
   printf("For WDC binary files:\n");
   printf("   No options currently supported.\n");
   printf("\n");

   printf("Multiple input files are supported, and the types may be freely mixed.\n");
   printf("For example, you can input several different binary files into one output\n");
   printf("image, or you could load a binary file and an Intel HEX file.\n");
   printf("\n");
   printf("Only one output file is supported.\n");
   printf("\n");

   printf("Examples:\n");
   printf("\n");
   printf("RetroFileTool -ifh inFile.hex -ofp outFile.pap\n");
   printf("RetroFileTool -ifb inFile.bin,A=0x200 -ofw outFile.wdc.bin\n");
   printf("RetroFileTool -ifb inFile1.bin,A=0x200 -ifb inFile2.bin,A=0x8000 -ifh inFile3.hex -ofw outFile.wdc.bin\n");
   printf("\n");
}

/**************************************************************************//**
* Reads an ASCII-encoded byte from the given file.
*
* @param[in] inFile The file from which to read.
* @param[in] pU8 The data will be stored here.
* @param[in] pChkSum A checksum variable to update, or NULL to not update anything.
*
* @return An RESULT indicating success or failure.
******************************************************************************/
static RESULT LoadU8(FILE* inFile, U8* pU8, U8* pChkSum)
{
   int i, inByte[2];

   for (i = 0; i < 2; i++)
   {
      inByte[i] = fgetc(inFile);
      if (inByte[i] == EOF)
      {
         printf("Unexpected end of file.\n");
         return END_OF_FILE;
      }
      else if (inByte[i] >= '0' && inByte[i] <= '9')
      {
         inByte[i] -= '0';
      }
      else if (inByte[i] >= 'a' && inByte[i] <= 'f')
      {
         inByte[i] = (inByte[i] - 'a') + 10;
      }
      else if (inByte[i] >= 'A' && inByte[i] <= 'F')
      {
         inByte[i] = (inByte[i] - 'A') + 10;
      }
      else
      {
         printf("Invalid hex byte value.\n");
         return INVALID_DATA;
      }
   }

   *pU8 = (inByte[0] << 4) | inByte[1];
   if (pChkSum != NULL) *pChkSum += *pU8;

   return OK;
}

/**************************************************************************//**
* Reads an ASCII-encoded U16 from the given file, in MSB.
*
* @param[in] inFile The file from which to read.
* @param[in] pU16 The data will be stored here.
* @param[in] pChkSum A checksum variable to update, or NULL to not update anything.
*
* @return An RESULT indicating success or failure.
******************************************************************************/
static RESULT LoadU16(FILE* inFile, U16* pU16, U8* pChkSum)
{
   RESULT r;
   U8 b1, b2;

   r = LoadU8(inFile, &b1, pChkSum);
   if (r != OK)
   {
      return r;
   }

   r = LoadU8(inFile, &b2, pChkSum);
   if (r != OK)
   {
      return r;
   }

   *pU16 = (b1 << 8) | b2;
   return OK;
}

/**************************************************************************//**
* Reads an ASCII-encoded U32 from the given file, in MSB.
*
* @param[in] inFile The file from which to read.
* @param[in] pU32 The data will be stored here.
* @param[in] pChkSum A checksum variable to update, or NULL to not update anything.
*
* @return An RESULT indicating success or failure.
******************************************************************************/
static RESULT LoadU32(FILE* inFile, U32* pU32, U8* pChkSum)
{
   RESULT r;
   int i;
   U8 b;

   *pU32 = 0;
   for (i = 0; i < 4; i++)
   {
      r = LoadU8(inFile, &b, pChkSum);
      if (r != OK)
      {
         return r;
      }
      *pU32 = ((*pU32) << 8) | b;
   }

   return OK;
}

/**************************************************************************//**
* Concatenates any contiguous ranges.
*
* Two ranges may grow to be adjacent, so concatenate them if so.
*
* @return None.
******************************************************************************/
static void CombineRanges(void)
{
   RANGE* pRange = pAllRanges;
   RANGE* pNext = pRange->pNext;

   while (pRange && pNext)
   {
      if (pRange->addr + pRange->len == pNext->addr)
      {
         numRanges--;
         pRange->len += pNext->len;
         pRange->pSegEnd->pNext = pNext->pSegStart;
         pRange->pSegEnd = pNext->pSegEnd;
         pRange->pNext = pNext->pNext;
         free(pNext);
      }
      pRange = pRange->pNext;
      if (pRange)
      {
         pNext = pRange->pNext;
      }
   }
}

/**************************************************************************//**
* Adds a new segment into the data structures.
*
* @param[in] pSeg The segment to add.
*
* @return An RESULT indicating success or failure.
******************************************************************************/
static RESULT AddSegment(SEGMENT* pSeg)
{
   RANGE* pRange, * pTemp, ** ppPrev;
   U32 segStart, segEnd, rangeStart, rangeEnd;

   if (pSeg->len == 0)
   {
      return OK;
   }

   dataBytes += pSeg->len;

   segStart = pSeg->addr;
   segEnd = segStart + pSeg->len - 1;

   /* See if the segment is adjacent to any existing ranges. */
   pRange = pAllRanges;
   while (pRange)
   {
      rangeStart = pRange->addr;
      rangeEnd = rangeStart + pRange->len - 1;

      /* Make sure the new segment does not overlap an existing range. */
      if ((segStart >= rangeStart && segStart <= rangeEnd) ||
         (segEnd >= rangeStart && segEnd <= rangeEnd))
      {
         printf("A segment overlapps a previous segment.\n");
         return OVERLAPPING_SEGMENT;
      }

      /* See if the new segment is contiguous with an existing range. */
      if ((segEnd + 1) == rangeStart)
      {
         /* The new segment immediately preceeds this range. */
         pRange->addr = pSeg->addr;
         pRange->len += pSeg->len;
         pSeg->pNext = pRange->pSegStart;
         pRange->pSegStart = pSeg;

         return OK;
      }
      else if ((rangeEnd + 1) == segStart)
      {
         /* The new segment immediately follows this range. */
         pRange->len += pSeg->len;
         pRange->pSegEnd->pNext = pSeg;
         pRange->pSegEnd = pSeg;
         pSeg->pNext = NULL;

         return OK;
      }

      pRange = pRange->pNext;
   }

   /* The segment is a new range, so create one. */
   numRanges++;
   pRange = (RANGE*)malloc(sizeof(RANGE));
   if (pRange == NULL)
   {
      printf("Out of memory.\n");
      return NO_MEMORY;
   }

   /* Fill in the new range data. */
   pRange->addr = pSeg->addr;
   pRange->len = pSeg->len;
   pRange->pSegStart = pSeg;
   pRange->pSegEnd = pSeg;
   pSeg->pNext = NULL;

   /* Insert the new range and maintain sorted order. */
   ppPrev = &pAllRanges;
   pTemp = *ppPrev;
   while (pTemp && pTemp->addr < pRange->addr)
   {
      ppPrev = &pTemp->pNext;
      pTemp = pTemp->pNext;
   }
   *ppPrev = pRange;
   pRange->pNext = pTemp;

   return OK;
}

/**************************************************************************//**
* Loads a raw binary file into RAM.
*
* @param[in] inFile The file object to read from.
* @param[in] pOpts The file options for this file type.
*
* @return An RESULT indicating success or failure.
******************************************************************************/
static RESULT LoadBinFile(FILE* inFile, FILE_OPTS_BIN *pOpts)
{
   printf("a raw binary file, addr=0x%0X.\n", pOpts->startAddr);

   printf("Currently not supported.\n");
   return UNSUPPORTED;
}

/**************************************************************************//**
* Loads an Intel HEX file into RAM.
*
* @param[in] inFile The file object to read from.
*
* @return An RESULT indicating success or failure.
******************************************************************************/
static RESULT LoadHexFile(FILE* inFile)
{
   RESULT r;
   char inChar;
   U8 recType, byteCount;
   U8 chkSumActual, chkSumFile;
   U16 addr16, extAddr = 0, segAddr = 0;
   U32 i;
   SEGMENT* pSeg;
   U8 endRecordFound = 0;

   printf("an Intel HEX file.\n");

   while (1)
   {
      /* Find a record, which always starts with a ':'. */
      while (((inChar = fgetc(inFile)) != ':') && !feof(inFile));
      if (inChar == EOF)
      {
         break;
      }

      /* There should only be one end record at the very last entry. */
      if (endRecordFound)
      {
         printf("Multiple end records encountered.\n");
         return END_RECORD_ERROR;
      }

      chkSumActual = 0;

      /* Read the byte count. */
      r = LoadU8(inFile, &byteCount, &chkSumActual);
      if (r != OK)
      {
         return r;
      }

      /* Read the 16-bit address. */
      r = LoadU16(inFile, &addr16, &chkSumActual);
      if (r != OK)
      {
         return r;
      }

      /* Read the record type. */
      r = LoadU8(inFile, &recType, &chkSumActual);
      if (r != OK)
      {
         return r;
      }

      /* Handle the record. */
      switch (recType)
      {
         case REC_DATA:
         {
            /* Allocate a new segment to hold the data. */
            pSeg = (SEGMENT*)malloc(sizeof(SEGMENT) + byteCount);
            if (pSeg == NULL)
            {
                  printf("Out of memory.\n");
                  return NO_MEMORY;
            }

            /* Set the segment's info depending on which addressing mode is used. */
            if (segAddr != 0)
            {
                  pSeg->addr = (segAddr << 4) + addr16;
            }
            else
            {
                  pSeg->addr = (extAddr << 16) | addr16;
            }
            pSeg->len = byteCount;

            /* Read the data into the segment. */
            for (i = 0; i < byteCount; i++)
            {
                  r = LoadU8(inFile, &pSeg->data[i], &chkSumActual);
                  if (r != OK)
                  {
                     return r;
                  }
            }

            /* Add the new segment into our data structures. */
            r = AddSegment(pSeg);
            if (r != OK)
            {
                  return r;
            }

            break;
         }

         case REC_EOF:
         {
            endRecordFound = 1;
            break;
         }

         case REC_EXT_SEG_ADDR:
         {
            /* Make sure the extAddr is 0. Any given HEX file may only use segment addressing
            or extended linear addressing, but not both. */
            if (extAddr != 0)
            {
                  printf("Both segment addressing and linear addressing used. Only one type or the other is supported.\n");
                  return MIXED_ADDRESSING_MODES;
            }

            /* Read the 16-bit segment address. */
            r = LoadU16(inFile, &segAddr, &chkSumActual);
            if (r != OK)
            {
                  return r;
            }
            break;
         }

         case REC_START_SEG_ADDR:
         {
            U16 startSeg, startOfs;

            /* Read the 16-bit segment of the starting address. */
            r = LoadU16(inFile, &startSeg, &chkSumActual);
            if (r != OK)
            {
                  return r;
            }

            /* Read the 16-bit offset of the starting address with the segment. */
            r = LoadU16(inFile, &startOfs, &chkSumActual);
            if (r != OK)
            {
                  return r;
            }

            /* Compute the 32-bit starting address using the segment and offset. */
            startAddr = (startSeg << 4) + startOfs;
            break;
         }

         case REC_EXT_LIN_ADDR:
         {
            /* Make sure the segAddr is 0. Any given HEX file may only use segment addressing
            or extended linear addressing, but not both. */
            if (segAddr != 0)
            {
                  printf("Both segment addressing and linear addressing used. Only one type or the other is supported.\n");
                  return MIXED_ADDRESSING_MODES;
            }

            /* Read the upper 16-bits of the address. */
            r = LoadU16(inFile, &extAddr, &chkSumActual);
            if (r != OK)
            {
                  return r;
            }
            break;
         }

         case REC_START_LIN_ADDR:
         {
            /* Read the 32-bit starting address. */
            r = LoadU32(inFile, &startAddr, &chkSumActual);
            if (r != OK)
            {
                  return r;
            }
            break;
         }

         default:
         {
            printf("Invalid record type: %i.\n", recType);
            return INVALID_RECORD_TYPE;
         }
      }

      /* Read the checksum. */
      r = LoadU8(inFile, &chkSumFile, NULL);
      if (r != OK)
      {
         return r;
      }

      /* Validate the checksum. */
      if (((~chkSumActual + 1) & 0xFF) != chkSumFile)
      {
         printf("Checksum error.\n");
         return CHECKSUM_ERROR;
      }
   }

   /* Make sure and end record was processed. */
   if (endRecordFound == 0)
   {
      printf("No end record was found.\n");
      return END_RECORD_ERROR;
   }

   return OK;
}

/**************************************************************************//**
* Write the loaded input data as a WDC binary format file.
*
* @param[in] outFile The file object to write to.
*
* @return An RESULT indicating success or failure.
******************************************************************************/
static RESULT WriteWdcFile(FILE *outFile)
{
   printf("WDC file output is currently not supported.\n");
   return UNSUPPORTED;
}

/**************************************************************************//**
* Write the loaded input data as a PAP format file.
*
* @param[in] outFile The file object to write to.
*
* @return An RESULT indicating success or failure.
******************************************************************************/
static RESULT WritePapFile(FILE *outFile)
{
   SEGMENT* pSeg = NULL;
   U32 papRecords = 0;
   RANGE* pRange;
   U8* pData;

   /* Write each range. */
   pRange = pAllRanges;
   while (pRange)
   {
      /* Set up the starting segment for this range. */
      pSeg = pRange->pSegStart;
      pData = &pSeg->data[0];

      /* Write each byte in the current range. */
      while (pRange->len)
      {
         U32 papRecLen;
         U16 chkSum;

         /* Determine the length of the PAP record to write. */
         papRecLen = pRange->len < PAP_REC_LEN ? pRange->len : PAP_REC_LEN;

         /* Write the record start char, the record length, and the address. */
         if (fprintf(outFile, ";%02X%02X%02X", papRecLen, (pRange->addr >> 8) & 0xFF,
               pRange->addr & 0xFF) < 0)
         {
               printf("Error writing output file.\n");
               return IO_ERROR;
         }

         /* Initialize the checkSum. All hex-formatted data is included. */
         chkSum = papRecLen + (pRange->addr & 0xFF) + ((pRange->addr >> 8) & 0xFF);

         /* Move to the next PAP record. */
         pRange->len -= papRecLen;
         pRange->addr += papRecLen;

         /* Write the data for this PAP record. */
         while (papRecLen--)
         {
            /* Update the checkSum. */
            chkSum += *pData;

            /* Write the current byte and move to the next one. */
            if (fprintf(outFile, "%02X", *(pData++)) < 0)
            {
               printf("Error writing output file.\n");
               return IO_ERROR;
            }

            /* See if we've reached the end of the current segment. */
            if ((--pSeg->len) == 0)
            {
               /* Move to the next segment. */
               pSeg = pSeg->pNext;
               pData = &pSeg->data[0];
            }
         }

         /* Write the checksum and the record footer. */
         if (fprintf(outFile, "%02X%02X\r\n\0\0\0\0\0\0", (chkSum >> 8) & 0xFF,
               chkSum & 0xFF) < 0)
         {
            printf("Error writing output file.\n");
            return IO_ERROR;
         }

         /* We have completed a PAP record. */
         papRecords++;
      }

      /* Move to the next range. */
      pRange = pRange->pNext;
   }

   /* Write the end record. */
   if (fprintf(outFile, ";00%02X%02X%02X%02X\r\n\0\0\0\0\0\0",
      (papRecords >> 8) & 0xFF, papRecords & 0xFF,
      (papRecords >> 8) & 0xFF, papRecords & 0xFF) < 0)
   {
      printf("Error writing output file.\n");
      return IO_ERROR;
   }

   printf("File written as PAP file.\n");
   return OK;
}

/**************************************************************************//**
* Parses a numeric options as a U32, supporting 0x and $.
*
* @param[in] strDesc A description of the option being parsed.
* @param[in] str The numeric string to parse.
* @param[out] pU32 The parsed value is stored here.
*
* @return An RESULT indicating success or failure.
******************************************************************************/
RESULT ParseOptU32(const char *strDesc, const char *str, U32 *pU32)
{
   char *pNext;
   int radix = 0;

   if (str[0] == '$')
   {
      str++;
      radix = 16;
   }

   *pU32 = strtol(str, &pNext, radix);
   if (errno != 0)
   {
      printf("Invalid %s: \"%s\"\n", strDesc, str);
      return INVALID_ARGUMENTS;
   }

   if (pNext == str)
   {
      printf("Invalid or unspecified %s: \"%s\"\n", strDesc, str);
      return INVALID_ARGUMENTS;
   }

   return OK;
}

/**************************************************************************//**
* Parses options for binary files.
*
* @param[in,out] pInFile The input file being processed.
*
* Use strtok() to gain access to each option.
*
* @return An RESULT indicating success or failure.
******************************************************************************/
RESULT ParseBinOpts(DATA_FILE *pInFile)
{
   FILE_OPTS_BIN *pOpts;
   char *opt;
   RESULT r;

   pOpts = (FILE_OPTS_BIN *) malloc(sizeof(FILE_OPTS_BIN));
   if (pOpts == NULL)
   {
      return NO_MEMORY;
   }
   memset(pOpts, 0, sizeof(*pOpts));
   pInFile->pOpts = pOpts;

   while ((opt = strtok(NULL, ",")) != NULL)
   {
      if (!(strncmp(opt, "A=", 2)))
      {
         r = ParseOptU32("start address", &opt[2], &pOpts->startAddr);
         if (r != OK)
         {
            return r;
         }

         pOpts->addrSpecified = 1;
      }
      else
      {
         printf("Invalid binary file option: \"%s\"\n", opt);
         return INVALID_ARGUMENTS;
      }
   }

   if (!pOpts->addrSpecified)
   {
      printf("ERROR: Missing start address (A=<ADDR>).\n");
      return INVALID_ARGUMENTS;
   }

   return OK;
}

/**************************************************************************//**
* Parses options for Intel hex files.
*
* @param[in,out] pInFile The input file being processed.
*
* Use strtok() to gain access to each option.
*
* @return An RESULT indicating success or failure.
******************************************************************************/
RESULT ParseHexOpts(DATA_FILE *pInFile)
{
   char *opt;

   while ((opt = strtok(NULL, ",")) != NULL)
   {
      printf("Invalid HEX file option: \"%s\"\n", opt);
      return INVALID_ARGUMENTS;
   }

   return OK;
}

/**************************************************************************//**
* Parses options for MOS PAP files.
*
* @param[in,out] pInFile The input file being processed.
*
* Use strtok() to gain access to each option.
*
* @return An RESULT indicating success or failure.
******************************************************************************/
RESULT ParsePapOpts(DATA_FILE *pInFile)
{
   char *opt;

   while ((opt = strtok(NULL, ",")) != NULL)
   {
      printf("Invalid PAP file option: \"%s\"\n", opt);
      return INVALID_ARGUMENTS;
   }

   return OK;
}

/**************************************************************************//**
* Parses options for WDC binary files.
*
* @param[in,out] pInFile The input file being processed.
*
* Use strtok() to gain access to each option.
*
* @return An RESULT indicating success or failure.
******************************************************************************/
RESULT ParseWdcOpts(DATA_FILE *pInFile)
{
   char *opt;

   while ((opt = strtok(NULL, ",")) != NULL)
   {
      printf("Invalid WDC file option: \"%s\"\n", opt);
      return INVALID_ARGUMENTS;
   }

   return OK;
}

/**************************************************************************//**
* Parses the command line parameters.
*
* @param[in] argc The count of the arguments, including the exe name.
* @param[in] argv The arguements.
*
* @return An RESULT indicating success or failure.
******************************************************************************/
RESULT ParseParams(int argc, char* argv[])
{
   DATA_FILE *pLastInFile = NULL;
   char *arg;
   RESULT r;

   while (arg = *(++argv))
   {
      if (!strncmp(arg, "-if", 3))
      {
         char *fileStr = *(++argv);

         /* Allocate a new input file and clear it. */
         DATA_FILE *pInFile = (DATA_FILE *) malloc(sizeof(DATA_FILE));
         if (pInFile == NULL)
         {
            return NO_MEMORY;
         }
         memset(pInFile, 0, sizeof(*pInFile));

         // Add the new input file to the end of the list.
         if (pLastInFile == NULL)
         {
            pInFiles    = pInFile;
         }
         else
         {
            pLastInFile->pNext = pInFile;
         }
         pLastInFile = pInFile;

         /* Ensure the user specified the file name (plus any options). */
         if (fileStr == NULL)
         {
            printf("ERROR: Missing input file name.");
            return INVALID_ARGUMENTS;
         }

         /* Extract the file name (strip off any options). */
         pInFile->pName = strtok(fileStr, ",");

         /* Determine the file type. */
         switch (arg[3])
         {
            case 'h':
               pInFile->type = FILE_TYPE_HEX;

               r = ParseHexOpts(pInFile);
               if (r != OK)
               {
                  return r;
               }

               break;

            case 'b':
               pInFile->type = FILE_TYPE_BIN;

               r = ParseBinOpts(pInFile);
               if (r != OK)
               {
                  return r;
               }

               break;

            default:
               printf("ERROR: Invalid input file type: '%c'\n", arg[3]);
               return INVALID_ARGUMENTS;
               break;
         }
      }
      else if (!strncmp(arg, "-of", 3))
      {
         char *fileStr = *(++argv);

         /* Only one output file is supported. */
         if (pOutFile != NULL)
         {
            printf("ERROR: Only one output file is supported.");
            return INVALID_ARGUMENTS;
         }

         /* Allocate a new output file and clear it. */
         pOutFile = (DATA_FILE *) malloc(sizeof(DATA_FILE));
         if (pOutFile == NULL)
         {
            return NO_MEMORY;
         }
         memset(pOutFile, 0, sizeof(*pOutFile));

         /* Ensure the user specified the file name (plus any options). */
         if (fileStr == NULL)
         {
            printf("ERROR: Missing output file name.");
            return INVALID_ARGUMENTS;
         }

         /* Extract the file name (strip off any options). */
         pOutFile->pName = strtok(fileStr, ",");

         /* Determine the file type. */
         switch (arg[3])
         {
            case 'p':
               pOutFile->type = FILE_TYPE_PAP;
               r = ParsePapOpts(pOutFile);
               if (r != OK)
               {
                  return r;
               }

               break;

            case 'w':
               pOutFile->type = FILE_TYPE_WDC;
               r = ParseWdcOpts(pOutFile);
               if (r != OK)
               {
                  return r;
               }

               break;

            default:
               printf("ERROR: Invalid output file type: '%c'\n", arg[3]);
               return INVALID_ARGUMENTS;
               break;
         }
      }
      else
      {
         printf("ERROR: Unsupported option \"%s\"\n", arg);
         return INVALID_ARGUMENTS;
      }
   }

   if (pInFiles == NULL)
   {
      printf("ERROR: At least one input file must be specified.\n");
      return INVALID_ARGUMENTS;
   }

   if (pOutFile == NULL)
   {
      printf("ERROR: An output file must be specified.\n");
      return INVALID_ARGUMENTS;
   }

   return OK;
}

/******************************************************************************
 Public Function Definitions
******************************************************************************

/**************************************************************************//**
* The main() function.
*
* @param[in] argc The count of the arguments, including the exe name.
* @param[in] argv The arguements.
*
* @return 0 on success, non-zero on error.
******************************************************************************/
int main(int argc, char* argv[])
{
   RESULT r;
   RANGE* pRange;

   printf("Retro file conversion utility, Timothy Alicie, 2017-2022, v" VER_STR ".\n\n");

   if (argc == 1)
   {
      PrintUsage();
      return USAGE_SHOWN;
   }

   r = ParseParams(argc, argv);
   if (r != OK)
   {
      return r;
   }

   /* Load each input file. */
   do
   {
      printf("Loading \"%s\" as ", pInFiles->pName);

      FILE* inFile = fopen(pInFiles->pName, "rb");
      if (!inFile)
      {
          printf("Unable to open the input file \"%s\".\n", pInFiles->pName);
          return CANNOT_OPEN_FILE;
      }

      switch (pInFiles->type)
      {
         case FILE_TYPE_HEX:
            r = LoadHexFile(inFile);
            if (r != OK)
            {
               return r;
            }

            break;

         case FILE_TYPE_BIN:
            r = LoadBinFile(inFile, (FILE_OPTS_BIN *) pInFiles->pOpts);
            if (r != OK)
            {
               return r;
            }

            break;
      }

      fclose(inFile);

      /* Concatenate any ranges that have become contiguous. */
      CombineRanges();

   } while (pInFiles = pInFiles->pNext);

   printf("\nRanges:\n");
   pRange = pAllRanges;
   while (pRange)
   {
      printf("0x%04X - 0x%04X: %u bytes.\n",
         pRange->addr, pRange->addr + pRange->len - 1, pRange->len);
      pRange = pRange->pNext;
   }

   printf("\nWriting \"%s\"...\n", pOutFile->pName);

   FILE* outFile = fopen(pOutFile->pName, "w+b");
   if (!outFile)
   {
      printf("Unable to open the output file \"%s\".\n", pOutFile->pName);
      return CANNOT_OPEN_FILE;
   }

   /* Write the output file. */
   switch (pOutFile->type)
   {
      case FILE_TYPE_PAP:
         r = WritePapFile(outFile);
         if (r != OK)
         {
            return r;
         }

         break;

      case FILE_TYPE_WDC:
         r = WriteWdcFile(outFile);
         if (r != OK)
         {
            return r;
         }

         break;
   }

   fclose(outFile);

   return OK;
}

//
// newtonpkg: Newton package format utility
// by Steven Frank <stevenf@panic.com>, 2015
//
// Based on information from:
// http://www.unna.org/unna/apple/documentation/developer/NewtonFormats1.1.pdf
//

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

// Package flags

#define kAutoRemoveFlag				0x80000000
#define kCopyProtectFlag			0x40000000
#define kNoCompressionFlag			0x10000000
#define kRelocationFlag				0x04000000
#define kUseFasterCompressionFlag	0x02000000

// Part flags

#define kProtocolPart				0x00000000
#define kNOSPart					0x00000001
#define kRawPart					0x00000002
#define kAutoLoadFlag				0x00000010
#define kPartAutoRemoveFlag			0x00000020
#define kNotifyFlag					0x00000080
#define kAutoCopyFlag				0x00000100

// Object formats

#define OBJECT_FORMAT_MASK			0x000000ff

#define OBJECT_FORMAT_BINARY		0x00000040
#define OBJECT_FORMAT_ARRAY			0x00000041
#define OBJECT_FORMAT_FRAME			0x00000043

// Classes

#define NIL_CLASS					0x00000002
#define SYMBOL_CLASS				0x00055552

typedef uint8_t Byte;
typedef uint32_t ULong;
typedef uint32_t Date;
typedef uint16_t UShort;

typedef struct InfoRefStruct
{
	UShort offset;
	UShort length;
} InfoRef;

struct PackageDirectory
{
	Byte signature[8];
	ULong reserved1;
	ULong flags;
	ULong version;
	InfoRef copyright;
	InfoRef name;
	ULong size;
	Date creationDate; /* Seconds since midnight Jan 4, 1904 */
	ULong reserved2;
	ULong reserved3;
	ULong directorySize;
	ULong numParts;
	/* PartEntry parts[numParts]; */
	/* Byte variableLengthData[]; */
};

struct PartEntry
{
	ULong offset;
	ULong size;
	ULong size2;
	ULong type;
	ULong reserved1;
	ULong flags;
	InfoRef info;
	ULong reserved2;
};

struct RelocationHeader
{
	ULong reserved;
	ULong relocationSize;
	ULong pageSize;
	ULong numEntries;
	ULong baseAddress;
};

struct RelocationSet
{
	UShort pageNumber;
	UShort offsetCount;
	/* Byte offsets[]; */
};


void print4CharCode(uint32_t code)
{
	uint8_t b1 = (code & 0xff000000) >> 24;
	uint8_t b2 = (code & 0x00ff0000) >> 16;
	uint8_t b3 = (code & 0x0000ff00) >> 8;
	uint8_t b4 = (code & 0x000000ff);
	
	printf("%c%c%c%c", b1, b2, b3, b4);
}


void printRef(uint32_t ref)
{
	if ( (ref & 0x00000003) == 0x0 )
	{
		int32_t theInt = ref >> 2;
		printf("  Integer: 0x%08X (%d)", theInt, theInt);
	}
	else if ( (ref & 0x00000003) == 0x1 )
	{
		printf("  Pointer: 0x%08X", ref >> 2);
	}
	else if ( (ref & 0xfff0000f) == 0xa )
	{
		uint16_t wc = (ref >> 4);
		printf("  Character: 0x%04x", wc);
	}
	else if ( (ref & 0x00000003) == 0x2 )
	{
		printf("  Special: 0x%08X", ref >> 2);
	}
	else if ( (ref & 0x00000003) == 0x3 )
	{
		uint32_t table = (ref & 0xffff0000) >> 16;
		uint32_t index = (ref & 0x0000ffff) >> 2;
		
		printf("  MagicPtr: table %u, index %u", table, index);
	}
	
	printf("\n");
}


void printFlag(ULong flags, ULong bitmask, const char *flagName)
{
	if ( (flags & bitmask) != 0 )
	{
		printf(" %s", flagName);
	}
}


void printWideString(uint8_t *s, unsigned int length)
{
	if ( length == 0 )
		return;
		
	for ( int offset = 0; offset < length - 2; offset += 2 )
	{
		wchar_t wc = (s[offset] << 8) | (s[offset + 1]);
	
		printf("%lc", wc);
	}
	
	printf("\n");
}


void printClass(uint32_t class)
{
	printf("Class: 0x%08X ", class);

	if ( class == NIL_CLASS )
	{
		printf("(NIL)");
	}
	else if ( class == SYMBOL_CLASS )
	{
		printf("(Symbol)");
	}
	
	printf("\n");
}


uint8_t *printObject(uint8_t *p)
{
	uint32_t word1 = ntohl(*(uint32_t *)p);
	uint32_t word2 = ntohl(*((uint32_t *)(p + 4)));
	uint32_t word3 = ntohl(*((uint32_t *)(p + 8)));
	
//	printf("Object header (0x%08X 0x%08X)\n", word1, word2);
	
	if ( (word1 & OBJECT_FORMAT_MASK) == OBJECT_FORMAT_ARRAY )
	{
		uint32_t arraySize = (word1 & 0xffffff00) >> 8;
		int alignment = word2 & 0x1;
		
		printf("Type: Array (0x%X (%u) bytes, %u byte aligned)\n", arraySize, arraySize, alignment ? 4 : 8);
	
		printClass(word3);

		uint32_t word4 = ntohl(*((uint32_t *)(p + 12)));

		printRef(word4);
	
		p += arraySize;
	}
	else if ( (word1 & OBJECT_FORMAT_MASK) == OBJECT_FORMAT_BINARY )
	{
		uint32_t binarySize = (word1 & 0xffffff00) >> 8;

		printf("Type: Binary object\n");
		printf("Size: 0x%X bytes (%u)\n", binarySize, binarySize);

		printClass(word3);

		if ( word3 == SYMBOL_CLASS )
		{
			// Decode and print the symbol
			
			printf("Symbol: '");
			
			unsigned char *symbol = p + 16;
			for ( int i = 0; i < binarySize - 17; ++i )
			{
				printf("%c", symbol[i]);
			}
			
			printf("'\n");
		}

		// Padding
		
		while ( (binarySize % 4) != 0 )
		{
			++binarySize;
		}

		p += binarySize;
	}
	else if ( (word1 & OBJECT_FORMAT_MASK) == OBJECT_FORMAT_FRAME )
	{
		uint32_t frameSize = (word1 & 0xffffff00) >> 8;
		
		printf("Type: Frame\n");
		printf("Size: 0x%X bytes (%u)\n", frameSize, frameSize);
		
		frameSize -= 8;
		p += 8;
		
		while ( frameSize > 0 )
		{
			uint32_t word = ntohl(*((uint32_t *)(p)));
			printRef(word);
			p += 4;
			frameSize -= 4;
		}
	}
	else
	{
	}

	printf("\n");
	return p;
}


int main(int argc, const char *argv[])
{
	if ( argc < 2 )
	{
		printf("Usage: newtonpkg <package>\n");
		return 1;
	}
	
	const char *filename = argv[1];
	FILE *fp = fopen(filename, "rb");
	
	if ( fp == NULL )
	{
		printf("Can't open %s\n", filename);
		return 1;
	}

	fseek(fp, 0, SEEK_END);
	fpos_t size = 0;
	fgetpos(fp, &size);
	
	printf("%s (%lld bytes)\n\n", filename, size);
	
	fseek(fp, 0, SEEK_SET);
	
	unsigned char *buffer = malloc(size);
	
	if ( buffer == NULL )
	{
		printf("Out of memory.\n");
		return 1;
	}
	
	if ( fread(buffer, size, 1, fp) != 1 )
	{
		printf("I/O error.\n");
		return 1;
	}
	
	struct PackageDirectory *pkgdir = (struct PackageDirectory *)buffer;
	
	// Endian swap
	
	pkgdir->flags = ntohl(pkgdir->flags);
	pkgdir->version = ntohl(pkgdir->version);
	pkgdir->size = ntohl(pkgdir->size);
	pkgdir->creationDate = ntohl(pkgdir->creationDate);
	pkgdir->directorySize = ntohl(pkgdir->directorySize);
	pkgdir->numParts = ntohl(pkgdir->numParts);

	pkgdir->name.offset = ntohs(pkgdir->name.offset);
	pkgdir->name.length = ntohs(pkgdir->name.length);

	pkgdir->copyright.offset = ntohs(pkgdir->copyright.offset);
	pkgdir->copyright.length = ntohs(pkgdir->copyright.length);

	// Find variable length data section
	
	uint8_t *varData = (uint8_t *)&buffer[sizeof(struct PackageDirectory) + (sizeof(struct PartEntry) * pkgdir->numParts)];

	// Signature
	
	printf("    Signature: '");
	
	for ( int i = 0; i < 8; ++i )
	{
		printf("%c", pkgdir->signature[i]);
	}
	
	printf("'");
	
	if ( pkgdir->signature[7] == '0' )
	{
		printf(" (no relocation info, all Newton OS)");
	}
	else if ( pkgdir->signature[7] == '1' )
	{
		printf(" (may contain relocation info, Newton OS 2.0+)");
	}
	else
	{
		printf(" (unknown format)");
	}
	
	printf("\n");
	
	// Flags
	
	printf("        Flags: 0x%08x", pkgdir->flags);
	
	printFlag(pkgdir->flags, kAutoRemoveFlag, "kAutoRemoveFlag");
	printFlag(pkgdir->flags, kCopyProtectFlag, "kCopyProtectFlag");
	printFlag(pkgdir->flags, kNoCompressionFlag, "kNoCompressionFlag");
	printFlag(pkgdir->flags, kRelocationFlag, "kRelocationFlag");
	printFlag(pkgdir->flags, kUseFasterCompressionFlag, "kUseFasterCompressionFlag");

	printf("\n");
	
	if ( (pkgdir->flags & kRelocationFlag) != 0 )
	{
		printf("newtonpkg can't parse packages with relocation data yet.\n");
		return 0;
	}
	
	// Version

	printf("      Version: 0x%08x (%u)\n", pkgdir->version, pkgdir->version);
	
	// Copyright

	printf("    Copyright: ");
	
	uint8_t *wstr = &varData[pkgdir->copyright.offset];
	
	printWideString(wstr, pkgdir->copyright.length);
	
	// Name
	
	printf("         Name: ");
	
	wstr = &varData[pkgdir->name.offset];
	
	printWideString(wstr, pkgdir->name.length);
	
	// Size
	
	printf("         Size: 0x%08x (%u)\n", pkgdir->size, pkgdir->size);

	// Creation date
	
	printf(" creationDate: 0x%08x (%u) (Jan 1, 1904 + %u days)\n", pkgdir->creationDate, pkgdir->creationDate, pkgdir->creationDate / 60 / 60 / 24);
 
	// Directory size
	
	printf("directorySize: 0x%08x (%u)\n", pkgdir->directorySize, pkgdir->directorySize);

	// Number of parts
	
	printf("     numParts: 0x%08x (%u)\n", pkgdir->numParts, pkgdir->numParts);
	
	// Parts
	
	uint8_t *pkgData = (uint8_t *)&buffer[pkgdir->directorySize];
	
	struct PartEntry *partEntries = (struct PartEntry *)&buffer[sizeof(struct PackageDirectory)];
	
	for ( int partNum = 0; partNum < pkgdir->numParts; ++partNum )
	{
		printf("\nPart %d:\n", partNum);
		
		struct PartEntry *partEntry = &partEntries[partNum];
		
		partEntry->offset = ntohl(partEntry->offset);
		partEntry->size = ntohl(partEntry->size);
		partEntry->flags = ntohl(partEntry->flags);
		partEntry->type = ntohl(partEntry->type);
		
		printf("       Offset: 0x%08x (%u)\n", partEntry->offset, partEntry->offset);
		printf("         Size: 0x%08x (%u)\n", partEntry->size, partEntry->size);
		printf("        Flags: 0x%08x", partEntry->flags);

		printFlag(partEntry->flags, kProtocolPart, "kProtocolPart");
		printFlag(partEntry->flags, kNOSPart, "kNOSPart");
		printFlag(partEntry->flags, kRawPart, "kRawPart");
		printFlag(partEntry->flags, kAutoLoadFlag, "kAutoLoadFlag");
		printFlag(partEntry->flags, kPartAutoRemoveFlag, "kAutoRemoveFlag");
		printFlag(partEntry->flags, kNotifyFlag, "kNotifyFlag");
		printFlag(partEntry->flags, kAutoCopyFlag, "kAutoCopyFlag");
		printf("\n");
		
		printf("         Type: '");
		print4CharCode(partEntry->type);
		printf("'\n");

		printf("\n");
		
		uint8_t *p = &pkgData[partEntry->offset];
		int offset = pkgdir->directorySize;
		
		while ( p < (uint8_t *)&pkgData[partEntry->offset + partEntry->size] )
		{
			uint8_t *oldp = p;
			printf("[file offset %08X]\n", offset);
			p = printObject(p);
			offset += p - oldp;
		}
	}
	
	// Done
	
	fclose(fp);

	return 0;
}
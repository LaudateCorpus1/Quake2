#include "zip.h"

/*
 *  When ZIP_USE_CENTRAL_DIRECTORY is defined, ZipEnum, ZipOpenFile and
 *  ZipExtractFileMem will use central zip directory to get info (faster zip file scan)
 */
#define ZIP_USE_CENTRAL_DIRECTORY

#define READ_BUF_SIZE 65536		// size of buffer for reading compressed contents

typedef unsigned char   uch;	// 1-byte unsigned
typedef unsigned short  ush;	// 2-byte unsigned
typedef unsigned long   ulg;	// 4-byte unsigned


#ifdef _WIN32
#pragma pack(push,1)
#endif

typedef struct
{
	uch		versionNeededToExtract[2];
	ush		generalPurposeBitFlag;
	ush		compressionMethod;
	ulg		lastModDosDatetime;
	ulg		crc32;
	ulg		csize;
	ulg		ucsize;
	ush		filenameLength;
	ush		extraFieldLength;
} localFileHdr_t;

typedef struct
{
	uch		versionMadeBy[2];
	uch		versionNeededToExtract[2];
	ush		generalPurposeBitFlag;
	ush		compressionMethod;
	ulg		lastModDosDatetime;
	ulg		crc32;
	ulg		csize;
	ulg		ucsize;
	ush		filenameLength;
	ush		extraFieldLength;
	ush		fileCommentLength;
	ush		diskNumberStart;
	ush		internalFileAttributes;
	ulg		externalFileAttributes;
	ulg		relativeOffsetLocalHeader;
} cdirFileHdr_t;

typedef struct
{
	ush		numberThisDisk;
	ush		numDiskStartCdir;
	ush		numEntriesCentrlDirThsDisk;
	ush		totalEntriesCentralDir;
	ulg		sizeCentralDirectory;
	ulg		offsetStartCentralDirectory;
	ush		zipfileCommentLength;
} ecdirRec_t;

#ifdef _WIN32
#pragma pack(pop)
#endif


FILE* Zip_OpenArchive (char *name)
{
	FILE	*F;
	localFileHdr_t hdr;
	unsigned signature;

	F = fopen (name, "rb");
	if (!F) return NULL; // file not found

	if ((fread (&signature, 4, 1, F) != 1) ||
		(signature != 0x04034B50) ||
		(fread (&hdr, sizeof(hdr), 1, F) != 1) ||
		(hdr.compressionMethod >= 9))
	{
		fclose (F);
		return NULL;
	}
	return F;
}


void Zip_CloseArchive (FILE *F)
{
	fclose (F);
}


#ifndef ZIP_USE_CENTRAL_DIRECTORY

// Enumerate zip files
bool Zip_EnumArchive (FILE *F, enumZipFunc_t enum_func)
{
	localFileHdr_t hdr;
	char	buf[512];
	zipFile_t file;

	if (!enum_func) return false;

	fseek(F, 0, SEEK_SET);									// rewind archive
	while (!feof(F))
	{
		unsigned signature;

		if (fread (&signature, 4, 1, F) != 1) return false;	// cannot read signature

		if (signature == 0x02014B50 || signature == 0x06054B50)
			return true;									// central directory structure - not interesting

		if (signature != 0x04034B50) return false;			// MUST be local file header
		if (fread (&hdr, sizeof(hdr), 1, F) != 1) return false;
		unsigned name_len = hdr.filenameLength;
		if (fread (&buf[0], name_len, 1, F) != 1) return false;

		buf[name_len] = 0;
		// fill zip_file structure
		file.name = buf;
		file.csize = hdr.csize;
		file.ucsize = hdr.ucsize;
		file.crc32 = hdr.crc32;
		file.method = hdr.compression_method;
		file.pos = ftell (F) + hdr.extraFieldLength;
		// seek to next file
		if (fseek (F, hdr.extraFieldLength + hdr.csize, SEEK_CUR)) return false;
		// if OK - make a callback
		if (!enum_func (&file)) return true;				// break on "false"
		//printf("%10d < %10d  [%1d]  %s\n", hdr.csize, hdr.ucsize, hdr.compression_method, buf);
		// dirs have size=0, method=0, last_name_char='/'
	}
	return true;
}


#else // ZIP_USE_CENTRAL_DIRECTORY


// Enumerate zip central directory (faster)
bool Zip_EnumArchive (FILE *F, enumZipFunc_t enum_func)
{
	unsigned pos;
	cdirFileHdr_t hdr;										// central directory record
	char	buf[512], *buf_ptr;
	zipFile_t file;

	if (!enum_func) return false;

	// Find end of central directory record (by signature)
	if (fseek (F, 0, SEEK_END)) return false;				// rewind to the archive end
	if ((pos = ftell (F)) < 22) return false;				// file too small to be a zip-archive
	// here: 18 = sizeof(ecdir_rec_ehdr) - sizeof(signature)
	if (pos < sizeof(buf) - 18)
		pos = 0;											// small file
	else
		pos -= sizeof(buf) - 18;

	// try to fill buf 128 times (64K total - max size of central arc comment)
	bool found = false;
	for (int i = 0; i < 128; i++)
	{
		if (fseek (F, pos, SEEK_SET)) return false;
		unsigned rs = fread (buf, 1, sizeof(buf), F);		// rs = read size
		for (buf_ptr = buf + rs - 4; buf_ptr >= buf; buf_ptr--)
			if (*(unsigned*)buf_ptr == 0x06054B50)
			{
				pos += buf_ptr - buf + 16;					// 16 - offset to a central dir ptr in cdir_end struc
				found = true;
				break;
			}
		if (found) break;
		if (!rs || !pos) return false;						// central directory not found
		pos -= sizeof(buf) - 4;								// make a 4 bytes overlap between readings
		if (pos < 0) pos = 0;
	}
	if (!found) return false;

	// get pointer to a central directory
	fseek(F, pos, SEEK_SET);
	if (fread (&pos, 4, 1, F) != 1) return false;
	// seek to a central directory
	if (fseek (F, pos, SEEK_SET)) return false;

	while (!feof(F))
	{
		unsigned signature;

		if (fread (&signature, 4, 1, F) != 1) return false;	// cannot read signature
		if (signature == 0x06054B50)
			return true;									// end of central directory structure - finish
		if (signature != 0x02014B50) return false;			// MUST be a central directory file header
		if (fread (&hdr, sizeof(hdr), 1, F) != 1) return false;
		unsigned name_len = hdr.filenameLength;
		if (fread (&buf[0], name_len, 1, F) != 1) return false;
		buf[name_len] = 0;
		// fille zip_file structure
		file.name = buf;
		file.csize = hdr.csize;
		file.ucsize = hdr.ucsize;
		file.crc32 = hdr.crc32;
		file.method = hdr.compressionMethod;
		file.pos = hdr.relativeOffsetLocalHeader;
		// seek to next file
		if (fseek (F, hdr.extraFieldLength + hdr.fileCommentLength, SEEK_CUR)) return false;
		// if OK - make a callback
		if (!enum_func (&file)) return false;				// break on "false"
		//printf("%10d < %10d  [%1d]  %s\n", hdr.csize, hdr.ucsize, hdr.compression_method, buf);
		// dirs have size=0, method=0, last_name_char='/'
	}
	return true;
}


#endif // ZIP_USE_CENTRAL_DIRECTORY


bool Zip_ExtractFileMem (FILE *F, zipFile_t *file, void *buf)
{
#ifdef ZIP_USE_CENTRAL_DIRECTORY
	localFileHdr_t hdr;
#endif

	if (file->method != 0 && file->method != Z_DEFLATED)
		return false;										// unknown method
	if (fseek (F, file->pos, SEEK_SET))
		return false;

#ifdef ZIP_USE_CENTRAL_DIRECTORY
	unsigned signature;
	// file->pos points to a local file header, we must skip it
	if (fread (&signature, 4, 1, F) != 1) return false;
	if (signature != 0x04034B50) return false;
	if (fread (&hdr, sizeof(hdr), 1, F) != 1) return false;
	if (fseek (F, hdr.filenameLength + hdr.extraFieldLength, SEEK_CUR)) return false;
#endif

	// init z_stream
	if (file->method == Z_DEFLATED)
	{
		z_stream s;

		// inflate file
		s.zalloc = NULL;
		s.zfree = NULL;
		s.opaque = NULL;
		if (inflateInit2 (&s, -MAX_WBITS) != Z_OK)
			return false;									// not initialized
		s.avail_in = 0;
		s.next_out = (uch*)buf;
		s.avail_out = file->ucsize;
		unsigned rest_read = file->csize;
		while (s.avail_out > 0)
		{
			char	read_buf[READ_BUF_SIZE]; // buffer for reading packed contents; any size

			if (s.avail_in == 0 && rest_read)
			{
				// fill read buffer
				unsigned read_size = min(rest_read, sizeof(read_buf));
				if (fread (read_buf, read_size, 1, F) != 1)
				{
					inflateEnd (&s);
					return false;							// cannot read zip
				}
				// update stream
				s.avail_in += read_size;
				s.next_in = (uch*)read_buf;
				rest_read -= read_size;
			}
			// decompress data; exit from inflate() by error or out of stream input buffer
			int ret = inflate (&s, Z_SYNC_FLUSH);
			if ((ret == Z_STREAM_END && (s.avail_in + rest_read)) || // end of stream, but unprocessed data
				(ret != Z_OK && ret != Z_STREAM_END))		// error code
			{
				inflateEnd (&s);
				return false;
			}
		}
		inflateEnd (&s);
		if (rest_read) return false;						// bad compressed size
	}
	else
	{
		// unstore file
		if (fread (buf, file->ucsize, 1, F) != 1)
			return false;
	}
	// check CRC (if provided)
	if (file->crc32 && file->crc32 != crc32 (0, (uch*)buf, file->ucsize))
		return false;										// bad CRC or size

	return true;
}


ZFILE *Zip_OpenFile (FILE *F, zipFile_t *file)
{
	ZFILE	*z;
#ifdef ZIP_USE_CENTRAL_DIRECTORY
	ulg		signature;
	localFileHdr_t hdr;
#endif

	if (file->method != 0 && file->method != Z_DEFLATED)
		return NULL;										// unknown method
	z = (ZFILE*) appMalloc (sizeof(ZFILE));

#ifdef ZIP_USE_CENTRAL_DIRECTORY
	// file->pos points to a local file header, we must skip it
	if (fseek (F, file->pos, SEEK_SET)) return NULL;
	if (fread (&signature, 4, 1, F) != 1) return NULL;
	if (signature != 0x04034B50) return NULL;
	if (fread (&hdr, sizeof(hdr), 1, F) != 1) return NULL;
	z->zpos = file->pos + 4 + sizeof(hdr) + hdr.filenameLength + hdr.extraFieldLength;
#else
	z->zpos = file->pos;
#endif

	if (file->method == Z_DEFLATED)
	{   // inflate file
//		z->s.zalloc = NULL;
//		z->s.zfree = NULL;
//		z->s.opaque = NULL;
		if (inflateInit2 (&z->s, -MAX_WBITS) != Z_OK)
		{
			appFree (z);
			return NULL;									// not initialized
		}
//		z->s.avail_in = 0;
		z->buf = (char*) appMalloc (READ_BUF_SIZE);
	}
//	z->crc32 = 0;
	memcpy (&z->file, file, sizeof(zipFile_t));
	z->rest_read = file->csize;
	z->rest_write = file->ucsize;
	z->arc = F;

	return z;
}


int Zip_ReadFile (ZFILE *z, void *buf, int size)
{
	int		read_size, write_size, ret;

	if (fseek (z->arc, z->zpos, SEEK_SET)) return 0;		// cannot seek to data

	if (size > z->rest_write) size = z->rest_write;
	if (z->file.method == Z_DEFLATED)
	{
		z->s.avail_out = size;
		z->s.next_out = (uch*)buf;
		while (z->s.avail_out > 0)
		{
			if (z->s.avail_in == 0 && z->rest_read > 0)
			{
				// fill read buffer
				read_size = READ_BUF_SIZE;
				if (read_size > z->rest_read) read_size = z->rest_read;
				if (fread (z->buf, read_size, 1, z->arc) != 1) return 0; // cannot read zip
				z->zpos += read_size;
				// update stream
				z->s.avail_in += read_size;
				z->s.next_in = (uch*)z->buf;
				z->rest_read -= read_size;
			}
			// decompress data; exit from inflate() by error or
			// out of stream input buffer
			ret = inflate (&z->s, Z_SYNC_FLUSH);
			if ((ret == Z_STREAM_END && (z->s.avail_in + z->rest_read)) // end of stream, but unprocessed data
				|| (ret != Z_OK && ret != Z_STREAM_END))	// error code
			return 0;
		}
		write_size = size;
	}
	else
	{
		// stored
		write_size = z->rest_read;
		if (write_size > size) write_size = size;
		if (fread(buf, write_size, 1, z->arc) != 1) return 0; // cannot read
		z->rest_read -= write_size;
		z->zpos += write_size;
	}
	z->rest_write -= write_size;
	// update CRC32 (if needed)
	if (z->file.crc32) z->crc32 = crc32 (z->crc32, (uch*)buf, write_size);

	return write_size;
}


// Check size & CRC32 and close file
bool Zip_CloseFile (ZFILE *z)
{
	bool res = true;
	if (z->file.method == Z_DEFLATED)
	{
		inflateEnd (&z->s);
		appFree (z->buf);
	}
	if (z->rest_read || z->rest_write) res = false;			// bad sizes
	if (z->crc32 != z->file.crc32) res = false;				// bad CRC
	appFree (z);

	return res;
}


ZBUF *Zip_OpenBuf (void *data, int size)
{
	ZBUF *z = (ZBUF*) appMalloc (sizeof(ZBUF));
//	z->s.zalloc = NULL;
//	z->s.zfree = NULL;
//	z->s.opaque = NULL;
	if (inflateInit2 (&z->s, -MAX_WBITS) != Z_OK)
	{
		appFree (z);
		return NULL;										// not initialized
	}
	z->s.avail_in = size;
	z->s.next_in = (uch*)data;
//	z->readed = 0;

	return z;
}


int Zip_ReadBuf (ZBUF *z, void *buf, int size)
{
	z->s.avail_out = size;
	z->s.next_out = (uch*)buf;
	while (z->s.avail_out > 0)
	{
		// decompress data; exit from inflate() by error or
		// out of stream input buffer
		int ret = inflate (&z->s, Z_SYNC_FLUSH);
		if ((ret == Z_STREAM_END && z->s.avail_in) ||		// end of stream, but unprocessed data
			(ret != Z_OK && ret != Z_STREAM_END))			// error code
			return 0;
		//?? detect unexpected EOF ("readed" is less than "size")
		z->readed += size;
	}

	return size;
}


void Zip_CloseBuf (ZBUF *z)
{
	inflateEnd (&z->s);
	appFree (z);
}
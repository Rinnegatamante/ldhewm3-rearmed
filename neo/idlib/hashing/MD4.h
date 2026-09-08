#ifndef __MD4_H__
#define __MD4_H__

/*
===============================================================================

	Calculates a checksum for a block of data
	using the MD4 message-digest algorithm.

===============================================================================
*/

unsigned int MD4_BlockChecksum( const void *data, int length );
void MD4_BlockDigest( const void *data, int length, unsigned char digest[16] );

#endif /* !__MD4_H__ */

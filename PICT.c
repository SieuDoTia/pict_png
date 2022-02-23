// PICT.c

/* HEAD - ĐẦU
   (512 bytes) - bỏ qua

 
Image data after head
   (10 bytes)
   2 bytes  | ushort | FILE_SIZE   | File size
   2 bytes  | ushort | LEFT        | Left coordinate
   2 bytes  | ushort | TOP         | Top coordinate
   2 bytes  | ushort | RIGHT       | Right coordinate
   2 bytes  | ushort | BOTTOM      | Bottom coordinate
 
 Phiên Bản 2.0
   (4 bytes)
   2 bytes  | ushort | TYPE        | File type, for version 2.0 should equal 0x0011
   2 bytes  | ushort | VERSION     | Version, for version 2.0 should equal 0x02ff

   (26 btyes)
   2 bytes  | ushort | HEAD_OP     | Head opocde, for version 2.0 should equal 0x0c00
   2 bytes  | ushort | CODE        | for version 2.0 should equal 0xffef or 0xffee but some time 0xffff (if PixMap Handle == 0, use 0xffff)
   2 bytes  | ushort | ????        | Equal 0xfffff or 0x0000
   4 bytes  | uint   | RESOLUT_X   | Resolution horizontal (pixel/inch, divide 2.54 for cm)
   4 bytes  | uint   | RESOLUT_Y   | Resolution vertical (pixel/inch, divide 2.54 for cm)
   2 bytes  | ushort | FRAME_LEFT  | Frame left
   2 bytes  | ushort | FRAME_TOP   | Frame top
   2 bytes  | ushort | FRAME_RIGHT | Frame left
   2 bytes  | ushort | FRAME_BOTTOM| Frame bottom
   4 bytes  | uint   | RESERVE     | Reserve, no use

   Image data (at 512 + 40 bytes)
   2 bytes | ushort | OP_CODE
   n bytes |        | DATA
   2 bytes | ushort | OP_CODE
   n bytes |        | DATA
   ....

   End file
   2 bytes | ushort | PICTURE FINISH
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "PICT.h"
#include "PNG.h"

#define kFILE_TYPE__VERSION_2    0x0011
#define kVERSION_CODE__VERSION_2 0x02ff

#define kPICT  1
#define kPNG   2

// ---- op code list
#define OP_CODE__NO_OP   0x0000     // +0
#define OP_CODE__CLIP    0x0001     // +Region
#define OP_CODE__BK_PAT  0x0002   // +8

#define OP_CODE__FILL_PAT  0x000A   // +8
#define OP_CODE__ORIGIN    0x000C   // +4
#define OP_CODE__FG_COLOR  0x000E   // +4
#define OP_CODE__BK_COLOR  0x000F   // +4

#define OP_CODE__LINE      0x0020   // +8
#define OP_CODE__FRAME_RECT 0x0030   // +8
#define OP_CODE__PAINT_RECT 0x0031   // +8

#define OP_CODE__FRAME_RRECT 0x0040   // +8
#define OP_CODE__PAINT_RRECT 0x0041   // +8

#define OP_CODE__BITS_RECT      0x0090   // +???
#define OP_CODE__PACK_BITS_RECT 0x0098   // cho RGB

#define OP_CODE__DIRECT_BITS_RECT 0x009a   // cho RGB
#define OP_CODE__DIRECT_BITS_RGN  0x009b   // cho RGB

#define OP_CODE__END     0x00ff   // +0


typedef enum {
   PICT_COMPRESSION_RLE,      /* 8-bit run-length-encoded */
   PICT_COMPRESSION_JPEG,     /* jpeg compression - no support */
} pict_compression_enum;



typedef struct {
   char fileName[256];         /* file name max 255 char UTF8 */
   unsigned char compression; /* compression: no or RLE       */
   unsigned char dimension;   /* width * height * num_channel */
   unsigned short width;      /* image width                  */
   unsigned short height;     /* image height                 */
} pict_attributes;

/*
typedef struct {
   unsigned short size;
   unsigned short top;
   unsigned short left;
   unsigned short bottom;
   unsigned short right;
} region;
*/

/* chunk data */
typedef struct {
   unsigned int num_chunks;            /* number chunks */
   unsigned int *chunk_table;          /* chunk table list */
   unsigned int *chunk_size;         /* chunk size */
   unsigned int *chunk_table_position; /* list for chunk's position in table */
} chunk_data;



void luuAnhKongNen( char *tenTep, image_data *anh, unsigned char kieuDuLieu, unsigned short thoiGianKetXuat );

void luuAnhRLE( char *tenTep, image_data *anh, unsigned char kieuDuLieu, unsigned short thoiGianKetXuat );

#pragma mark ---- Read Attributes
/*pict_attributes readAttributes( FILE *pict_fp ) {
   
   pict_attributes attributes;

   // ---- skip file size
   fgetc(pict_fp);
   fgetc(pict_fp);

   // ---- image coordinates
   unsigned short left = fgetc(pict_fp) << 8 | fgetc(pict_fp);
   unsigned short top = fgetc(pict_fp) << 8 | fgetc(pict_fp);
   unsigned short right = fgetc(pict_fp) << 8 | fgetc(pict_fp);
   unsigned short bottom = fgetc(pict_fp) << 8 | fgetc(pict_fp);

   // ---- width
   attributes.width = right - left;
   
   // ---- height
   attributes.height = bottom - top;
   
   // ---- ASSUME RLE
   attributes.compression = PICT_COMPRESSION_RLE;

   return attributes;
} */

#pragma mark ---- Create Chunk Table
chunk_data create_chunk_table( unsigned int table_length ) {
   
   chunk_data chunk_data;
   chunk_data.num_chunks = table_length;
   
   // ---- get memory for chunk table
   chunk_data.chunk_size = malloc( table_length << 2 );   // 4 bytes
   chunk_data.chunk_table_position = malloc( table_length << 2 );  // 4 bytes
   
   if( chunk_data.chunk_size == NULL ) {
      printf( "Problem create memory for chunk table\n" );
      exit(0);
   }
   
   if( chunk_data.chunk_table_position == NULL ) {
      printf( "Problem create memory for chunk table\n" );
      exit(0);
   }
   
   return chunk_data;
}

#pragma mark ---- Read Chunk Data
// for read tile of file
void read_chunk_data( FILE *pict_fp, chunk_data *chunk_data, unsigned int table_start, unsigned int length, unsigned char twoByte ) {
   printf( "twoByte %d\n", twoByte );
   unsigned short dataLength = 0;
   unsigned short chunk_index = table_start;
   unsigned short end = table_start + length;
   while( chunk_index < end ) {
      if( twoByte )
         dataLength = fgetc(pict_fp) << 8 | fgetc(pict_fp);
      else
         dataLength = fgetc(pict_fp);

      if( dataLength > 0 ) {
         chunk_data->chunk_size[chunk_index] = dataLength;
         chunk_data->chunk_table_position[chunk_index] = ftell( pict_fp );
         // ---- next chunk
         fseek( pict_fp, dataLength, SEEK_CUR );
      }

      chunk_index++;
   }
}

// For see table
void printChunkTable( chunk_data *chunk_data, unsigned int table_length ) {
   
   unsigned short chunk_index = 0;
   while( chunk_index < table_length ) {
      printf( "%d - %d  %d   %p %p\n", chunk_index, chunk_data->chunk_table_position[chunk_index], chunk_data->chunk_size[chunk_index], chunk_data->chunk_table_position, chunk_data->chunk_size );

      chunk_index++;
   }
}


#pragma mark ----- RLE Compress
void uncompress_rle( char *compressed_buffer, int compressed_buffer_length, unsigned char *uncompressed_buffer, int uncompressed_buffer_length) {
   
   // ---- while not finish all in buffer
   while( compressed_buffer_length > 0 ) {
      char byteCount = *compressed_buffer;
      // ---- move to next byte
      compressed_buffer++;

      // ---- if byte value ≥ 0
      if( byteCount > -1 ) {
         // ---- count for not same byte value
         int count = byteCount + 1;
//         printf( "  chép count %d ", count );
         // ---- reduce amount count of bytes remaining for in buffer
         compressed_buffer_length -= count;
         compressed_buffer_length--;
         // ---- if count larger than out buffer length still available
         if (0 > (uncompressed_buffer_length -= count)) {
            printf( "  uncompress_rle: Error buffer not length not enough. Have %d but need %d\n", uncompressed_buffer_length, count );
            exit(0);
            return;
         }
         // ---- copy not same byte and move to next byte
         while( count-- > 0 )
            *uncompressed_buffer++ = *(compressed_buffer++);

      }
      else if( byteCount < 0 ) {
         // ---- count number bytes same
         int count = -byteCount+1;
         unsigned char valueForCopy = *compressed_buffer;
//         printf( "  repeat count %d (%d) ", count, valueForCopy );
         // ---- reduce amount count of remaining bytes for in buffer
         compressed_buffer_length -= 2;
         // ---- if count larger than out buffer length still available
         if (0 > (uncompressed_buffer_length -= count)) {
            printf( "  uncompress_rle: Error buffer not length not enough. Have %d but need %d\n", uncompressed_buffer_length, count );
            exit(0);
            return;
         }
         // ---- copy same byte
         while( count-- > 0 )
            *uncompressed_buffer++ = valueForCopy;
         // ---- move to next byte
         compressed_buffer++;
      }
   
   }

}

void uncompress_rle_16bit( char *compressed_buffer, int compressed_buffer_length, unsigned char *uncompressed_buffer, int uncompressed_buffer_length) {
   
   // ---- while not finish all in buffer
   while( compressed_buffer_length > 0 ) {
      char byteCount = *compressed_buffer;
      // ---- move to next byte
      compressed_buffer++;
      
      // ---- if byte value ≥ 0
      if( byteCount > -1 ) {
         // ---- count for not same byte value
         int count = byteCount + 1;
         //         printf( "  chép count %d ", count );
         // ---- reduce amount count of 2*bytes remaining for in buffer
         compressed_buffer_length -= count << 1;
         compressed_buffer_length--;
         // ---- if count larger than out buffer length still available
         if (0 > (uncompressed_buffer_length -= count << 1)) {
            printf( "  uncompress_rle: Error buffer not length not enough. Have %d but need %d\n", uncompressed_buffer_length, count );
            exit(0);
            return;
         }
         // ---- copy not same 2 byte and move to next 2 byte
         while( count-- > 0 ) {
            *uncompressed_buffer++ = *(compressed_buffer++);
            *uncompressed_buffer++ = *(compressed_buffer++);
         }
         
      }
      else if( byteCount < 0 ) {
         // ---- count number bytes same
         int count = -byteCount+1;
         unsigned char valueForCopy0 = *compressed_buffer;
         compressed_buffer++;
         unsigned char valueForCopy1 = *compressed_buffer;
         // ---- move to next byte
         compressed_buffer++;
         //         printf( "  repeat count %d (%d) ", count, valueForCopy );
         // ---- reduce amount count of remaining bytes for in buffer
         compressed_buffer_length -= 3;
         // ---- if count larger than out buffer length still available
         if (0 > (uncompressed_buffer_length -= count << 1)) {
            printf( "  uncompress_rle: Error buffer not length not enough. Have %d but need %d\n", uncompressed_buffer_length, count );
            exit(0);
            return;
         }
         // ---- copy same 2 byte
         while( count-- > 0 ) {
            *uncompressed_buffer++ = valueForCopy0;
            *uncompressed_buffer++ = valueForCopy1;
         }
      }
      
   }
   
}

#pragma mark ---- Copy Data
void copyBufferUchar( unsigned char *destination, unsigned char *ucharSource, unsigned int length ) {

   unsigned int index = 0;
   while( index < length ) {
      *destination = *ucharSource;
      destination++;
      ucharSource++;
      index++;
   }
}


#pragma mark ---- RLE Compression
void read_data_compression_rle__scanline_32bit( FILE *pict_fp, chunk_data *chunk_data, image_data *image_data ) {

   unsigned int num_columns = image_data->width;
   unsigned int num_rows = image_data->height;

   unsigned short componentCount = image_data->componentCount;
   unsigned int uncompressed_data_length = num_columns << 2;  // RGBO

   char *compressed_buffer = malloc( uncompressed_data_length << 1 );
   unsigned char *uncompressed_buffer = malloc( uncompressed_data_length );
   if( compressed_buffer == NULL ) {
      printf( "Problem create compressed buffer\n" );
      exit(0);
   }
   
   if( uncompressed_buffer == NULL ) {
      printf( "Problem create uncompressed buffer\n" );
      exit(0);
   }

   unsigned short chunk_number = 0;
   while( chunk_number < chunk_data->num_chunks ) {
      // ---- begin chunk
      fseek( pict_fp, chunk_data->chunk_table_position[chunk_number], SEEK_SET );

      // ---- read data length
      unsigned int data_length = chunk_data->chunk_size[chunk_number];

      // ---- read compressed data
      fread( compressed_buffer, 1, data_length, pict_fp );

      // ---- uncompress rle data
      uncompress_rle( compressed_buffer, data_length, uncompressed_buffer, uncompressed_data_length );

      // ---- address for component data from umcopressed buffer
      unsigned int bufferIndexR = 0;
      unsigned int bufferIndexG = 0;
      unsigned int bufferIndexB = 0;
      unsigned int bufferIndexO = 0;

      if( componentCount == 3 ) {
         bufferIndexR = 0;
         bufferIndexG = num_columns;
         bufferIndexB = num_columns << 1;
      }
      else if( componentCount == 4 ) {
         bufferIndexO = 0;
         bufferIndexR = num_columns;
         bufferIndexG = num_columns << 1;
         bufferIndexB = num_columns*3;
      }
      
      // ---- address for copy in channel
      unsigned int channelDataIndex = num_columns*(num_rows - 1 - chunk_number);
   
      unsigned int uncompressed_buffer_index = 0;
      while( uncompressed_buffer_index < num_columns ) {
         image_data->channel_R[channelDataIndex] = uncompressed_buffer[bufferIndexR];
         image_data->channel_G[channelDataIndex] = uncompressed_buffer[bufferIndexG];
         image_data->channel_B[channelDataIndex] = uncompressed_buffer[bufferIndexB];
         
         if( componentCount == 3 )
            image_data->channel_O[channelDataIndex] = 0xff;
         else if( componentCount == 4 )
            image_data->channel_O[channelDataIndex] = uncompressed_buffer[bufferIndexO];

         bufferIndexR++;
         bufferIndexG++;
         bufferIndexB++;
         bufferIndexO++;
         uncompressed_buffer_index++;

         channelDataIndex++;
      }

      chunk_number++;
   }

   // ---- free memory
   free( compressed_buffer );
   free( uncompressed_buffer );
}


// ==== 16 bit pixel format
// 1111 11
// 5432 1098 7654 3210
// -------------------
// xxxx xxxx xxxx xxxx
// -rrr rrgg gggb bbbb
void read_data_compression_rle__scanline_16bit( FILE *pict_fp, chunk_data *chunk_data, image_data *image_data ) {
   
   unsigned int num_columns = image_data->width;
   unsigned int num_rows = image_data->height;
   
   unsigned short componentCount = image_data->componentCount;
   unsigned int uncompressed_data_length = num_columns << 1;  //16 bit
   
   char *compressed_buffer = malloc( uncompressed_data_length << 1 );
   unsigned char *uncompressed_buffer = malloc( uncompressed_data_length );
   if( compressed_buffer == NULL ) {
      printf( "Problem create compressed buffer\n" );
      exit(0);
   }
   
   if( uncompressed_buffer == NULL ) {
      printf( "Problem create uncompressed buffer\n" );
      exit(0);
   }
   
   unsigned short chunk_number = 0;
   while( chunk_number < chunk_data->num_chunks ) {
      // ---- begin chunk
      fseek( pict_fp, chunk_data->chunk_table_position[chunk_number], SEEK_SET );
      
      // ---- read data length
      unsigned int data_length = chunk_data->chunk_size[chunk_number];

      // ---- read compressed data
      fread( compressed_buffer, 1, data_length, pict_fp );
      
      // ---- uncompress rle 16 bit data
      uncompress_rle_16bit( compressed_buffer, data_length, uncompressed_buffer, uncompressed_data_length );
      
      // ---- address for component data from umcopressed buffer
      unsigned int bufferIndex = 0;

      // ---- address for copy in channel
      unsigned int channelDataIndex = num_columns*(num_rows - 1 - chunk_number);
      
      unsigned int uncompressed_buffer_index = 0;
      while( uncompressed_buffer_index < num_columns ) {
         unsigned short pixel16bit = uncompressed_buffer[bufferIndex] << 8 | uncompressed_buffer[bufferIndex+1];
         unsigned char red = (pixel16bit >> 10) & 0x1f;
         unsigned char green = (pixel16bit >> 5) & 0x1f;
         unsigned char blue = pixel16bit & 0x1f;
   
         // ---- use convert method from Imaging With QuickDraw 1994 page 4-17
         image_data->channel_R[channelDataIndex] = (red << 3) | (red >> 2);
         image_data->channel_G[channelDataIndex] = (green << 3) | (green >> 2);
         image_data->channel_B[channelDataIndex] = (blue << 3) | (blue >> 2);
         image_data->channel_O[channelDataIndex] = 0xff;

         bufferIndex += 2;

         uncompressed_buffer_index++;
         channelDataIndex++;
      }
      
      chunk_number++;
   }

   // ---- free memory
   free( compressed_buffer );
   free( uncompressed_buffer );
}


#pragma mark ---- Compress RLE
// ---- Hàm từ thư viện OpenEXR của ILM
#define MIN_RUN_LENGTH   3
#define MAX_RUN_LENGTH 127
unsigned int compress_chunk_RLE(unsigned char *in, unsigned int inLength, unsigned char *out ) {
   const unsigned char *inEnd = in + inLength;
   const unsigned char *runStart = in;
   const unsigned char *runEnd = in + 1;
   unsigned char *outWrite = out;
   
   // ---- while not at end of in buffer
   while (runStart < inEnd) {
      
      // ---- count number bytes same value; careful not go beyond end of chunk, or chunk longer than MAX_RUN_LENGTH
      while (runEnd < inEnd && *runStart == *runEnd && runEnd - runStart - 1 < MAX_RUN_LENGTH) {
         ++runEnd;
      }
      // ---- if number bytes same value >= MIN_RUN_LENGTH
      if (runEnd - runStart >= MIN_RUN_LENGTH) {
         //
         // Compressable run
         //
         // ---- number bytes same value - 1
         char count = (runEnd - runStart) - 1;
         *outWrite++ = -count;
         // ---- byte value
         *outWrite++ = *(signed char *) runStart;
         // ---- move to where different value found or MAX_RUN_LENGTH
         runStart = runEnd;
      }
      else {
         //
         // Uncompressable run
         //
         // ---- count number of bytes not same; careful end of chunk,
         while (runEnd < inEnd &&
                ((runEnd + 1 >= inEnd || *runEnd != *(runEnd + 1)) || (runEnd + 2 >= inEnd || *(runEnd + 1) != *(runEnd + 2))) &&
                runEnd - runStart < MAX_RUN_LENGTH) {
            ++runEnd;
         }
         // ---- number bytes not same
         *outWrite++ = runEnd - runStart - 1;
         // ---- not same bytes
         while (runStart < runEnd) {
            *outWrite++ = *(signed char *) (runStart++);
         }
      }
      // ---- move to next byte
      ++runEnd;
   }
   
   return outWrite - out;
}

#pragma mark ---- OpCode Frame
void opCodeFrame( FILE *pict_fp, unsigned short top, unsigned short left, unsigned short bottom, unsigned short right ) {

   // ---- op code number 0x0001
   fputc( 0x00, pict_fp );
   fputc( 0x01, pict_fp );
   
   // ---- size (include this ushort)
   fputc( 0x00, pict_fp );
   fputc( 0x0a, pict_fp );
   
   // ---- top
   fputc( top >> 8, pict_fp );
   fputc( top & 0xff, pict_fp );
   // ---- left
   fputc( left >> 8, pict_fp );
   fputc( left & 0xff, pict_fp );
   // ---- bottom
   fputc( bottom >> 8, pict_fp );
   fputc( bottom & 0xff, pict_fp );
   // ---- right
   fputc( right >> 8, pict_fp );
   fputc( right & 0xff, pict_fp );
}

#pragma mark ---- OpCode PixMap
void opCodePixMap( FILE *pict_fp, image_data *image ) {

   // ---- image frame
   unsigned short right = image->width;
   unsigned short bottom = image->height;

   // ---- op code number 0x009a PixMap
   fputc( 0x00, pict_fp );
   fputc( 0x9a, pict_fp );
   
   // ---- pixMap (50 byte)  page 4-46 Imaging With Quickdraw year 1994
   // ---- baseAddr 4 byte
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( 0xff, pict_fp );
   
   // ---- rowBytes 2 byte
   unsigned short rowBytes = right << 2;
   
   unsigned char twoByte = 0x01;
   if( rowBytes < 250 )
      twoByte = 0;

   rowBytes |= 0x8000;  // add pixMap flag
   fputc( rowBytes >> 8, pict_fp );
   fputc( rowBytes & 0xff, pict_fp );

   // ---- frame
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( bottom >> 8, pict_fp );
   fputc( bottom & 0xff, pict_fp );
   fputc( right >> 8, pict_fp );
   fputc( right & 0xff, pict_fp );

   // ---- pixMapVersion 2 
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   
   // ---- packType  2 bytes
   fputc( 0x00, pict_fp );
   fputc( 0x04, pict_fp );
   
   //  ---- packSize     4 bytes
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   
   // ---- resolution x (fix point 16.16)
   fputc( 0x00, pict_fp );
   fputc( 0x48, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );

   // ---- resolution y (fix point 16.16)
   fputc( 0x00, pict_fp );
   fputc( 0x48, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   
   // ---- pixelType (0 == index; 16 = direct)
   fputc( 0x00, pict_fp );
   fputc( 0x10, pict_fp );
   
   // ---- pixelSize (bits: 1; 2; 4; 8; 16; 32 bits)
   fputc( 0x00, pict_fp );
   fputc( 0x20, pict_fp );
   
   // ---- componentCount (1 == index; 3 == direct RGB, 4 == ORGB )
   fputc( 0x00, pict_fp );
   fputc( 0x04, pict_fp );
   
   // ---- componentSize (bits: 1; 2; 4; 8;  5 for 16 bit RGB)
   fputc( 0x00, pict_fp );
   fputc( 0x08, pict_fp );
   
   // ---- planeBytes
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );

   // ---- pixMapTable (Handle)
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   
   // ---- pixMapReserve
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   
   // ---- source rect
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( bottom >> 8, pict_fp );
   fputc( bottom & 0xff, pict_fp );
   fputc( right >> 8, pict_fp );
   fputc( right & 0xff, pict_fp );
   
   // ---- dest rect
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( bottom >> 8, pict_fp );
   fputc( bottom & 0xff, pict_fp );
   fputc( right >> 8, pict_fp );
   fputc( right & 0xff, pict_fp );
   
   // ---- mode
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );

   // ==== PixData
   unsigned char *uncompressed_buffer = malloc( right << 2 ); // ORGB
   unsigned char *compressed_buffer = malloc( right << 3 ); //
   
   if( (uncompressed_buffer == NULL) || (compressed_buffer == NULL) ) {
      printf( "Problem create buffer for compress image\n" );
      exit(0);
   }
   

   unsigned short row = 0;
   while( row < bottom ) {
      // ---- flip image/lật ảnh
      unsigned int channelDataIndex = bottom*(right - 1 - row);
      // ---- copy data in buffer, order O R G B
      unsigned short indexO = 0;
      unsigned short indexR = right;
      unsigned short indexG = right << 1;
      unsigned short indexB = right*3;
      unsigned short index = 0;
      while( index < right ) {
         uncompressed_buffer[indexO] = image->channel_O[channelDataIndex];
         uncompressed_buffer[indexR] = image->channel_R[channelDataIndex];
         uncompressed_buffer[indexG] = image->channel_G[channelDataIndex];
         uncompressed_buffer[indexB] = image->channel_B[channelDataIndex];
         index++;
         indexO++;
         indexR++;
         indexG++;
         indexB++;
         channelDataIndex++;
      }
      
      // ---- compress buffer
      unsigned short dataSize = compress_chunk_RLE( uncompressed_buffer, right << 2, compressed_buffer );
/*      printf( " Data size : %d\n", dataSize );
      index = 0;
      while( index < dataSize ) {
         printf( " %02x", compressed_buffer[index] );
         index++;
      }
      printf( "\n" );
      exit(0);
      */
      // ---- data size
      if( twoByte ) {
         fputc( dataSize >> 8, pict_fp );
         fputc( dataSize & 0xff, pict_fp );
      }
      else
         fputc( dataSize & 0xff, pict_fp );
   
      // ---- data
      index = 0;
      unsigned char *buffer = compressed_buffer;
      while( index < dataSize ) {
         fputc( *buffer, pict_fp );
         buffer++;
         index++;
      }
      
      // ---- next row
      row++;
   }
   
   // ---- giữ số lượng byte chẵn
   if( ftell( pict_fp ) & 1 )
      fputc( 0x00, pict_fp );
}

#pragma mark ---- Decode PICT
image_data decode_pict( const char *sfile ) {

   FILE *pict_fp;
   image_data duLieuAnhPICT;
   duLieuAnhPICT.width = 0;
   duLieuAnhPICT.height = 0;
   
   printf( "docPICT: TenTep %s\n", sfile );
   
   /* open pict using filename */
   pict_fp = fopen(sfile, "rb");
   
   if (!pict_fp) {
      printf("%-15.15s: Error open exr file: %s\n","read_exr",sfile);
      exit(0);
   }

   // ---- skip 512 byte head
   fseek( pict_fp, 512 + 10, SEEK_SET );
   
   // ---- read file type and version
   unsigned short fileType = 0x0;
   unsigned short version = 0x0;
   fileType = fgetc(pict_fp) << 8 | fgetc(pict_fp);
   version = fgetc(pict_fp) << 8 | fgetc(pict_fp);

   if( (fileType != kFILE_TYPE__VERSION_2) || (version != kVERSION_CODE__VERSION_2 ) ) {
      printf( "%-15.15s: For PICT 2.0: Wrong file type 0x%02x and version code 0x%02x\n", sfile, kFILE_TYPE__VERSION_2, kVERSION_CODE__VERSION_2 );
      printf( "%s is not a valid PICT 2.0 file", sfile);
      return duLieuAnhPICT;
   }
   
   chunk_data chunk_list;

   // ---- read op code list
   fseek( pict_fp, 512 + 40, SEEK_SET );
   
   unsigned short opCode = OP_CODE__NO_OP;
   unsigned char readClip = 0;
   
   while( (opCode != OP_CODE__END) && !feof(pict_fp) ) {
      // ---- get op code
      opCode = fgetc(pict_fp) << 8 | fgetc(pict_fp);
      printf( "opcode %04x\n", opCode );

      if( opCode == OP_CODE__CLIP ) {
         unsigned short size = fgetc(pict_fp) << 8 | fgetc(pict_fp);
         
         if( readClip ) {
            // ---- skip
            fseek( pict_fp, size - 2, SEEK_CUR );
         }
         else {
            unsigned short frameTop = fgetc(pict_fp) << 8 | fgetc(pict_fp);
            unsigned short frameLeft = fgetc(pict_fp) << 8 | fgetc(pict_fp);
            unsigned short frameBottom = fgetc(pict_fp) << 8 | fgetc(pict_fp);
            unsigned short frameRight = fgetc(pict_fp) << 8 | fgetc(pict_fp);
            
            if( frameRight > frameLeft )
               duLieuAnhPICT.width = frameRight - frameLeft;
            else
               duLieuAnhPICT.width = frameLeft - frameRight;
            
            if( frameBottom > frameTop )
               duLieuAnhPICT.height = frameBottom - frameTop;
            else
               duLieuAnhPICT.height = frameTop - frameBottom;
            printf( "Frame %d %d %d %d\n", frameTop, frameLeft, frameBottom, frameRight );
            printf( "Image size %d %d\n", duLieuAnhPICT.width, duLieuAnhPICT.height );
            chunk_list = create_chunk_table( duLieuAnhPICT.height );
            
            readClip = 1;
         }

      }
      else if( opCode == OP_CODE__FRAME_RRECT ) {
         // ---- skip
         fseek( pict_fp, 8, SEEK_CUR );
      }
      else if( opCode == OP_CODE__PACK_BITS_RECT ) {
         // ---- pixMap
         
         // ---- color table
            // color table seed 4 bytes
            // color table flag 2 bytes
            // color table size 2 bytes
            // color table array
               // value 2 bytes
               // red 2 bytes
               // green 2 bytes
               // blue 2 bytes

         // ---- source rect
         
         // ---- dest rect
         
         // ---- PixData
         exit(0);  // chưa làm
         
      }
      else if( opCode == OP_CODE__DIRECT_BITS_RECT ) {
         // ==== START PixMap
         // ---- pixMap (50 byte)  page 4-46 Imaging With Quickdraw year 1994
         //   bassAddr 4 byte (ignore)
         //   rowBytes 2 byte
         fseek( pict_fp, 4, SEEK_CUR );
         unsigned short rowBytes = fgetc(pict_fp) << 8 | fgetc(pict_fp);

         //   bounds Rect 8 bytes
         unsigned short top = fgetc(pict_fp) << 8 | fgetc(pict_fp);
         unsigned short left = fgetc(pict_fp) << 8 | fgetc(pict_fp);
         unsigned short bottom = fgetc(pict_fp) << 8 | fgetc(pict_fp);
         unsigned short right = fgetc(pict_fp) << 8 | fgetc(pict_fp);
//         printf( "  rowBytes %d  left %d  top %d  right %d  bottom %d\n", rowBytes, left, top, right, bottom );

         //   pixMapVersion 2 bytes

         //   packType  2 bytes
         fseek( pict_fp, 2, SEEK_CUR );
         unsigned short packType = fgetc(pict_fp) << 8 | fgetc(pict_fp);
      
         //   packSize     4 bytes
         //   horz resolut 4 bytes (fix point 16.16)
         //   vert resolut 4 bytes (fix point 16.16)

         //   pixelType   2 bytes (0 == index; 16 = direct)
         fseek( pict_fp, 12, SEEK_CUR );
         unsigned short pixelType = fgetc(pict_fp) << 8 | fgetc(pict_fp);
         
         //   pixelSize   2 bytes (bits: 1; 2; 4; 8; 16; 32 bits)
         unsigned short pixelSize = fgetc(pict_fp) << 8 | fgetc(pict_fp);
         duLieuAnhPICT.pixelSize = pixelSize;
         if( (pixelSize < 16) ) {
            printf( "PixelSize = %d, only support 32 or 16 bit.\n", pixelSize );
            exit(0);
         }
            
         //   componentCount    2 bytes (1 == index; 3 == direct RGB )
         unsigned short componentCount = fgetc(pict_fp) << 8 | fgetc(pict_fp);
         duLieuAnhPICT.componentCount = componentCount;

         //   componentSize    2 bytes (bits: 1; 2; 4; 8;  5 for 16 bit RGB)
         //   planeBytes   4 bytes
         //   pixMapTable  4 bytes (Handle)
         //   pixMapReserve 4 bytes (ignore)
         // ==== END PixMap
   
         // ---- source rect (8 byte)
         fseek( pict_fp, 14, SEEK_CUR );
         unsigned short rectSourceTop = fgetc(pict_fp) << 8 | fgetc(pict_fp);
         unsigned short rectSourceLeft = fgetc(pict_fp) << 8 | fgetc(pict_fp);
         unsigned short rectSourceBottom = fgetc(pict_fp) << 8 | fgetc(pict_fp);
         unsigned short rectSourceRight = fgetc(pict_fp) << 8 | fgetc(pict_fp);
//         printf( "rectSource %d %d %d %d\n", rectSourceTop, rectSourceLeft, rectSourceBottom, rectSourceRight );
         // ---- dest rect (8 byte)
         
         // ---- mode (2 byte)
         fseek( pict_fp, 8, SEEK_CUR );
         unsigned short mode = fgetc(pict_fp) << 8 | fgetc(pict_fp);

         // ---- remove PixMap flag
         rowBytes &= 0x7fff;
         printf( " rowBytes %d (%x) %d  packType %d  pixelType %d  pixelSize %d  mode %d\n", rowBytes, rowBytes, rowBytes & 0x7fff, packType, pixelType, pixelSize , mode);
 
         // ---- PixData (scan line)
         read_chunk_data( pict_fp, &chunk_list, rectSourceTop, rectSourceBottom - rectSourceTop, rowBytes > 250 );
         
//         printChunkTable( &chunk_list, rectSourceBottom - rectSourceTop );
         
         // ---- giữ số lượng byte chẵn
         if( ftell( pict_fp ) & 1 )
            fgetc(pict_fp);
      }
      
      if( opCode == 0xffff )
         opCode = OP_CODE__END;
   }
   
   // ---- create buffers for image data
   unsigned int beDaiDem = duLieuAnhPICT.width*duLieuAnhPICT.height;  // uchar
   printf( " beDaiDem %d\n", beDaiDem );

   duLieuAnhPICT.channel_B = malloc( beDaiDem );
   duLieuAnhPICT.channel_G = malloc( beDaiDem );
   duLieuAnhPICT.channel_R = malloc( beDaiDem );
   duLieuAnhPICT.channel_O = malloc( beDaiDem );

   // ---- check channel
   if( (duLieuAnhPICT.channel_B == NULL) || (duLieuAnhPICT.channel_G == NULL) ||
      (duLieuAnhPICT.channel_R == NULL) || (duLieuAnhPICT.channel_O == NULL) ) {
      printf( "Problem create image channel\n");
      exit(0);
   }
   
   if( duLieuAnhPICT.pixelSize > 16 )
      read_data_compression_rle__scanline_32bit( pict_fp, &chunk_list, &duLieuAnhPICT );
   else
      read_data_compression_rle__scanline_16bit( pict_fp, &chunk_list, &duLieuAnhPICT );
   
   free( chunk_list.chunk_table_position );
   free( chunk_list.chunk_size );

   fclose( pict_fp );

   return duLieuAnhPICT;
}

#pragma mark ----
void encode_pict( const char *sfile, image_data *image ) {
   
   FILE *pict_fp = fopen( sfile, "wb" );

   if( pict_fp == NULL ) {
      printf( "Problem create file %s\n", sfile );
      exit(0);
   }
   
   // ---- skip 512 byte head
   unsigned int index = 0;
   while( index < 512 ) {
      fputc( 0x00, pict_fp );
      index++;
   }
   
   // ---- image frame
   fputc( 0x00, pict_fp );
   fputc( 0x0a, pict_fp );
   

   unsigned short right = image->width;
   unsigned short bottom = image->height;
   // ---- top
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   // ---- left
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   // ---- bottom
   fputc( bottom >> 8, pict_fp );
   fputc( bottom & 0xff, pict_fp );
   // ---- right
   fputc( right >> 8, pict_fp );
   fputc( right & 0xff, pict_fp );
   
   // ---- version PICT 2.0 - Phiên Bản 2.0
   fputc( 0x00, pict_fp );
   fputc( 0x11, pict_fp );
   
   fputc( 0x02, pict_fp );
   fputc( 0xff, pict_fp );
   
   fputc( 0x0c, pict_fp );
   fputc( 0x00, pict_fp );
   
   // ---- use this value is PixMap Handle is zero
   fputc( 0xff, pict_fp );
   fputc( 0xff, pict_fp );
   fputc( 0xff, pict_fp );
   fputc( 0xff, pict_fp );
   
   // ---- resolution x
   fputc( 0x00, pict_fp );
   fputc( 0x48, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   
   // ---- resolution y
   fputc( 0x00, pict_fp );
   fputc( 0x48, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   
   // ---- frame
   fputc( right >> 8, pict_fp );
   fputc( right & 0xff, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( bottom >> 8, pict_fp );
   fputc( bottom & 0xff, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   
   // ---- reserve
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   fputc( 0x00, pict_fp );
   
   // ---- add op code Frame
   opCodeFrame( pict_fp, 0, 0, bottom, right );
   
   // ---- add op code PixMap
   opCodePixMap( pict_fp, image );
   
   // ---- end
   fputc( 0x00, pict_fp );
   fputc( 0xff, pict_fp );
   
   fclose( pict_fp );
}


#pragma mark ---- Phân Tích Đuôi Tập Tin
unsigned char phanTichDuoiTapTin( char *tenTapTin ) {

   // ---- đến cuối cùnh tên
   while( *tenTapTin != 0x00 ) {
      tenTapTin++;
   }

   // ---- trở lại 3 cái
   tenTapTin -= 3;
   
   // ---- xem có đuôi nào
   unsigned char kyTu0 = *tenTapTin;
   tenTapTin++;
   unsigned char kyTu1 = *tenTapTin;
   tenTapTin++;
   unsigned char kyTu2 = *tenTapTin;
   
   unsigned char loaiTapTin = 0;

   if( (kyTu0 == 'p') || (kyTu0 == 'P')  ) {
      if( (kyTu1 == 'n') || (kyTu1 == 'N')  ) {
         if( (kyTu2 == 'g') || (kyTu2 == 'G')  ) {
            loaiTapTin = kPNG;
         }
      }
      else  if( (kyTu1 == 'c') || (kyTu1 == 'C')  ) {
         if( (kyTu2 == 't') || (kyTu2 == 'T')  ) {
            loaiTapTin = kPICT;
         }
      }
   }
   
   
   return loaiTapTin;
}


#pragma mark ==== Đuôi Tập Tin
void tenAnhPNG( char *tenAnhGoc, char *tenAnhPNG ) {
   
   // ---- chép tên ảnh gốc
   while( *tenAnhGoc != 0x00 ) {
      *tenAnhPNG = *tenAnhGoc;
      tenAnhPNG++;
      tenAnhGoc++;
   }
   
   // ---- trở lại 3 cái
   tenAnhPNG -= 3;
   
   // ---- kèm đuôi PNG
   *tenAnhPNG = 'p';
   tenAnhPNG++;
   *tenAnhPNG = 'n';
   tenAnhPNG++;
   *tenAnhPNG = 'g';
   tenAnhPNG++;
   *tenAnhPNG = 0x0;
}

void tenAnhPICT( char *tenAnhGoc, char *tenAnhPCT ) {
   
   // ---- chép tên ảnh gốc
   while( *tenAnhGoc != 0x00 ) {
      *tenAnhPCT = *tenAnhGoc;
      tenAnhPCT++;
      tenAnhGoc++;
   }
   
   // ---- trở lại 3 cái
   tenAnhPCT -= 3;
   
   // ---- kèm đuôi PNG
   *tenAnhPCT = 'p';
   tenAnhPCT++;
   *tenAnhPCT = 'c';
   tenAnhPCT++;
   *tenAnhPCT = 't';
   tenAnhPCT++;
   *tenAnhPCT = 0x0;
}


#pragma mark ==== main.c
int main( int argc, char **argv ) {

   if( argc > 1 ) {
      // ---- phân tích đuôi tập tin
      unsigned char loaiTapTin = 0;
      loaiTapTin = phanTichDuoiTapTin( argv[1] );
      
      if( loaiTapTin == kPICT ) {
         printf( " PICT file\n" );
         image_data anhPICT = decode_pict( argv[1] );
         
         // ---- chuẩn bị tên tập tin
         char tenTep[255];
         tenAnhPNG( argv[1], tenTep );
         
         // ---- pha trộn các kênh
         unsigned int chiSoCuoi = anhPICT.width*anhPICT.height << 2;
         unsigned char *demPhaTron = malloc( chiSoCuoi );
         
         if( demPhaTron != NULL ) {
            
            unsigned int chiSo = 0;
            unsigned int chiSoKenh = 0;
            
            while( chiSo < chiSoCuoi ) {
               
               demPhaTron[chiSo] = anhPICT.channel_R[chiSoKenh];
               demPhaTron[chiSo+1] = anhPICT.channel_G[chiSoKenh];
               demPhaTron[chiSo+2] = anhPICT.channel_B[chiSoKenh];
               demPhaTron[chiSo+3] = anhPICT.channel_O[chiSoKenh];
               chiSo += 4;
               chiSoKenh++;
            }
            
            // ---- lưu tập tin PNG
            luuAnhPNG( tenTep, demPhaTron, anhPICT.width, anhPICT.height, kPNG_BGRO );
         }
         
         free( anhPICT.channel_R );
         free( anhPICT.channel_G );
         free( anhPICT.channel_B );
         free( anhPICT.channel_O );
      }
      else if( loaiTapTin == kPNG ) {
         printf( " PNG file\n" );

         unsigned int beRong = 0;
         unsigned int beCao = 0;
         unsigned char canLatMau = 0;
         unsigned char loaiPNG;
         unsigned char *duLieuAnhPNG = docPNG( argv[1], &beRong, &beCao, &canLatMau, &loaiPNG );

         image_data anhPNG;
         anhPNG.width = beRong;
         anhPNG.height = beCao;
         anhPNG.componentCount = 4;
         printf( "Size %d x %d\n", beRong, beCao );
         // ----
         anhPNG.channel_O = malloc( beRong*beCao );
         anhPNG.channel_R = malloc( beRong*beCao );
         anhPNG.channel_G = malloc( beRong*beCao );
         anhPNG.channel_B = malloc( beRong*beCao );
   
         if( (anhPNG.channel_O == NULL) || (anhPNG.channel_R == NULL )
            || (anhPNG.channel_G == NULL) || (anhPNG.channel_B == NULL) ) {
            printf( "Problem create channel for image\n" );
            exit(0);
         }
         
         unsigned int chiSoDuLieuAnh = 0;
         unsigned int chiSoDuLieuAnhCuoi = beRong*beCao << 2;
         unsigned int chiSoKenh = 0;
         while( chiSoDuLieuAnh < chiSoDuLieuAnhCuoi ) {
            anhPNG.channel_O[chiSoKenh] = duLieuAnhPNG[chiSoDuLieuAnh+3];
            anhPNG.channel_R[chiSoKenh] = duLieuAnhPNG[chiSoDuLieuAnh];
            anhPNG.channel_G[chiSoKenh] = duLieuAnhPNG[chiSoDuLieuAnh+1];
            anhPNG.channel_B[chiSoKenh] = duLieuAnhPNG[chiSoDuLieuAnh+2];
            
            chiSoKenh++;
            chiSoDuLieuAnh += 4;
         }
         
         // ---- chuẩn bị tên tập tin
         char tenTep[255];
         tenAnhPICT( argv[1], tenTep );
         printf( " %s\n", tenTep );
         
         // ---- encode PICT
         encode_pict( tenTep, &anhPNG );
      }

   }
   
   return 0;
}





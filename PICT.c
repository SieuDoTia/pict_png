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
   2 bytes  | ushort | CODE        | for version 2.0 should equal 0xffef or 0xffee
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
#define OP_CODE__PACK_BITS_RECT 0x0098   // +???

#define OP_CODE__DIRECT_BITS_RECT 0x009a   // +???   cho RGB
#define OP_CODE__DIRECT_BITS_RGN  0x009b   // +???   cho RGB

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
void read_chunk_data( FILE *pict_fp, chunk_data *chunk_data, unsigned int table_start, unsigned int length ) {

   unsigned short dataLength = 0;
   unsigned short chunk_index = table_start;
   unsigned short end = table_start + length;
   while( chunk_index < end ) {
      dataLength = fgetc(pict_fp) << 8 | fgetc(pict_fp);

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
         //   exit(0);
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
          //  exit(0);
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


#pragma mark ---- No Compression


#pragma mark ---- RLE Compression
void read_data_compression_rle__scanline( FILE *pict_fp, chunk_data *chunk_data, image_data *image_data ) {

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
//       printf( " chunk %d  data_length %d  channelDataIndex %d \n", chunk_number, data_length, channelDataIndex );
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
         else
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
   printf( "componentCount %d  image_data->channel %d %d %d %d\n", componentCount, image_data->channel_R[0], image_data->channel_G[0],
          image_data->channel_B[0], image_data->channel_O[0] );
   // ---- free memory
   free( compressed_buffer );
   free( uncompressed_buffer );
}


image_data decode_pict( const char *sfile ) {

   FILE *pict_fp;
   image_data duLieuAnhPICT;
   duLieuAnhPICT.width = 0;
   duLieuAnhPICT.height = 0;
   
   printf( "docPICT: TenTep %s\n", sfile );
    
   /* open pict using filename */
   pict_fp = fopen(sfile, "rb");
    
   if (!pict_fp) {
      printf("%-15.15s: Error open exr file %s\n","read_exr",sfile);
      return duLieuAnhPICT;
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
      else if( opCode == OP_CODE__DIRECT_BITS_RECT ) {
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
         if( pixelSize != 32 ) {
            printf( "PixelSize = %d, only support 32 bit.\n", pixelSize );
            exit(0);
         }
            
         //   componentCount    2 bytes (1 == index; 3 == direct RGB )
         unsigned short componentCount = fgetc(pict_fp) << 8 | fgetc(pict_fp);
         duLieuAnhPICT.componentCount = componentCount;

         //   componentSize    2 bytes (bits: 1; 2; 4; 8;  5 for 16 bit RGB)
         //   planeBytes   4 bytes
         //   pixMapTable  4 bytes (Handle)
         //   pixMapReserve 4 bytes (ignore)
   
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
         
         printf( " rowBytes %d  packType %d  pixelType %d  pixelSize %d  mode %d\n", rowBytes, packType, pixelType, pixelSize , mode);
 
         // ---- PixData (scan line)
         read_chunk_data( pict_fp, &chunk_list, rectSourceTop, rectSourceBottom - rectSourceTop );
         
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
   
   read_data_compression_rle__scanline( pict_fp, &chunk_list, &duLieuAnhPICT );
   
   free( chunk_list.chunk_table_position );
   free( chunk_list.chunk_size );

   fclose( pict_fp );

   return duLieuAnhPICT;
}

// ===================================
void tenAnh_RGBO( char *tenAnhGoc, char *tenAnhPNG ) {
   
   // ---- chép tên ảnh gốc
   while( (*tenAnhGoc != 0x00) && (*tenAnhGoc != '.') ) {
      *tenAnhPNG = *tenAnhGoc;
      tenAnhPNG++;
      tenAnhGoc++;
   }
   
   // ---- kèm đuôi
   *tenAnhPNG = '.';
   tenAnhPNG++;
   *tenAnhPNG = 'p';
   tenAnhPNG++;
   *tenAnhPNG = 'n';
   tenAnhPNG++;
   *tenAnhPNG = 'g';
   tenAnhPNG++;
   *tenAnhPNG = 0x0;
   tenAnhPNG++;
}


#pragma mark ==== main.c
int main( int argc, char **argv ) {

   if( argc > 1 ) {
      image_data anhPICT = decode_pict( argv[1] );
      
      // ---- tên tập tin
      char tenTep[255];
      tenAnh_RGBO( argv[1], tenTep );
      
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
   
   return 1;
}


// ---- Từ thư viện 
unsigned int nenRLE(unsigned char *dem, int beDaiDem, unsigned char *demNen, int beDaiDemNen ) {
   
   unsigned int beDaiNen = 0;
   
   return beDaiNen;
}



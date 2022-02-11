/* docPICT.h */

#pragma once


/* image data */
typedef struct {
   unsigned short width;    /* width      */
   unsigned short height;   /* height     */
   unsigned short componentCount;  /* component count */

   unsigned char *channel_B;        /* channel b  */
   unsigned char *channel_G;        /* channel g  */
   unsigned char *channel_R;        /* channel r  */
   unsigned char *channel_O;        /* channel o  */
} image_data;


/* đọc tệp PICT */
image_data decode_pict( const char *sfile );

/*
Copyright (c) 2011 Satoru NAKAJIMA

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct RawBitmapData RawBitmapData;

struct RawBitmapData {
  int width, height;
  int components;
  int byteLength;
  unsigned char *bitmap;
};

//utils
static int loadOneLine( FILE *f, char *buf, int maxlen )
{
  int i;
  
  for( i = 0; i < maxlen -1; i++ ){
    int c = fgetc(f);
    if( c == EOF || c == '\n' ){
      break;
    }
    
    *buf = c;
    buf++;
  }
  
  *buf = '\0';
  
  return i;
}

static void put16Bytes( unsigned short s, FILE *f )
{
  fputc( s & 0xff, f );
  fputc( (s >> 8) & 0xff, f );
}

static void put32Bytes( unsigned long l, FILE *f )
{
  fputc( l & 0xff, f );
  fputc( (l >> 8) & 0xff, f );
  fputc( (l >> 16) & 0xff, f );
  fputc( (l >> 24) & 0xff, f );
}

//load
static RawBitmapData* loadPPM( char *filename )
{
  #define _LOAD_BUF_LEN     1024
  static char loadbuf[_LOAD_BUF_LEN];
  
  FILE *ppmfile;
  RawBitmapData *ret = NULL;
  int l;
  char *bufptr;
  int imgw, imgh, depth;
  
  //
  printf("load %s\n", filename);
  
  //open
  ppmfile = fopen(filename, "rb");
  if( ppmfile == NULL ){
    fprintf(stderr, "file %s couldn't open\n", filename);
    goto LOAD_FAIL_RET;
  }
  
  //signature
  l = loadOneLine( ppmfile, loadbuf, _LOAD_BUF_LEN );
  if( l <= 0 ){
    fprintf(stderr, "PPM header couldn't load\n");
    goto LOAD_FAIL_RET;
  }
  
  //printf("PPM signature: %s\n", loadbuf);
  
  if( strcmp(loadbuf, "P6") != 0 ){
    fprintf(stderr, "illegal signature: %s\n", loadbuf);
    goto LOAD_FAIL_RET;
  }
  
  //size
  l = loadOneLine( ppmfile, loadbuf, _LOAD_BUF_LEN );
  if( l <= 0 ){
    fprintf(stderr, "PPM header couldn't load\n");
    goto LOAD_FAIL_RET;
  }
  
  imgw = strtol( loadbuf, &bufptr, 0 );
  imgh = strtol( bufptr, &bufptr, 0 );
  
  //printf("size: \"%s\":(%d,%d)\n", loadbuf, imgw, imgh);
  
  //depth
  l = loadOneLine( ppmfile, loadbuf, _LOAD_BUF_LEN );
  if( l <= 0 ){
    fprintf(stderr, "PPM header couldn't load\n");
    goto LOAD_FAIL_RET;
  }
  
  depth = strtol( loadbuf, &bufptr, 0 );
  
  //printf("pixel depth: \"%s\"(%d)\n", loadbuf, depth);
  
  //load data
  ret = (RawBitmapData*)malloc( sizeof(RawBitmapData) + imgw * imgh * 3 );
  
  if( ret == NULL ){
    fprintf(stderr, "buffer coulcn't allocate\n");
    goto LOAD_FAIL_RET;
  }
  
  ret->width = imgw;
  ret->height = imgh;
  ret->components = 3;
  ret->byteLength = imgw * imgh * 3;
  ret->bitmap = (unsigned char*)(ret + 1);
  
  fread(ret->bitmap, 1, ret->byteLength, ppmfile);
  
LOAD_FAIL_RET:
  if( ppmfile ) fclose(ppmfile);
  return ret;
}

static void saveBMP( RawBitmapData *bmpdat, char *filename )
{
  FILE *bmpfile;
  int ix, iy;
  
  bmpfile = fopen( filename, "wb" );
  if( bmpfile == NULL ){
    fprintf(stderr, "BMP file couldn't open\n");
    return;
  }
  
  //=== BMP header ===
  //signature
  fprintf( bmpfile, "BM" );
  //total length
  put32Bytes( bmpdat->byteLength + 14 + 40, bmpfile );//BMP header = 14, BMP image header = 40
  //reserved
  put16Bytes( 0, bmpfile );
  put16Bytes( 0, bmpfile );
  //offset to image (from file head)
  put32Bytes( 14 + 40, bmpfile );
  
  //=== BMP image header ===
  //header size
  put32Bytes( 40, bmpfile );
  //size
  put32Bytes( bmpdat->width, bmpfile );
  put32Bytes( bmpdat->height, bmpfile );
  //bit planes
  put16Bytes( 1, bmpfile );
  //color bits
  put16Bytes( 24, bmpfile );
  //compression
  put32Bytes( 0, bmpfile );//0:none
  //bitmap size;
  put32Bytes( bmpdat->byteLength, bmpfile );
  //dot per meter
  put32Bytes( 2835, bmpfile );//x (== 72 dpi)
  put32Bytes( 2835, bmpfile );//y (== 72 dpi)
  //color table states
  put32Bytes( 0, bmpfile );//color used
  put32Bytes( 0, bmpfile );//color important
  
  //write data
  for( iy = bmpdat->height - 1; iy >= 0; iy-- ){
    for( ix = 0; ix < bmpdat->width; ix++ ){
      unsigned char *curbuf = bmpdat->bitmap + (ix + iy * bmpdat->width) * 3;
      fputc( curbuf[2], bmpfile );//B
      fputc( curbuf[1], bmpfile );//G
      fputc( curbuf[0], bmpfile );//R
    }
  }
  
  /* //BMP is fliped
  for( i = 0; i < bmpdat->byteLength; i += 3 ){
    fputc( bmpdat->bitmap[i + 2], bmpfile );//B
    fputc( bmpdat->bitmap[i + 1], bmpfile );//G
    fputc( bmpdat->bitmap[i + 0], bmpfile );//R
  }
  */
  //done
  fclose(bmpfile);
  printf("save as %s\n", filename);
}

int main( int argc, char *argv[] )
{
  RawBitmapData *bmpdat;
  
  if( argc <= 1 ){
    fprintf(stderr, "usage: %s file.ppm\n", argv[0]);
    return 0;
  }
  
  bmpdat = loadPPM( argv[1] );
  if( bmpdat != NULL ) {
    sprintf( argv[1] + strlen(argv[1]) - 4, ".bmp" );
    saveBMP(bmpdat, argv[1]);
    free(bmpdat);
  }
  
  return 0;
}

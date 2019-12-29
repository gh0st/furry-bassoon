/**********************************************************************
 *
 *      (c) videantis GmbH
 *
 *	Description:  VC-1 decoder application program
 *
 **********************************************************************/
#include "svg_layer.h"
#include "adit_typedef.h"
#include <string.h>

#ifndef LINUX
#include <tk/tkernel.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <arpa/inet.h>
#define DISPLAY_WIDTH  800
#define DISPLAY_HEIGHT 480

#include "vvc1_dec.h"


//#define YUV420_TEST
//#define TVVC1_OUTPUT_FILE
#define TVVC1_OUTPUT_SCREEN
//#define MEMCOPY_OUTPUT
//#define COMPLIANCE_TEST
#define PERFORMANCE_TEST

#ifdef LINUX
#define FULLPATH(FILENAME) "conf_vc1/" #FILENAME
#else
#define FULLPATH(FILENAME) "/uda1/conf_vc1/" #FILENAME
#endif

#ifdef PERFORMANCE_TEST
#define TVVC1_PERF_CNT_SZ 1048576 //1024

typedef enum {
    TVVC1_START_DECODE = 0x7FFF0000,
    TVVC1_DECODE_RETURNS
} TVVC1_perf_cnt_pos_t;

#ifndef LINUX
typedef struct{
    TVVC1_perf_cnt_pos_t pos;
    UW timestamp;
    UW data;
}TVVC1_perf_cnt_t;

typedef struct{
    U32 index;
    TVVC1_perf_cnt_t perf_cnt[TVVC1_PERF_CNT_SZ];
}TVVC1_Info_t;

LOCAL TVVC1_Info_t TVVC1_info;

LOCAL void TVVC1_add_to_perf_cnt(TVVC1_Info_t *const p_info, TVVC1_perf_cnt_pos_t pos, UW data);
#endif
#endif /* PERFORMANCE_TEST */

int vc1_decoder_app(char* in, unsigned int frames, int output, int yuv422);

#define VOP_SIZE        524288

/*
 * The RCV format contains a type byte. The top bit indicates
 * extension data. We need VC1 type, with extension data.
 */
#define RCV_VC1_TYPE (0x85)
/* Bit 6 of the type indicates V1 if 0, V2 if 1 */
#define RCV_V2_MASK (1 << 6)


unsigned lvop_buffer32[VOP_SIZE/4];
unsigned char  *lvop_buffer;
int lstartcode_parsed;
int lend_of_input;
int setup_complete;
unsigned char* lbitstream_buffer;
unsigned *lCurBitstr;
unsigned lBitstrL, lBitstrR;
unsigned lCurOffset;
unsigned width;
unsigned height;

#ifndef LINUX
LOCAL SYSTIM mytim, my2ndtim;
#endif

#if defined(SYSTEM_PROGRAM)

LOCAL U32 my_atoui(char* in)
{
    U32 i = 0;
    U32 out = 0;

    while((in[i] >= 0x30) && (in[i] <= 0x39))
    {
        if (i!=0)
        {
            out = out * 10;
        }
        out = out + in[i] -0x30;
        i++;
    }
    return out;
}

/* PRQA: Message 3625: It isn't possible to declare to (un-)signed char explicitly */
#pragma PRQA_MESSAGES_OFF 3625
#ifdef LINUX
int main(int ac, char *av[])
#else
EXPORT ER main(int ac, char* av[])
#endif
{
#ifdef PERFORMANCE_TEST
#ifndef LINUX
    /* Initialize Performance Counter 2 */
    UW  results[2];

    /* check Arm Spec for deatils on these settings */
    /* this means that every count equals 16ns (updated with every 64th clock cycle) */
    results[0] = 0x00800001;
    /* This just resets the count to 0*/
    results[1] = 0;
    _SetPerformanceCounter(2, results);
    memset(&TVVC1_info, 0x00, sizeof(TVVC1_info));
#endif
#endif /* PERFORMANCE_TEST */

    if (ac < 0)
    {
#ifndef LINUX
        return E_OK;
#else
        return 0;
#endif
    }
    if (1 == ac)
    {
      vc1_decoder_app("/uda1/fifth_mp_1M_2Mpeak.rcv", 0, 1, 1);
    }
    else if (5 == ac)
    {
        vc1_decoder_app(av[1], my_atoui(av[2]), my_atoui(av[3]), my_atoui(av[4]));
#ifndef PERFORMANCE_TEST
#ifndef LINUX
#if 0
        tk_get_tim(&my2ndtim);
        printf("1st time: %d\n2nd time: %d\ndifference: %d\n", mytim.lo, my2ndtim.lo, my2ndtim.lo - mytim.lo);
#endif
#endif
#endif
    }
    else
    {
#ifdef YUV420_TEST
#define YUV_FLAG 0
#else
#define YUV_FLAG 1
#endif
	vc1_decoder_app(FULLPATH(SSL0013.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SSL0014.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SSL0015.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SSL0016.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SSL0017.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SSL0018.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SSL0019.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SSL0020.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SSL0021.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SSL0022.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SSL0023.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SSL0024.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SSL0025.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SSM0010.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SSM0011.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SSM0012.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SSM0013.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SSM0014.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SSM0015.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SSM0016.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SSM0017.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SSM0018.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SSM0019.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SML0000.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SML0001.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SML0002.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SML0003.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SML0004.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SML0005.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SML0006.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SML0007.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SML0008.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SML0009.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SML0010.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SML0011.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SML0012.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SML0013.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SML0014.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SML0015.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SML0016.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SML0017.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SML0018.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SMM0000.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SMM0001.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SMM0002.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SMM0003.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SMM0004.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SMM0005.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SMM0006.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SMM0007.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SMM0008.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SMM0009.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SMM0010.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SMM0011.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SMM0012.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SMM0013.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SMM0014.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(SMM0015.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(16_9_par_2_1.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(BA_264_main.rcv), 0, 1, YUV_FLAG);
	vc1_decoder_app(FULLPATH(nuhr_short.rcv), 0, 1, YUV_FLAG);
    }

    return 1; //VMP_startup_ssy();
}

#pragma PRQA_MESSAGES_ON 3625



#endif




typedef unsigned long ulong;
typedef unsigned char uchar;

#define leRulong( P, V) {			\
    register ulong v = (uchar) *P++;		\
    v += (ulong) (uchar) *P++ << 8;		\
    v += (ulong) (uchar) *P++ << 16;		\
    (V) = v + ( (ulong) (uchar) *P++ << 24);	\
  }
#define leRulongF( F, V) {			\
    char b[4], *p = b;				\
    fread( (void *) b, 1, 4, F);		\
    leRulong( p, V);				\
  }

#define leRulongB(V) {				\
    char b[4], *p = b;				\
    b[0] = *lbitstream_buffer++;	        \
    b[1] = *lbitstream_buffer++;	       	\
    b[2] = *lbitstream_buffer++;       		\
    b[3] = *lbitstream_buffer++;       		\
    leRulong( p, V);				\
  }



LOCAL int readCompressedFrame(FILE *fp, unsigned char *buffer)
{
  ulong frameSize, timeStamp;
  leRulongF(fp, frameSize);
  frameSize &= 0xffffff;

  leRulongF(fp, timeStamp);
  fread(buffer, 1, frameSize ,fp);

  return (int)frameSize;
}


LOCAL int readCompressedFrameFromBuffer(unsigned char **buffer)
{
  ulong frameSize, timeStamp;
  int i;
  unsigned char *p = *buffer;

  leRulongB(frameSize); 
  frameSize &= 0xffffff;

  leRulongB(timeStamp);

  for (i = 0; i<frameSize; i++) {
    *p++ = *lbitstream_buffer++;
  }

  return (int)frameSize;
}


LOCAL int read_rcv_header(FILE *fp, unsigned char *buffer, int verbose)
{
  unsigned int value;
  int RCVisV2Format;
  unsigned char struct_A[8];
  unsigned char struct_B[12];
  unsigned char struct_C[4];
  unsigned char tmp[4];
  unsigned char *p = buffer;
  int i;
  unsigned char* bitstream_ptr = lbitstream_buffer;

  // read RCV format

  // number of frames
  value = *bitstream_ptr++;
  value |= ((*bitstream_ptr++) << 8);
  value |= ((*bitstream_ptr++) << 16);

  if (verbose)
    printf("Number of frames in sequence: %d\n", value);

  // type and extension flag
  value = *bitstream_ptr++;
  if ((value & ~RCV_V2_MASK) != RCV_VC1_TYPE) {
    return -1;
  }
  RCVisV2Format = ((value & ~RCV_V2_MASK) != 0);

  // size of extension data
  value = *bitstream_ptr++;
  value |= ((*bitstream_ptr++) << 8);
  value |= ((*bitstream_ptr++) << 16);
  value |= ((*bitstream_ptr++) << 24);

  if (value != 4) { // simple, main
    return -1;
  }

  // extension data contains sequence header... (STRUCT C)
  bitstream_ptr++;
  bitstream_ptr++;
  bitstream_ptr++;
  bitstream_ptr++;

  // end of extension data

  // heigth
  height = *bitstream_ptr++;
  height |= ((*bitstream_ptr++) << 8);
  height |= ((*bitstream_ptr++) << 16);
  height |= ((*bitstream_ptr++) << 24);

  // width
  width = *bitstream_ptr++;
  width |= ((*bitstream_ptr++) << 8);
  width |= ((*bitstream_ptr++) << 16);
  width |= ((*bitstream_ptr++) << 24);

  if (RCVisV2Format) {

    // size of 2ns extension data (STRUCT B)
    value = *bitstream_ptr++;
    value |= ((*bitstream_ptr++) << 8);
    value |= ((*bitstream_ptr++) << 16);
    value |= ((*bitstream_ptr++) << 24);

    // parse off extension data
    while (value--) {
      bitstream_ptr++;
    }
  }

  // read sequence header off from file
  fread(tmp, 1, 4, fp);
  fread(tmp, 1, 4, fp);
  fread(struct_C, 1, 4, fp);
  fread(struct_A, 1, 8, fp);
  fread(tmp, 1, 4, fp);
  fread(struct_B, 1, 12, fp);

  // construct alternative sequences header for init function
  *p++ = 44;
  *p++ = 0;
  *p++ = 0;
  *p++ = 0;
  for (i=0; i<4;i++)
    *p++ = struct_A[4+i];
  for (i=0; i<4;i++)
    *p++ = struct_A[i];
  p+=28;
  for (i=0; i<4;i++)
    *p++ = struct_C[i];

  return (int)((unsigned)bitstream_ptr - (unsigned)lbitstream_buffer);
}








//
// Parameters of vc1 decoder test application
//    in	         : path and file name of VC-1 elementary stream file
//    frames             : number of frames to be decoded (0=all available frames in sequence)
//    output             : write frames in display order in specified color format to file (0=no, 1=yes)
//    yuv422             : generate YUV422 output (0=no, 1=yes)
// Return value
//    0	                 : success
//    1	                 : error

int vc1_decoder_app(char* in, unsigned int frames, int output, int yuv422)
{
  VVC1_VID_DEC_INFO vid_dec_info;
#ifdef TVVC1_OUTPUT_FILE
  char output_yuv_filename[120];
#endif
  unsigned input_vc1_file_size;
  unsigned l_bufsize;
  int bytes_consumed;
  int file_in;
  FILE *fd_vc1_file_in;
#ifdef TVVC1_OUTPUT_FILE
  FILE *fd_yuv_file_out;
#endif
  int count;
  int image_format;
  int status;
  unsigned char* lbitstream_buffer_save;
  SVGSurface      *p_svg_surface1 = NULL;
  SVGLayerContext *p_svg_layer    = NULL;

#ifdef MEMCOPY_OUTPUT
  SVGUint32       i				= 0;
  SVGUint32       *pui32Src		= 0;
  SVGUint32       *pui32Dst		= 0;
#endif

  unsigned char* yuv422_buf;

  lvop_buffer = (unsigned char *) &lvop_buffer32[0];

  width = 0;
  height = 0;
  count = 0;

  printf("opening input file: %s\n", in);
  fd_vc1_file_in = fopen(in, "r");
  if (fd_vc1_file_in == NULL) {
    printf("cannot open input file\n");
    return -1;
  }

#ifdef TVVC1_OUTPUT_FILE
  if (output)
  {
    strcpy(output_yuv_filename, in);
    strcat(output_yuv_filename, ".yuv");
#ifndef PERFORMANCE_TEST
    printf("opening output file: %s\n", output_yuv_filename);
#endif
    fd_yuv_file_out = fopen(output_yuv_filename, "w");
    if (fd_yuv_file_out == NULL)
    {
      printf("cannot open output file\n");
    }
  }
#endif
  fseek(fd_vc1_file_in, 0, SEEK_END);
  input_vc1_file_size = ftell(fd_vc1_file_in);
  fseek(fd_vc1_file_in, 0, SEEK_SET);

  if (input_vc1_file_size > 64<<20)
    input_vc1_file_size = 64<<20;

  printf("size of VC-1 elementary stream in bytes: %d\n",input_vc1_file_size);

  lbitstream_buffer = malloc(input_vc1_file_size);
  lbitstream_buffer_save = lbitstream_buffer;

  file_in = open(in, O_RDONLY);
  if (file_in == -1)
  {
    printf("cannot open input file (2)\n");
    return -1;
  }

  read(file_in, lbitstream_buffer, input_vc1_file_size);
  close(file_in);

#ifdef LINUX
  svgInitResourceManager();
#endif

  // parse dimensions at beginning of sequence
  bytes_consumed = read_rcv_header(fd_vc1_file_in, lvop_buffer, 1);
  printf("Size of header: %d\n", bytes_consumed);
  printf("\nparsed width: %d   height: %d\n", width, height);

  // error
  if (bytes_consumed == -1)
    return 1;

  lbitstream_buffer += bytes_consumed;

  l_bufsize = input_vc1_file_size - bytes_consumed;

#ifndef LINUX
  tk_get_tim(&mytim);
#endif

  image_format = (yuv422) ? VVC1_VID_IMAGE_FORMAT_YUV422 : VVC1_VID_IMAGE_FORMAT_YUV420;
  status = VVC1_dec_init(lvop_buffer, 0, &(VMP_fbmem_allocator), image_format);
  if (status != VVC1_VID_STATUS_OK)
  {
    printf("FATAL: Decoder initialization error occured (%d).\nAbort.\n", status);
    return 1;
  }

  /*First the surfaces (memory for drawing)*/
  p_svg_surface1 = svgCreateResourceSurface(width, height, SVG_COLOR_YUV422);
//Debugging
  SVGSurfaceStatus my_surface_status;
  svgGetSurfaceStatus(p_svg_surface1, &my_surface_status);
// finished debugging

  /*create layer context*/
  svgSetLayerDispmodeAndTiming(DISP_NORMAL_T1);
  p_svg_layer    = svgCreateLayerContext(p_svg_surface1, p_svg_surface1);
#if 1
#ifndef LINUX
  memset(p_svg_surface1->framebuffer, 0x00, p_svg_surface1->strideSize * p_svg_surface1->sizeY);
#endif
#endif
  /*make layer visible*/
  svgSetLayerViewport (p_svg_layer, 0, 0, (DISPLAY_WIDTH-width)>>1, (DISPLAY_HEIGHT-height)>>1, width, height);
  svgSetLayerOrder (p_svg_layer, 0);
#ifndef LINUX
  svgEnableLayer(p_svg_layer);
#endif
  svgApplyLayerInSync(SVG_PC_DISPLAY_DEFAULT);


  yuv422_buf = malloc(width*height*2);

  vid_dec_info.status = VVC1_VID_STATUS_FRAME_NOT_FINISHED;

  while (l_bufsize)
    {
      bytes_consumed = readCompressedFrameFromBuffer(&lvop_buffer);
      l_bufsize-= (bytes_consumed+8);

      while (bytes_consumed < 8) {
	bytes_consumed = readCompressedFrameFromBuffer(&lvop_buffer);
	l_bufsize-= (bytes_consumed+8);
      }

#ifdef PERFORMANCE_TEST
#ifndef LINUX
      TVVC1_add_to_perf_cnt(&TVVC1_info, TVVC1_START_DECODE, 0);
#endif
#endif /* PERFORMANCE_TEST */
      VVC1_dec_frame(lvop_buffer, bytes_consumed, 0, 0, (yuv422 ? &vid_dec_info.image_1 : &vid_dec_info.image_0), (vid_dec_info.status == VVC1_VID_STATUS_OK), &vid_dec_info);
#ifdef PERFORMANCE_TEST
#ifndef LINUX
      TVVC1_add_to_perf_cnt(&TVVC1_info, TVVC1_DECODE_RETURNS, vid_dec_info.error_resilience_status);
#endif
#endif /* PERFORMANCE_TEST */

    if (vid_dec_info.status == VVC1_VID_STATUS_ERROR_NOBUFFER) 
    {
	printf("FATAL: No display buffer available.\nAbort.\n");
	break;	
    }
    else
    {
	if (vid_dec_info.status < VVC1_VID_STATUS_OK)
	{
	    printf("FATAL: Decoder error occured (%d).\nAbort.\n", vid_dec_info.status);
	}
	if (vid_dec_info.error_resilience_status != VVC1_VID_STATUS_OK)
	{
	    printf("INFO: Decoding error occured.\n");
	}
    }

      if (vid_dec_info.status == VVC1_VID_STATUS_OK)
	count++;

      if (output && (vid_dec_info.status == VVC1_VID_STATUS_OK))
	{
	  if (yuv422)
	    {
#ifdef TVVC1_OUTPUT_FILE
	      {
		SVGUint32       i	       		= 0;
		SVGUint32       *pui32Src		= 0;
		U8              *lyuv422_buf		= yuv422_buf;

		pui32Src =  (unsigned int)vid_dec_info.image_1->virt_addr;

		for (i = 0; i < vid_dec_info.y_dim_1; i++)
		  {
		    memcpy((U32*)lyuv422_buf, pui32Src, vid_dec_info.x_dim_1*2);
		    pui32Src = (U8*)pui32Src + vid_dec_info.image_1->strideSize;
		    lyuv422_buf = lyuv422_buf + (vid_dec_info.x_dim_1 * 2);
		  }

		fwrite(yuv422_buf, vid_dec_info.x_dim_1*vid_dec_info.y_dim_1*2, 1, fd_yuv_file_out);
	      }
#endif
#ifdef TVVC1_OUTPUT_SCREEN
#ifdef MEMCOPY_OUTPUT
	      pui32Dst =  p_svg_surface1->framebuffer;
	      pui32Src =  (unsigned int)vid_dec_info.image_1->virt_addr;

	      for (i = 0; i < ((height < DISPLAY_HEIGHT)? height : DISPLAY_HEIGHT); i++)
		{

		  memcpy(pui32Dst, pui32Src, vid_dec_info.x_dim_1*2);
		  pui32Dst = (U8*)pui32Dst + p_svg_surface1->strideSize;
		  pui32Src = (U8*)pui32Src + (vid_dec_info.x_dim_1 * 2);

		}
#else
#ifdef LINUX
		  svgEnableLayer(p_svg_layer);
#endif
	      svgSetLayerSurfaces(p_svg_layer,vid_dec_info.image_1->svg_surface,vid_dec_info.image_1->svg_surface);
	      svgSetLayerViewport (p_svg_layer, 0, 0, (DISPLAY_WIDTH > width) ? (DISPLAY_WIDTH-width)>>1 : 0, (DISPLAY_HEIGHT > height) ? (DISPLAY_HEIGHT-height)>>1 : 0, width, height);
	      svgApplyLayerInSync(SVG_PC_DISPLAY_DEFAULT);
#ifndef PERFORMANCE_TEST
	      svgWaitLayerVSync(SVG_PC_DISPLAY_DEFAULT);
#endif /* PERFORMANCE_TEST */
#endif /* MEMCOPY_OUTPUT*/
#endif /* TVVC1_OUTPUT_SCREEN */
	    }
	  else
	    {
#ifdef TVVC1_OUTPUT_FILE
	      fwrite((unsigned char *)vid_dec_info.image_0->virt_addr, (vid_dec_info.x_dim_0*vid_dec_info.y_dim_0*3)>>1, 1, fd_yuv_file_out);
#endif
	    }
	}

#ifdef COMPLIANCE_TEST
      if (vid_dec_info.status == VVC1_VID_STATUS_OK)
	{
	  printf("Frame %d of %d\n", count, frames);
	}
#else
      if (vid_dec_info.status == VVC1_VID_STATUS_OK)
	{
	  if ((count%100) == 0)
	    printf("Frame %d\n", count);
	}
#endif

      if (count && (count == frames))
	break;
    }


#ifndef LINUX
  tk_get_tim(&my2ndtim);
  printf("1st time: %d\n2nd time: %d\ndifference: %d\n", mytim.lo, my2ndtim.lo, my2ndtim.lo - mytim.lo);
#endif


#if 1 // force output of last two frames
  if (vid_dec_info.status >= VVC1_VID_STATUS_OK)
  {
    int count_init = count;
    do {
      fseek(fd_vc1_file_in, 0, SEEK_SET);
      lbitstream_buffer = lbitstream_buffer_save;
      bytes_consumed = read_rcv_header(fd_vc1_file_in, lvop_buffer, 0);
      bytes_consumed = readCompressedFrame(fd_vc1_file_in, lvop_buffer);
      while (bytes_consumed < 8) {
	bytes_consumed = readCompressedFrame(fd_vc1_file_in, lvop_buffer);
      }

#ifdef PERFORMANCE_TEST
#ifndef LINUX
      TVVC1_add_to_perf_cnt(&TVVC1_info, TVVC1_START_DECODE, 0);
#endif
#endif /* PERFORMANCE_TEST */
      VVC1_dec_frame(lvop_buffer, bytes_consumed, 0, 0, (yuv422 ? &vid_dec_info.image_1 : &vid_dec_info.image_0), (vid_dec_info.status == VVC1_VID_STATUS_OK), &vid_dec_info);
#ifdef PERFORMANCE_TEST
#ifndef LINUX
      TVVC1_add_to_perf_cnt(&TVVC1_info, TVVC1_DECODE_RETURNS, vid_dec_info.error_resilience_status);
#endif
#endif /* PERFORMANCE_TEST */

      if (vid_dec_info.status == VVC1_VID_STATUS_ERROR_NOBUFFER) 
      {
	  printf("FATAL: No display buffer available.\nAbort.\n");
	  break;	
      }
      else
      {
	  if (vid_dec_info.status < VVC1_VID_STATUS_OK)
	  {
	      printf("FATAL: Decoder error occured (%d).\nAbort.\n", vid_dec_info.status);
	  }
	  if (vid_dec_info.error_resilience_status != VVC1_VID_STATUS_OK)
	  {
	      printf("INFO: Decoding error occured.\n");
	  }
      }

      if (vid_dec_info.status == VVC1_VID_STATUS_OK)
	count++;

      if (output && (vid_dec_info.status == VVC1_VID_STATUS_OK))
	{
	  if (yuv422)
	    {
#ifdef TVVC1_OUTPUT_FILE
	      {
		SVGUint32       i	       		= 0;
		SVGUint32       *pui32Src		= 0;
		U8              *lyuv422_buf		= yuv422_buf;

		pui32Src =  (unsigned int)vid_dec_info.image_1->virt_addr;

		for (i = 0; i < vid_dec_info.y_dim_1; i++)
		  {
		    memcpy((U32*)lyuv422_buf, pui32Src, vid_dec_info.x_dim_1*2);
		    pui32Src = (U8*)pui32Src + vid_dec_info.image_1->strideSize;
		    lyuv422_buf = lyuv422_buf + (vid_dec_info.x_dim_1 * 2);
		  }

		fwrite(yuv422_buf, vid_dec_info.x_dim_1*vid_dec_info.y_dim_1*2, 1, fd_yuv_file_out);
	      }
#endif
#ifdef TVVC1_OUTPUT_SCREEN
#ifdef MEMCOPY_OUTPUT
	      pui32Dst =  p_svg_surface1->framebuffer;
	      pui32Src =  (unsigned int)vid_dec_info.image_1->virt_addr;

	      for (i = 0; i < ((height < DISPLAY_HEIGHT)? height : DISPLAY_HEIGHT); i++)
		{

		  memcpy(pui32Dst, pui32Src, vid_dec_info.x_dim_1*2);
		  pui32Dst = (U8*)pui32Dst + p_svg_surface1->strideSize;
		  pui32Src = (U8*)pui32Src + (vid_dec_info.x_dim_1 * 2);

		}
#else
	      svgSetLayerSurfaces(p_svg_layer,vid_dec_info.image_1->svg_surface,vid_dec_info.image_1->svg_surface);
	      svgSetLayerViewport (p_svg_layer, 0, 0, (DISPLAY_WIDTH > width) ? (DISPLAY_WIDTH-width)>>1 : 0, (DISPLAY_HEIGHT > height) ? (DISPLAY_HEIGHT-height)>>1 : 0, width, height);
	      svgApplyLayerInSync(SVG_PC_DISPLAY_DEFAULT);
#ifndef PERFORMANCE_TEST
	      svgWaitLayerVSync(SVG_PC_DISPLAY_DEFAULT);
#endif /* PERFORMANCE_TEST */
#endif /* MEMCOPY_OUTPUT*/
#endif /* TVVC1_OUTPUT_SCREEN */
	    }
	  else
	    {
#ifdef TVVC1_OUTPUT_FILE
	      fwrite((unsigned char *)vid_dec_info.image_0->virt_addr, (vid_dec_info.x_dim_0*vid_dec_info.y_dim_0*3)>>1, 1, fd_yuv_file_out);
#endif
	    }
	}

    } while(count < count_init+2);
  }
#endif // ~force output of last two frames


  VVC1_dec_finish(&(VMP_fbmem_destructor));


  free(yuv422_buf);

  free(lbitstream_buffer_save);

  svgDisableLayer(p_svg_layer);
  svgApplyLayerInSync(SVG_PC_DISPLAY_DEFAULT);
  svgDestroyLayerContext(p_svg_layer);
  svgDestroyResourceSurface(p_svg_surface1);

  if (output) {
#ifdef TVVC1_OUTPUT_FILE
    fclose(fd_yuv_file_out);
#endif
  }

  fclose(fd_vc1_file_in);

  return 0;
}



#ifdef PERFORMANCE_TEST
#ifndef LINUX
LOCAL void TVVC1_add_to_perf_cnt(TVVC1_Info_t *const p_info,
                         TVVC1_perf_cnt_pos_t pos, UW data)
{
    UW results[2];
    UW timestamp;
    UW ind;
    /* timestamp = bios_mpcoreip_cp15_getPMCount0(); */
    _GetPerformanceCounter(1+1,results);
    timestamp = results[1];

    ind = p_info->index;
    p_info->perf_cnt[ind].pos = pos;

    p_info->perf_cnt[ind].data = data;
    p_info->perf_cnt[ind].timestamp = timestamp;

    ind++;
    if ( ind >= TVVC1_PERF_CNT_SZ )
    {
        ind = 0;
    }

    p_info->index = ind;
}
#endif

#endif /* PERFORMANCE_TEST */




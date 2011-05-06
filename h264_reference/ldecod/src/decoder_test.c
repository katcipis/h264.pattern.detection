
/*!
 ***********************************************************************
 *  \file
 *     decoder_test.c
 *  \brief
 *     H.264/AVC decoder test 
 *  \author
 *     Main contributors (see contributors.h for copyright, address and affiliation details)
 *     - Yuwen He       <yhe@dolby.com>
 ***********************************************************************
 */

#include "contributors.h"

#include <sys/stat.h>
#include <math.h>

//#include "global.h"
#include "h264decoder.h"
#include "configfile.h"

#define DECOUTPUT_TEST      0

#define PRINT_OUTPUT_POC    0
#define BITSTREAM_FILENAME  "test.264"
#define DECRECON_FILENAME   "test_dec.yuv"
#define ENCRECON_FILENAME   "test_rec.yuv"
#define DECOUTPUT_VIEW0_FILENAME  "H264_Decoder_Output_View0.yuv"
#define DECOUTPUT_VIEW1_FILENAME  "H264_Decoder_Output_View1.yuv"


static void Configure(InputParameters *p_Inp, int ac, char *av[])
{
  //char *config_filename=NULL;
  //char errortext[ET_SIZE];
  memset(p_Inp, 0, sizeof(InputParameters));
  strcpy(p_Inp->infile, BITSTREAM_FILENAME); //! set default bitstream name
  strcpy(p_Inp->outfile, DECRECON_FILENAME); //! set default output file name
  strcpy(p_Inp->reffile, ENCRECON_FILENAME); //! set default reference file name
  
  p_Inp->FileFormat = PAR_OF_ANNEXB;
  p_Inp->ref_offset=0;
  p_Inp->poc_scale=2;
  p_Inp->silent = FALSE;
  p_Inp->intra_profile_deblocking = 0;

#ifdef _LEAKYBUCKET_
  p_Inp->R_decoder=500000;          //! Decoder rate
  p_Inp->B_decoder=104000;          //! Decoder buffer size
  p_Inp->F_decoder=73000;           //! Decoder initial delay
  strcpy(p_Inp->LeakyBucketParamFile,"leakybucketparam.cfg");    // file where Leaky Bucket parameters (computed by encoder) are stored
#endif
  p_Inp->iDecFrmNum = 0;

  p_Inp->write_uv=1;
  // picture error concealment
  p_Inp->conceal_mode = 0;
  p_Inp->ref_poc_gap = 2;
  p_Inp->poc_gap = 2;

  ParseCommand(p_Inp, ac, av);

  fprintf(stdout,"----------------------------- JM %s %s -----------------------------\n", VERSION, EXT_VERSION);
  //fprintf(stdout," Decoder config file                    : %s \n",config_filename);
  if(!p_Inp->bDisplayDecParams)
  {
    fprintf(stdout,"--------------------------------------------------------------------------\n");
    fprintf(stdout," Input H.264 bitstream                  : %s \n",p_Inp->infile);
    fprintf(stdout," Output decoded YUV                     : %s \n",p_Inp->outfile);
    //fprintf(stdout," Output status file                     : %s \n",LOGFILE);
    fprintf(stdout," Input reference file                   : %s \n",p_Inp->reffile);

    fprintf(stdout,"--------------------------------------------------------------------------\n");
  #ifdef _LEAKYBUCKET_
    fprintf(stdout," Rate_decoder        : %8ld \n",p_Inp->R_decoder);
    fprintf(stdout," B_decoder           : %8ld \n",p_Inp->B_decoder);
    fprintf(stdout," F_decoder           : %8ld \n",p_Inp->F_decoder);
    fprintf(stdout," LeakyBucketParamFile: %s \n",p_Inp->LeakyBucketParamFile); // Leaky Bucket Param file
    calc_buffer(p_Inp);
    fprintf(stdout,"--------------------------------------------------------------------------\n");
  #endif
  }

  if (!p_Inp->silent)
  {
    fprintf(stdout,"POC must = frame# or field# for SNRs to be correct\n");
    fprintf(stdout,"--------------------------------------------------------------------------\n");
    fprintf(stdout,"  Frame          POC  Pic#   QP    SnrY     SnrU     SnrV   Y:U:V Time(ms)\n");
    fprintf(stdout,"--------------------------------------------------------------------------\n");
  }
  
}

/*********************************************************
if bOutputAllFrames is 1, then output all valid frames to file onetime; 
else output the first valid frame and move the buffer to the end of list;
*********************************************************/
static int WriteOneFrame(DecodedPicList *pDecPic, int hFileOutput0, int hFileOutput1, int bOutputAllFrames)
{
  int iOutputFrame=0;
  DecodedPicList *pPic = pDecPic;

  if(pPic && (((pPic->iYUVStorageFormat==2) && pPic->bValid==3) || ((pPic->iYUVStorageFormat!=2) && pPic->bValid==1)) )
  {
    int i, iWidth, iHeight, iStride, iWidthUV, iHeightUV, iStrideUV;
    byte *pbBuf;    
    int hFileOutput;

    iWidth = pPic->iWidth*((pPic->iBitDepth+7)>>3);
    iHeight = pPic->iHeight;
    iStride = pPic->iYBufStride;
    if(pPic->iYUVFormat != YUV444)
      iWidthUV = pPic->iWidth>>1;
    else
      iWidthUV = pPic->iWidth;
    if(pPic->iYUVFormat == YUV420)
      iHeightUV = pPic->iHeight>>1;
    else
      iHeightUV = pPic->iHeight;
    iWidthUV *= ((pPic->iBitDepth+7)>>3);
    iStrideUV = pPic->iUVBufStride;
   
    do
    {
      printf("KMLO DO MAL \n");

      if(pPic->iYUVStorageFormat==2)
        hFileOutput = (pPic->iViewId&0xffff)? hFileOutput1 : hFileOutput0;
      else
        hFileOutput = hFileOutput0;

      printf("KMLO hFileOutput[%d]\n", hFileOutput);

      if(hFileOutput >=0)
      {
        //Y;
        pbBuf = pPic->pY;

        printf("KMLO 1: writing[%d] from[%d] to[%d]\n", iHeight, 0, iWidth);
        for(i=0; i < iHeight; i++) {
          memset(pbBuf+i*iStride, 0, iWidth);
        }

        for(i=0; i<iHeight; i++)
          write(hFileOutput, pbBuf+i*iStride, iWidth);

        if(pPic->iYUVFormat != YUV400)
        {
         //U;
         pbBuf = pPic->pU;
         for(i=0; i<iHeightUV; i++)
          write(hFileOutput, pbBuf+i*iStrideUV, iWidthUV);
         //V;
         pbBuf = pPic->pV;
         for(i=0; i<iHeightUV; i++)
          write(hFileOutput, pbBuf+i*iStrideUV, iWidthUV);
        }

        iOutputFrame++;
      }

      if((pPic->iYUVStorageFormat==2))
      {
        hFileOutput = ((pPic->iViewId>>16)&0xffff)? hFileOutput1 : hFileOutput0;
        if(hFileOutput>=0)
        {
          int iPicSize =iHeight*iStride;
          //Y;
          pbBuf = pPic->pY+iPicSize;

          printf("KMLO 2: writing[%d] from[%d] to[%d]\n", iHeight, 0, iWidth);
          for(i=0; i < iHeight; i++) {
            memset(pbBuf+i*iStride, 0, iWidth);
          }

          for(i=0; i<iHeight; i++)
            write(hFileOutput, pbBuf+i*iStride, iWidth);

          if(pPic->iYUVFormat != YUV400)
          {
           iPicSize = iHeightUV*iStrideUV;
           //U;
           pbBuf = pPic->pU+iPicSize;
           for(i=0; i<iHeightUV; i++)
            write(hFileOutput, pbBuf+i*iStrideUV, iWidthUV);
           //V;
           pbBuf = pPic->pV+iPicSize;
           for(i=0; i<iHeightUV; i++)
            write(hFileOutput, pbBuf+i*iStrideUV, iWidthUV);
          }

          iOutputFrame++;
        }
      }

#if PRINT_OUTPUT_POC
      fprintf(stdout, "\nOutput frame: %d/%d\n", pPic->iPOC, pPic->iViewId);
#endif
      pPic->bValid = 0;
      pPic = pPic->pNext;
    }while(pPic != NULL && pPic->bValid && bOutputAllFrames);
  }
#if PRINT_OUTPUT_POC
  else
    fprintf(stdout, "\nNone frame output\n");
#endif

  return iOutputFrame;
}

/* KATCIPIS - added to do the bounding box drawing */
/*!
 ***********************************************************************
 * \brief
 *    Draws a bouding box on the frame.
 ***********************************************************************
 */
static void decoder_draw_bounding_box(ExtractedMetadata * metadata, DecodedPicList *pPic)
{
  ExtractedObjectBoundingBox * bounding_box = extracted_object_bounding_box_from_metadata(metadata);
  int box_x                                 = 0;
  int box_y                                 = 0;
  int box_width                             = 0;
  int box_height                            = 0;  
 
  if (!bounding_box) {
    return;
  }

  extracted_object_bounding_box_get_data(bounding_box, NULL, &box_x, &box_y, &box_width, &box_height);

  if(pPic && (pPic->bValid == 3 || pPic->bValid == 1)) {

    int i, iWidth, iHeight, iStride, iWidthUV, iHeightUV, iStrideUV;
    byte *pbBuf;    

    iWidth  = pPic->iWidth*((pPic->iBitDepth+7)>>3);
    iHeight = pPic->iHeight;
    iStride = pPic->iYBufStride;

    if(pPic->iYUVFormat != YUV444) {
      iWidthUV = pPic->iWidth>>1;
    } else {
      iWidthUV = pPic->iWidth;
    }

    if(pPic->iYUVFormat == YUV420) {
      iHeightUV = pPic->iHeight>>1;
    } else {
      iHeightUV = pPic->iHeight;
    }

    iWidthUV *= ((pPic->iBitDepth+7)>>3);
    iStrideUV = pPic->iUVBufStride;
    
    /* lets validate our bounding box */
    if (box_width > iWidth || box_height > iHeight) {
      printf("decoder_draw_bounding_box: ERROR: bounding box has "
             "width[%d] height[%d] and the frame has width[%d] height[%d]!\n", 
             box_width, box_height, iWidth, iHeight);
      return;
    }
   
    /* We want a red bouding box. */
 
    //Y;
    pbBuf = pPic->pY;
    /*
    printf("writing[%d] from[%d] to[%d]\n", iWidth, box_y, box_y + box_height);
    for(i=box_y; i < box_y + box_height; i++) {
      memset(pbBuf+i*iStride, 0, iWidth);
    } */

    printf("writing[%d] from[%d] to[%d]\n", iHeight, 0, iWidth);
    for(i=0; i < iHeight; i++) {
      memset(pbBuf+i*iStride, 0, iWidth);
    }

    #if 0
    if(pPic->iYUVFormat != YUV400) {
      /* lets calculate the bounding box to the chroma components */
      float ratio       = (float) iWidth / (float) iWidthUV;
      int box_uv_x      = floor(box_x / ratio);
      int box_uv_y      = floor(box_y / ratio);
      int box_uv_height = floor(box_height / ratio);
      int box_uv_width  = floor(box_width / ratio);

      //U;
      pbBuf = pPic->pU;
      for(i=0; i<iHeightUV; i++) {
        write(hFileOutput, pbBuf+i*iStrideUV, iWidthUV);
      }
      //V;
      pbBuf = pPic->pV;
      for(i=0; i<iHeightUV; i++) {
        write(hFileOutput, pbBuf+i*iStrideUV, iWidthUV);
      }

    }
    #endif    
  }
}


/*!
 ***********************************************************************
 * \brief
 *    main function for JM decoder
 ***********************************************************************
 */
int main(int argc, char **argv)
{
  int iRet;
  DecodedPicList *pDecPicList;
  int hFileDecOutput0=-1, hFileDecOutput1=-1;
  int iFramesOutput=0, iFramesDecoded=0;
  InputParameters InputParams;
  ExtractedMetadataBuffer * metadata_buffer = NULL;

#if DECOUTPUT_TEST
  hFileDecOutput0 = open(DECOUTPUT_VIEW0_FILENAME, OPENFLAGS_WRITE, OPEN_PERMISSIONS);
  fprintf(stdout, "Decoder output view0: %s\n", DECOUTPUT_VIEW0_FILENAME);
  hFileDecOutput1 = open(DECOUTPUT_VIEW1_FILENAME, OPENFLAGS_WRITE, OPEN_PERMISSIONS);
  fprintf(stdout, "Decoder output view1: %s\n", DECOUTPUT_VIEW1_FILENAME);
#endif

  //get input parameters;
  Configure(&InputParams, argc, argv);
  //open decoder;

  /* KATCIPIS create the metadata buffer */
  metadata_buffer = extracted_metadata_buffer_new();

  iRet = OpenDecoder(&InputParams, metadata_buffer);
  if(iRet != DEC_OPEN_NOERR)
  {
    fprintf(stderr, "Open encoder failed: 0x%x!\n", iRet);
    return -1; //failed;
  }

  //decoding;
  do
  {
    iRet = DecodeOneFrame(&pDecPicList);
    if(iRet==DEC_EOS || iRet==DEC_SUCCEED)
    {
      //process the decoded picture, output or display;
  
     /* KATCIPIS - This seems the best place to do some process on the decoded frame, right before it is written on the file. */
     ExtractedMetadata * metadata = extracted_metadata_buffer_get(metadata_buffer, iFramesDecoded);

     if (metadata) {
       /* Lets process and free the metadata relative to the current frame */
       //decoder_draw_bounding_box(metadata, pDecPicList);
       extracted_metadata_save(metadata, 1);
       extracted_metadata_free(metadata);
     
       /* Next frame does not have a metadata yet */
       metadata = NULL;
     }
     /* KATCIPIS - end of metadata processing on the decoded frame. */ 
  
      iFramesOutput += WriteOneFrame(pDecPicList, hFileDecOutput0, hFileDecOutput1, 0);
      iFramesDecoded++;
    }
    else
    {
      //error handling;
      fprintf(stderr, "Error in decoding process: 0x%x\n", iRet);
    }
  }while((iRet == DEC_SUCCEED) && ((p_Dec->p_Inp->iDecFrmNum==0) || (iFramesDecoded<p_Dec->p_Inp->iDecFrmNum)));

  iRet = FinitDecoder(&pDecPicList);

  iFramesOutput += WriteOneFrame(pDecPicList, hFileDecOutput0, hFileDecOutput1 , 1);
  iRet = CloseDecoder();

  //quit;
  if(hFileDecOutput0>=0)
  {
    close(hFileDecOutput0);
  }
  if(hFileDecOutput1>=0)
  {
    close(hFileDecOutput1);
  }

  /* KATCIPIS free the metadata buffer */
  extracted_metadata_buffer_free(metadata_buffer);

  //printf("%d frames are decoded.\n", iFramesDecoded);
  //printf("%d frames are decoded, %d frames output.\n", iFramesDecoded, iFramesOutput);
  return 0;
}



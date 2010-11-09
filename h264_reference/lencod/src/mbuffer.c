
/*!
 ***********************************************************************
 *  \file
 *      mbuffer.c
 *
 *  \brief
 *      Frame buffer functions
 *
 *  \author
 *      Main contributors (see contributors.h for copyright, address and affiliation details)
 *      - Karsten S�hring                 <suehring@hhi.de>
 *      - Alexis Tourapis                 <alexismt@ieee.org>
 ***********************************************************************
 */

#include <limits.h>

#include "global.h"
#include "mbuffer.h"
#include "memalloc.h"
#include "output.h"
#include "image.h"
#include "nalucommon.h"
#include "img_luma.h"
#include "img_chroma.h"
#include "errdo.h"

extern void SbSMuxBasic(ImageData *imgOut, ImageData *imgIn0, ImageData *imgIn1, int offset);
extern void init_stats                   (InputParameters *p_Inp, StatParameters *stats);
static void insert_picture_in_dpb        (VideoParameters *p_Vid, FrameStore* fs, StorablePicture* p);
static void output_one_frame_from_dpb    (DecodedPictureBuffer *p_Dpb, FrameFormat *output);
static void get_smallest_poc             (DecodedPictureBuffer *p_Dpb, int *poc,int * pos);
static void gen_field_ref_ids            (StorablePicture *p);
static int  is_used_for_reference        (FrameStore* fs);
static int  remove_unused_frame_from_dpb (DecodedPictureBuffer *p_Dpb);
static int  flush_unused_frame_from_dpb  (DecodedPictureBuffer *p_Dpb);
static int  is_short_term_reference      (FrameStore* fs);
static int  is_long_term_reference       (FrameStore* fs);


#define MAX_LIST_SIZE 33

//Zhifeng 090616
extern void free_mem2Duint16(uint16 **array2D);
extern void free_mem3Duint16(uint16 ***array3D);

/*!
 ************************************************************************
 * \brief
 *    Print out list of pictures in DPB. Used for debug purposes.
 ************************************************************************
 */
void dump_dpb(DecodedPictureBuffer *p_Dpb)
{
#if DUMP_DPB
  unsigned i;

  for (i=0; i<p_Dpb->used_size;i++)
  {
    printf("(");
    printf("fn=%d  ", p_Dpb->fs[i]->frame_num);
    if (p_Dpb->fs[i]->is_used & 1)
    {
      if (p_Dpb->fs[i]->top_field)
        printf("T: poc=%d  ", p_Dpb->fs[i]->top_field->poc);
      else
        printf("T: poc=%d  ", p_Dpb->fs[i]->frame->top_poc);
    }
    if (p_Dpb->fs[i]->is_used & 2)
    {
      if (p_Dpb->fs[i]->bottom_field)
        printf("B: poc=%d  ", p_Dpb->fs[i]->bottom_field->poc);
      else
        printf("B: poc=%d  ", p_Dpb->fs[i]->frame->bottom_poc);
    }
    if (p_Dpb->fs[i]->is_used == 3)
      printf("F: poc=%d  ", p_Dpb->fs[i]->frame->poc);
    printf("G: poc=%d)  ", p_Dpb->fs[i]->poc);
    if (p_Dpb->fs[i]->is_reference) printf ("ref (%d) ", p_Dpb->fs[i]->is_reference);
    if (p_Dpb->fs[i]->is_long_term) printf ("lt_ref (%d) ", p_Dpb->fs[i]->is_reference);
    if (p_Dpb->fs[i]->is_output) printf ("out  ");
    if (p_Dpb->fs[i]->is_used == 3)
    {
      if (p_Dpb->fs[i]->frame->non_existing) printf ("ne  ");
    }
#if (MVC_EXTENSION_ENABLE)
    if(p_Dpb->p_Inp->num_of_views==2)
      printf ("view_id=%d ", p_Dpb->fs[i]->view_id);
#endif
    printf ("\n");
  }
#endif
}

/*!
 ************************************************************************
 * \brief
 *    Returns the size of the dpb depending on level and picture size
 *
 *
 ************************************************************************
 */
int getDpbSize(seq_parameter_set_rbsp_t *active_sps)
{
  int pic_size = (active_sps->pic_width_in_mbs_minus1 + 1) * (active_sps->pic_height_in_map_units_minus1 + 1) * (active_sps->frame_mbs_only_flag?1:2) * 384;

  int size = 0;

  switch (active_sps->level_idc)
  {
  case 9:
    size = 152064;
    break;
  case 10:
    size = 152064;
    break;
  case 11:
    if (!IS_FREXT_PROFILE(active_sps->profile_idc) && (active_sps->constrained_set3_flag == 1))
      size = 152064;
    else
      size = 345600;
    break;
  case 12:
    size = 912384;
    break;
  case 13:
    size = 912384;
    break;
  case 20:
    size = 912384;
    break;
  case 21:
    size = 1824768;
    break;
  case 22:
    size = 3110400;
    break;
  case 30:
    size = 3110400;
    break;
  case 31:
    size = 6912000;
    break;
  case 32:
    size = 7864320;
    break;
  case 40:
    size = 12582912;
    break;
  case 41:
    size = 12582912;
    break;
  case 42:
    size = 13369344;
    break;
  case 50:
    size = 42393600;
    break;
  case 51:
    size = 70778880;
    break;
  default:
    error ("undefined level", 500);
    break;
  }

  size /= pic_size;
  size = imin( size, 16);

  if (active_sps->vui_parameters_present_flag && active_sps->vui_seq_parameters.bitstream_restriction_flag)
  {
    if ((int)active_sps->vui_seq_parameters.max_dec_frame_buffering > size)
    {
      error ("max_dec_frame_buffering larger than MaxDpbSize", 500);
    }
    size = imax (1, active_sps->vui_seq_parameters.max_dec_frame_buffering);
  }

  return size;
}

/*!
 ************************************************************************
 * \brief
 *    Check then number of frames marked "used for reference" and break
 *    if maximum is exceeded
 *
 ************************************************************************
 */
void check_num_ref(DecodedPictureBuffer *p_Dpb)
{
  //printf("%d  %d  %d\n", p_Dpb->ltref_frames_in_buffer, p_Dpb->ref_frames_in_buffer, p_Dpb->num_ref_frames);
  if ((int)(p_Dpb->ltref_frames_in_buffer +  p_Dpb->ref_frames_in_buffer ) > (imax(1, p_Dpb->num_ref_frames)))
  {
    error ("Max. number of reference frames exceeded. Invalid stream.", 500);
  }
}


/*!
 ************************************************************************
 * \brief
 *    Allocate memory for decoded picture buffer and initialize with sane values.
 *
 ************************************************************************
 */
void init_dpb(VideoParameters *p_Vid, DecodedPictureBuffer *p_Dpb)
{
  unsigned i;

  p_Dpb->p_Vid = p_Vid;
  p_Dpb->p_Inp = p_Vid->p_Inp;
  p_Dpb->num_ref_frames = p_Vid->num_ref_frames;

  if (p_Dpb->init_done)
  {
    free_dpb(p_Dpb);
  }

  p_Dpb->size = getDpbSize(p_Vid->active_sps);

#if (MVC_EXTENSION_ENABLE)
  if(p_Vid->p_Inp->num_of_views==2)
  {
    if (p_Vid->p_Inp->output.width[0]>1900 && p_Vid->p_Inp->output.height[0]>1000 && p_Vid->p_Inp->PicInterlace!=0)
    {
      p_Dpb->size = p_Dpb->size + 2;
    }
    else
    {
      p_Dpb->size = (p_Dpb->size<<1) + 2;
    }
  }
#endif

  if (p_Dpb->size < (unsigned int) (p_Vid->p_Inp)->num_ref_frames)
  {
    error ("DPB size at specified level is smaller than the specified number of reference frames. This is not allowed.\n", 1000);
  }

  p_Dpb->used_size = 0;
  p_Dpb->last_picture = NULL;

  p_Dpb->ref_frames_in_buffer = 0;
  p_Dpb->ltref_frames_in_buffer = 0;

  p_Dpb->fs = calloc(p_Dpb->size, sizeof (FrameStore*));
  if (NULL==p_Dpb->fs)
    no_mem_exit("init_dpb: p_Dpb->fs");

  p_Dpb->fs_ref = calloc(p_Dpb->size, sizeof (FrameStore*));
  if (NULL==p_Dpb->fs_ref)
    no_mem_exit("init_dpb: p_Dpb->fs_ref");

  p_Dpb->fs_ltref = calloc(p_Dpb->size, sizeof (FrameStore*));
  if (NULL==p_Dpb->fs_ltref)
    no_mem_exit("init_pb: p_Dpb->fs_ltref");

  for (i = 0; i < p_Dpb->size; i++)
  {
    p_Dpb->fs[i]       = alloc_frame_store();
    p_Dpb->fs_ref[i]   = NULL;
    p_Dpb->fs_ltref[i] = NULL;
  }

  p_Dpb->last_output_poc = INT_MIN;

#if (MVC_EXTENSION_ENABLE)
  p_Dpb->last_output_view_id = 0;
#endif

  p_Vid->last_has_mmco_5 = 0;

  p_Dpb->init_done = 1;

}


/*!
 ************************************************************************
 * \brief
 *    Free memory for decoded picture buffer.
 ************************************************************************
 */
void free_dpb(DecodedPictureBuffer *p_Dpb)
{
  VideoParameters *p_Vid = p_Dpb->p_Vid;
  unsigned i;
  if (p_Dpb->fs)
  {
    for (i=0; i<p_Dpb->size; i++)
    {
      free_frame_store(p_Vid, p_Dpb->fs[i]);
    }
    free (p_Dpb->fs);
    p_Dpb->fs=NULL;
  }
  if (p_Dpb->fs_ref)
  {
    free (p_Dpb->fs_ref);
  }
  if (p_Dpb->fs_ltref)
  {
    free (p_Dpb->fs_ltref);
  }
  p_Dpb->last_output_poc = INT_MIN;
#if (MVC_EXTENSION_ENABLE)
  p_Dpb->last_output_view_id = 0;
#endif

  p_Dpb->init_done = 0;
}


/*!
 ************************************************************************
 * \brief
 *    Allocate memory for decoded picture buffer frame stores and initialize with sane values.
 *
 * \return
 *    the allocated FrameStore structure
 ************************************************************************
 */
FrameStore* alloc_frame_store(void)
{
  FrameStore *f;

  f = calloc (1, sizeof(FrameStore));
  if (NULL==f)
    no_mem_exit("alloc_frame_store: f");

  f->is_used      = 0;
  f->is_reference = 0;
  f->is_long_term = 0;
  f->is_orig_reference = 0;

  f->is_output = 0;

  f->frame        = NULL;;
  f->top_field    = NULL;
  f->bottom_field = NULL;

#if (MVC_EXTENSION_ENABLE)
  f->view_id            = -1;
  f->inter_view_flag[0] = 0;
  f->inter_view_flag[1] = 0;
  f->anchor_pic_flag[0] = 0;
  f->anchor_pic_flag[1] = 0;
#endif

  return f;
}

void alloc_pic_motion(VideoParameters *p_Vid, PicMotionParamsOld *motion, int size_y, int size_x)
{
  motion->mb_field = calloc (size_y * size_x, sizeof(byte));
  if (motion->mb_field == NULL)
    no_mem_exit("alloc_storable_picture: motion->mb_field");

  //get_mem2D (&(motion->field_frame), size_y, size_x);
}

/*!
 ************************************************************************
 * \brief
 *    Allocate memory for a stored picture.
 *
 * \param p_Vid
 *    VideoParameters
 * \param structure
 *    picture structure
 * \param size_x
 *    horizontal luma size
 * \param size_y
 *    vertical luma size
 * \param size_x_cr
 *    horizontal chroma size
 * \param size_y_cr
 *    vertical chroma size
 *
 * \return
 *    the allocated StorablePicture structure
 ************************************************************************
 */
StorablePicture* alloc_storable_picture(VideoParameters *p_Vid, PictureStructure structure, int size_x, int size_y, int size_x_cr, int size_y_cr)
{
  StorablePicture *s;
  int   nplane;
  InputParameters *p_Inp = p_Vid->p_Inp;

  //printf ("Allocating (%s) picture (x=%d, y=%d, x_cr=%d, y_cr=%d)\n", (type == FRAME)?"FRAME":(type == TOP_FIELD)?"TOP_FIELD":"BOTTOM_FIELD", size_x, size_y, size_x_cr, size_y_cr);

  s = calloc (1, sizeof(StorablePicture));
  if (NULL==s)
    no_mem_exit("alloc_storable_picture: s");

  s->imgY       = NULL;
  s->imgUV      = NULL;
  s->imgY_sub   = NULL;
  s->imgUV_sub  = NULL;
  
  s->p_img_sub[0] = NULL;
  s->p_img_sub[1] = NULL;
  s->p_img_sub[2] = NULL;

  s->de_mem = NULL;
  
  //get_mem2Dpel (&(s->imgY), size_y, size_x);
#if (MVC_EXTENSION_ENABLE)  
  if ((p_Vid->nal_reference_idc != NALU_PRIORITY_DISPOSABLE) || ((p_Inp->num_of_views == 2) && p_Vid->view_id == 0)) //p_Vid->inter_view_flag[structure?structure-1: structure]))
#else
  if (p_Vid->nal_reference_idc != NALU_PRIORITY_DISPOSABLE)
#endif
  {
    //if (p_Vid->nal_reference_idc == NALU_PRIORITY_DISPOSABLE)
    //printf("interpolate %d %d %d %d %d \n", p_Vid->active_sps->profile_idc, structure, p_Vid->nal_reference_idc, p_Vid->view_id, p_Vid->inter_view_flag[structure?structure-1: structure]);
    //get_mem4DpelWithPad(&(s->imgY_sub), 4, 4, size_y, size_x, IMG_PAD_SIZE_Y, IMG_PAD_SIZE_X);
    get_mem4DpelWithPadSeparately(&(s->imgY_sub), 4, 4, size_y, size_x, IMG_PAD_SIZE_Y, IMG_PAD_SIZE_X);
    s->imgY = s->imgY_sub[0][0];

     if ( p_Inp->ChromaMCBuffer || p_Vid->P444_joined || (p_Inp->yuv_format==YUV444 && !p_Vid->P444_joined))
     {
      // UV components
       if ( p_Vid->yuv_format != YUV400 )
       {
        if ( p_Vid->yuv_format == YUV420 )
        {
          //get_mem5DpelWithPad(&(s->imgUV_sub), 2, 8, 8, size_y_cr, size_x_cr, p_Vid->pad_size_uv_y, p_Vid->pad_size_uv_x);
          get_mem5DpelWithPadSeparately(&(s->imgUV_sub), 2, 8, 8, size_y_cr, size_x_cr, p_Vid->pad_size_uv_y, p_Vid->pad_size_uv_x);
        }
        else if ( p_Vid->yuv_format == YUV422 )
        {
          //get_mem5DpelWithPad(&(s->imgUV_sub), 2, 4, 8, size_y_cr, size_x_cr, p_Vid->pad_size_uv_y, p_Vid->pad_size_uv_x);
          get_mem5DpelWithPadSeparately(&(s->imgUV_sub), 2, 4, 8, size_y_cr, size_x_cr, p_Vid->pad_size_uv_y, p_Vid->pad_size_uv_x);
        }
        else
        { // YUV444
          //get_mem5DpelWithPad(&(s->imgUV_sub), 2, 4, 4, size_y_cr, size_x_cr, p_Vid->pad_size_uv_y, p_Vid->pad_size_uv_x);
          get_mem5DpelWithPadSeparately(&(s->imgUV_sub), 2, 4, 4, size_y_cr, size_x_cr, p_Vid->pad_size_uv_y, p_Vid->pad_size_uv_x);
        }
        s->p_img_sub[1] = s->imgUV_sub[0];
        s->p_img_sub[2] = s->imgUV_sub[1];
        s->imgUV = (imgpel ***)malloc(2*sizeof(imgpel**));
        s->imgUV[0] = s->imgUV_sub[0][0][0];
        s->imgUV[1] = s->imgUV_sub[1][0][0];
      }
     }
     else
     {
        get_mem3DpelWithPad(&(s->imgUV), 2, size_y_cr, size_x_cr, p_Vid->pad_size_uv_y, p_Vid->pad_size_uv_x);
     }
  }
  else
  {
    get_mem2DpelWithPad(&(s->imgY), size_y, size_x, IMG_PAD_SIZE_Y, IMG_PAD_SIZE_X);
    get_mem3DpelWithPad(&(s->imgUV), 2, size_y_cr, size_x_cr, p_Vid->pad_size_uv_y, p_Vid->pad_size_uv_x);
  }  
    
  s->p_img[0] = s->imgY;
  s->p_curr_img = s->p_img[0];    
  s->p_curr_img_sub = s->p_img_sub[0];

  if (p_Vid->yuv_format != YUV400)
  {
    //get_mem3Dpel (&(s->imgUV), 2, size_y_cr, size_x_cr);
    s->p_img[1] = s->imgUV[0];
    s->p_img[2] = s->imgUV[1];
  }

  /*
  if (p_Inp->MbInterlace)
  get_mem3Dmp    (&s->mv_info, size_y / BLOCK_SIZE, size_x / BLOCK_SIZE, 6);
  else
  get_mem3Dmp    (&s->mv_info, size_y / BLOCK_SIZE, size_x / BLOCK_SIZE, 2);
  */

  get_mem2Dmp (&s->mv_info, size_y / BLOCK_SIZE, size_x / BLOCK_SIZE);
  alloc_pic_motion(p_Vid, &s->motion, size_y / BLOCK_SIZE, size_x / BLOCK_SIZE);

  if( (p_Inp->separate_colour_plane_flag != 0) )
  {
    for( nplane=0; nplane<MAX_PLANE; nplane++ )
    {
      get_mem2Dmp (&s->JVmv_info[nplane], size_y / BLOCK_SIZE, size_x / BLOCK_SIZE);
      alloc_pic_motion(p_Vid, &s->JVmotion[nplane], size_y / BLOCK_SIZE, size_x / BLOCK_SIZE);
    }
  }

  if (p_Inp->rdopt == 3) 
  {
	  errdo_alloc_storable_picture(s, p_Vid, p_Inp, size_x, size_y, size_x_cr, size_y_cr);
  }			      

  s->pic_num=0;
  s->frame_num=0;
  s->long_term_frame_idx=0;
  s->long_term_pic_num=0;
  s->used_for_reference=0;
  s->is_long_term=0;
  s->non_existing=0;
  s->is_output = 0;

  s->structure=structure;
  s->size_x = size_x;
  s->size_y = size_y;
  s->size_x_padded = size_x + 2 * IMG_PAD_SIZE_X;
  s->size_y_padded = size_y + 2 * IMG_PAD_SIZE_Y;
  s->size_x_pad = size_x + 2 * IMG_PAD_SIZE_X - 1 - MB_BLOCK_SIZE -IMG_PAD_SIZE_X;
  s->size_y_pad = size_y + 2 * IMG_PAD_SIZE_Y - 1 - MB_BLOCK_SIZE -IMG_PAD_SIZE_Y;
  s->size_x_cr = size_x_cr;
  s->size_y_cr = size_y_cr;
  s->size_x_cr_pad = (int) (size_x_cr - 1) + (p_Vid->pad_size_uv_x << 1) - (p_Vid->mb_cr_size_x) - p_Vid->pad_size_uv_x;
  s->size_y_cr_pad = (int) (size_y_cr - 1) + (p_Vid->pad_size_uv_y << 1) - (p_Vid->mb_cr_size_y) - p_Vid->pad_size_uv_y;
  s->pad_size_uv_x = p_Vid->pad_size_uv_x;
  s->pad_size_uv_y = p_Vid->pad_size_uv_y;

  s->top_field    = NULL;
  s->bottom_field = NULL;
  s->frame        = NULL;

  s->coded_frame    = 0;
  s->mb_aff_frame_flag = 0;

  init_stats(p_Inp, &s->stats);
  return s;
}

/*!
 ************************************************************************
 * \brief
 *    Free frame store memory.
 *
 * \param p_Vid
 *    VideoParameters
 * \param f
 *    FrameStore to be freed
 *
 ************************************************************************
 */
void free_frame_store(VideoParameters *p_Vid, FrameStore* f)
{
  if (f)
  {
    if (f->frame)
    {
      free_storable_picture(p_Vid, f->frame);
      f->frame=NULL;
    }
    if (f->top_field)
    {
      free_storable_picture(p_Vid, f->top_field);
      f->top_field=NULL;
    }
    if (f->bottom_field)
    {
      free_storable_picture(p_Vid, f->bottom_field);
      f->bottom_field=NULL;
    }
    free(f);
  }
}

void free_pic_motion(PicMotionParamsOld *motion)
{
  if (motion->mb_field)
  {
    free(motion->mb_field);
    motion->mb_field = NULL;
  }
}

static void free_frame_data_memory(StorablePicture *picture, int bFreeImage)
{
  if(picture)
  {
    if (picture->imgY_sub)
    {
      if(bFreeImage)
      {
        picture->imgY = NULL;
        //free_mem4Dpel (picture->imgY_sub);
        //free_mem4DpelWithPad(picture->imgY_sub, IMG_PAD_SIZE_Y, IMG_PAD_SIZE_X);
        free_mem4DpelWithPadSeparately(picture->imgY_sub, 16, IMG_PAD_SIZE_Y, IMG_PAD_SIZE_X);
        picture->imgY_sub=NULL;
      }
      else
      {
        int k;
        for(k=1; k<16; k++)
        {
          if(picture->imgY_sub[k>>2][k&3])
          {
            free_mem2DpelWithPad(picture->imgY_sub[k>>2][k&3], IMG_PAD_SIZE_Y, IMG_PAD_SIZE_X);
            picture->imgY_sub[k>>2][k&3] = NULL;
          }
        }
      }
    }

    if (picture->imgUV_sub)
    {
      int iUVResX = 4*(picture->size_x/picture->size_x_cr);
      int iUVResY = 4*(picture->size_y/picture->size_y_cr);
      if(bFreeImage)
      {
        if(picture->imgUV)
        {
          free(picture->imgUV);
          picture->imgUV = NULL;
        }
        //free_mem5Dpel (picture->imgUV_sub);
        //free_mem5DpelWithPad(picture->imgUV_sub, picture->pad_size_uv_y, picture->pad_size_uv_x);
        free_mem5DpelWithPadSeparately(picture->imgUV_sub, 2*iUVResY*iUVResX, picture->pad_size_uv_y, picture->pad_size_uv_x);
        picture->imgUV_sub = NULL;
      }
      else
      {
        int i, j, k;
        for(k=1; k<iUVResY*iUVResX; k++)
        {
          j = k/iUVResX;
          i = k%iUVResX;
          if(picture->imgUV_sub[0][j][i])
          {
           free_mem2DpelWithPad(picture->imgUV_sub[0][j][i], picture->pad_size_uv_y, picture->pad_size_uv_x);
           picture->imgUV_sub[0][j][i] = NULL;
          }
          if(picture->imgUV_sub[1][j][i])
          {
           free_mem2DpelWithPad(picture->imgUV_sub[1][j][i], picture->pad_size_uv_y, picture->pad_size_uv_x);
           picture->imgUV_sub[1][j][i] = NULL;
          }
        }
      }
    }

    if (picture->mv_info)
    {
      free_mem2Dmp(picture->mv_info);
      picture->mv_info = NULL;
    }

    free_pic_motion(&picture->motion);
  }
}

/*!
 ************************************************************************
 * \brief
 *    Free picture memory.
 *
 * \param p_Vid
 *    VideoParameters
 * \param p
 *    Picture to be freed
 *
 ************************************************************************
 */
void free_storable_picture(VideoParameters *p_Vid, StorablePicture* p)
{
  if (p)
  {
    InputParameters *p_Inp = p_Vid->p_Inp;
    //if (p->imgY)
    if(p->imgY && !p->imgY_sub)
    {      
      //free_mem2Dpel (p->imgY);
      free_mem2DpelWithPad(p->imgY, IMG_PAD_SIZE_Y, IMG_PAD_SIZE_X);
      p->imgY=NULL;      
    }      

    if (p->imgUV && !p->imgUV_sub)
    {
      //free_mem3Dpel(p->imgUV);
      free_mem3DpelWithPad(p->imgUV, p_Vid->pad_size_uv_y, p_Vid->pad_size_uv_x);
      p->imgUV=NULL;
    }

    free_frame_data_memory(p, 1);

    if( (p_Inp->separate_colour_plane_flag != 0) )
    {
      int nplane;
      for( nplane=0; nplane<MAX_PLANE; nplane++ )
      {
      if (p->JVmv_info[nplane])
      {
        free_mem2Dmp(p->JVmv_info[nplane]);
        p->JVmv_info[nplane] = NULL;
      }

        free_pic_motion(&p->JVmotion[nplane]);
      }
      p->p_curr_img     = NULL;
      p->p_curr_img_sub = NULL;
      p->p_img[0]       = NULL;
      p->p_img[1]       = NULL;
      p->p_img[2]       = NULL;
      p->p_img_sub[0]   = NULL;
      p->p_img_sub[1]   = NULL;
      p->p_img_sub[2]   = NULL;
    }

    if (p_Inp->rdopt == 3) 
    {
      errdo_free_storable_picture(p);
    }

    free(p);
    p = NULL;
  }
}

/*!
 ************************************************************************
 * \brief
 *    mark FrameStore unused for reference
 *
 ************************************************************************
 */
static void unmark_for_reference(FrameStore* fs)
{
  if (fs->is_used & 1)
  {
    if (fs->top_field)
    {
      fs->top_field->used_for_reference = 0;
    }
  }
  if (fs->is_used & 2)
  {
    if (fs->bottom_field)
    {
      fs->bottom_field->used_for_reference = 0;
    }
  }
  if (fs->is_used == 3)
  {
    if (fs->top_field && fs->bottom_field)
    {
      fs->top_field->used_for_reference = 0;
      fs->bottom_field->used_for_reference = 0;
    }
    fs->frame->used_for_reference = 0;
  }

  fs->is_reference = 0;

  free_frame_data_memory(fs->frame, 0);
  free_frame_data_memory(fs->top_field, 0);
  free_frame_data_memory(fs->bottom_field, 0);
}


/*!
 ************************************************************************
 * \brief
 *    mark FrameStore unused for reference and reset long term flags
 *
 ************************************************************************
 */
static void unmark_for_long_term_reference(FrameStore* fs)
{

  if (fs->is_used & 1)
  {
    if (fs->top_field)
    {
      fs->top_field->used_for_reference = 0;
      fs->top_field->is_long_term = 0;
    }
  }
  if (fs->is_used & 2)
  {
    if (fs->bottom_field)
    {
      fs->bottom_field->used_for_reference = 0;
      fs->bottom_field->is_long_term = 0;
    }
  }
  if (fs->is_used == 3)
  {
    if (fs->top_field && fs->bottom_field)
    {
      fs->top_field->used_for_reference = 0;
      fs->top_field->is_long_term = 0;
      fs->bottom_field->used_for_reference = 0;
      fs->bottom_field->is_long_term = 0;
    }
    fs->frame->used_for_reference = 0;
    fs->frame->is_long_term = 0;
  }

  free_frame_data_memory(fs->frame, 0);
  free_frame_data_memory(fs->top_field, 0);
  free_frame_data_memory(fs->bottom_field, 0);

  fs->is_reference = 0;
  fs->is_long_term = 0;
}


/*!
 ************************************************************************
 * \brief
 *    compares two stored pictures by picture number for qsort in descending order
 *
 ************************************************************************
 */
static inline int compare_pic_by_pic_num_desc( const void *arg1, const void *arg2 )
{
  int pic_num1 = (*(StorablePicture**)arg1)->pic_num;
  int pic_num2 = (*(StorablePicture**)arg2)->pic_num;

  if (pic_num1 < pic_num2)
    return 1;
  if (pic_num1 > pic_num2)
    return -1;
  else
    return 0;
}

/*!
 ************************************************************************
 * \brief
 *    compares two stored pictures by picture number for qsort in descending order
 *
 ************************************************************************
 */
static inline int compare_pic_by_lt_pic_num_asc( const void *arg1, const void *arg2 )
{
  int long_term_pic_num1 = (*(StorablePicture**)arg1)->long_term_pic_num;
  int long_term_pic_num2 = (*(StorablePicture**)arg2)->long_term_pic_num;

  if ( long_term_pic_num1 < long_term_pic_num2)
    return -1;
  if ( long_term_pic_num1 > long_term_pic_num2)
    return 1;
  else
    return 0;
}

/*!
 ************************************************************************
 * \brief
 *    compares two frame stores by pic_num for qsort in descending order
 *
 ************************************************************************
 */
static inline int compare_fs_by_frame_num_desc( const void *arg1, const void *arg2 )
{
  int frame_num_wrap1 = (*(FrameStore**)arg1)->frame_num_wrap;
  int frame_num_wrap2 = (*(FrameStore**)arg2)->frame_num_wrap;
  if ( frame_num_wrap1 < frame_num_wrap2)
    return 1;
  if ( frame_num_wrap1 > frame_num_wrap2)
    return -1;
  else
    return 0;
}


/*!
 ************************************************************************
 * \brief
 *    compares two frame stores by lt_pic_num for qsort in descending order
 *
 ************************************************************************
 */
static inline int compare_fs_by_lt_pic_idx_asc( const void *arg1, const void *arg2 )
{
  int long_term_frame_idx1 = (*(FrameStore**)arg1)->long_term_frame_idx;
  int long_term_frame_idx2 = (*(FrameStore**)arg2)->long_term_frame_idx;

  if ( long_term_frame_idx1 < long_term_frame_idx2)
    return -1;
  if ( long_term_frame_idx1 > long_term_frame_idx2)
    return 1;
  else
    return 0;
}


/*!
 ************************************************************************
 * \brief
 *    compares two stored pictures by poc for qsort in ascending order
 *
 ************************************************************************
 */
static inline int compare_pic_by_poc_asc( const void *arg1, const void *arg2 )
{
  int poc1 = (*(StorablePicture**)arg1)->poc;
  int poc2 = (*(StorablePicture**)arg2)->poc;

  if ( poc1 < poc2)
    return -1;  
  if ( poc1 > poc2)
    return 1;
  else
    return 0;
}


/*!
 ************************************************************************
 * \brief
 *    compares two stored pictures by poc for qsort in descending order
 *
 ************************************************************************
 */
static inline int compare_pic_by_poc_desc( const void *arg1, const void *arg2 )
{
  int poc1 = (*(StorablePicture**)arg1)->poc;
  int poc2 = (*(StorablePicture**)arg2)->poc;

  if (poc1 < poc2)
    return 1;
  if (poc1 > poc2)
    return -1;
  else
    return 0;
}


/*!
 ************************************************************************
 * \brief
 *    compares two frame stores by poc for qsort in ascending order
 *
 ************************************************************************
 */
static inline int compare_fs_by_poc_asc( const void *arg1, const void *arg2 )
{
  int poc1 = (*(FrameStore**)arg1)->poc;
  int poc2 = (*(FrameStore**)arg2)->poc;

  if (poc1 < poc2)
    return -1;
  if (poc1 > poc2)
    return 1;
  else
    return 0;
}


/*!
 ************************************************************************
 * \brief
 *    compares two frame stores by poc for qsort in descending order
 *
 ************************************************************************
 */
static inline int compare_fs_by_poc_desc( const void *arg1, const void *arg2 )
{
  int poc1 = (*(FrameStore**)arg1)->poc;
  int poc2 = (*(FrameStore**)arg2)->poc;

  if (poc1 < poc2)
    return 1;
  if (poc1 > poc2)
    return -1;
  else
    return 0;
}


/*!
 ************************************************************************
 * \brief
 *    returns true, if picture is short term reference picture
 *
 ************************************************************************
 */
int is_short_ref(StorablePicture *s)
{
  return ((s->used_for_reference) && (!(s->is_long_term)));
}


/*!
 ************************************************************************
 * \brief
 *    returns true, if picture is long term reference picture
 *
 ************************************************************************
 */
int is_long_ref(StorablePicture *s)
{
  return ((s->used_for_reference) && (s->is_long_term));
}


/*!
 ************************************************************************
 * \brief
 *    Generates a alternating field list from a given FrameStore list
 *
 ************************************************************************
 */
static void gen_pic_list_from_frame_list(PictureStructure currStructure, FrameStore **fs_list, int list_idx, StorablePicture **list, char *list_size, int long_term)
{
  int top_idx = 0;
  int bot_idx = 0;

  int (*is_ref)(StorablePicture *s);

  if (long_term)
    is_ref=is_long_ref;
  else
    is_ref=is_short_ref;

  if (currStructure == TOP_FIELD)
  {
    while ((top_idx<list_idx)||(bot_idx<list_idx))
    {
      for ( ; top_idx<list_idx; top_idx++)
      {
        if(fs_list[top_idx]->is_used & 1)
        {
          if(is_ref(fs_list[top_idx]->top_field))
          {
            // short term ref pic
            list[(short) *list_size] = fs_list[top_idx]->top_field;
            (*list_size)++;
            top_idx++;
            break;
          }
        }
      }
      for ( ; bot_idx<list_idx; bot_idx++)
      {
        if(fs_list[bot_idx]->is_used & 2)
        {
          if(is_ref(fs_list[bot_idx]->bottom_field))
          {
            // short term ref pic
            list[(short) *list_size] = fs_list[bot_idx]->bottom_field;
            (*list_size)++;
            bot_idx++;
            break;
          }
        }
      }
    }
  }
  if (currStructure == BOTTOM_FIELD)
  {
    while ((top_idx<list_idx)||(bot_idx<list_idx))
    {
      for ( ; bot_idx<list_idx; bot_idx++)
      {
        if(fs_list[bot_idx]->is_used & 2)
        {
          if(is_ref(fs_list[bot_idx]->bottom_field))
          {
            // short term ref pic
            list[(short) *list_size] = fs_list[bot_idx]->bottom_field;
            (*list_size)++;
            bot_idx++;
            break;
          }
        }
      }
      for ( ; top_idx<list_idx; top_idx++)
      {
        if(fs_list[top_idx]->is_used & 1)
        {
          if(is_ref(fs_list[top_idx]->top_field))
          {
            // short term ref pic
            list[(short) *list_size] = fs_list[top_idx]->top_field;
            (*list_size)++;
            top_idx++;
            break;
          }
        }
      }
    }
  }
}


void update_pic_num(Slice *currSlice)
{
  unsigned int i;
  VideoParameters *p_Vid = currSlice->p_Vid;
  DecodedPictureBuffer *p_Dpb = currSlice->p_Dpb;

  int add_top = 0, add_bottom = 0;

#if (MVC_EXTENSION_ENABLE)
  InputParameters *p_Inp = currSlice->p_Inp;
#endif

  if (currSlice->structure == FRAME)
  {
    for (i=0; i<p_Dpb->ref_frames_in_buffer; i++)
    {
#if (MVC_EXTENSION_ENABLE)
      if (p_Dpb->fs_ref[i]->is_used==3 && (p_Inp->num_of_views == 1 || p_Dpb->fs_ref[i]->view_id == p_Vid->view_id))
#else
      if (p_Dpb->fs_ref[i]->is_used==3)
#endif
      {
        if ((p_Dpb->fs_ref[i]->frame->used_for_reference)&&(!p_Dpb->fs_ref[i]->frame->is_long_term))
        {
          if( p_Dpb->fs_ref[i]->frame_num > currSlice->frame_num )
          {
#if (MVC_EXTENSION_ENABLE)
            if ( p_Inp->num_of_views == 2 )
              p_Dpb->fs_ref[i]->frame_num_wrap = p_Dpb->fs_ref[i]->frame_num - (currSlice->max_frame_num << 1);
            else
#endif
            p_Dpb->fs_ref[i]->frame_num_wrap = p_Dpb->fs_ref[i]->frame_num - currSlice->max_frame_num;
          }
          else
          {
            p_Dpb->fs_ref[i]->frame_num_wrap = p_Dpb->fs_ref[i]->frame_num;
          }
          p_Dpb->fs_ref[i]->frame->pic_num = p_Dpb->fs_ref[i]->frame_num_wrap;
        }
      }
    }
    // update long_term_pic_num
    for (i=0; i<p_Dpb->ltref_frames_in_buffer; i++)
    {
#if (MVC_EXTENSION_ENABLE)
      if (p_Dpb->fs_ltref[i]->is_used==3 && (p_Inp->num_of_views==1 || p_Dpb->fs_ref[i]->view_id == p_Vid->view_id))
#else
      if (p_Dpb->fs_ltref[i]->is_used==3)
#endif
      {
        if (p_Dpb->fs_ltref[i]->frame->is_long_term)
        {
          p_Dpb->fs_ltref[i]->frame->long_term_pic_num = p_Dpb->fs_ltref[i]->frame->long_term_frame_idx;
        }
      }
    }
  }
  else
  {
    if (currSlice->structure == TOP_FIELD)
    {
      add_top    = 1;
      add_bottom = 0;
    }
    else
    {
      add_top    = 0;
      add_bottom = 1;
    }

    for (i=0; i<p_Dpb->ref_frames_in_buffer; i++)
    {
#if (MVC_EXTENSION_ENABLE)
      if (p_Dpb->fs_ref[i]->is_reference && (p_Inp->num_of_views == 1 || p_Dpb->fs_ref[i]->view_id == p_Vid->view_id))
#else
      if (p_Dpb->fs_ref[i]->is_reference)
#endif
      {        
        if( p_Dpb->fs_ref[i]->frame_num > currSlice->frame_num )
        {
          p_Dpb->fs_ref[i]->frame_num_wrap = p_Dpb->fs_ref[i]->frame_num - currSlice->max_frame_num;
        }
        else
        {
          p_Dpb->fs_ref[i]->frame_num_wrap = p_Dpb->fs_ref[i]->frame_num;
        }
        if (p_Dpb->fs_ref[i]->is_reference & 1)
        {
          p_Dpb->fs_ref[i]->top_field->pic_num = (2 * p_Dpb->fs_ref[i]->frame_num_wrap) + add_top;
        }
        if (p_Dpb->fs_ref[i]->is_reference & 2)
        {
          p_Dpb->fs_ref[i]->bottom_field->pic_num = (2 * p_Dpb->fs_ref[i]->frame_num_wrap) + add_bottom;
        }
      }
    }
  
    // update long_term_pic_num
    for (i=0; i<p_Dpb->ltref_frames_in_buffer; i++)
    {
#if (MVC_EXTENSION_ENABLE)
      if (p_Inp->num_of_views == 1 || p_Dpb->fs_ref[i]->view_id == p_Vid->view_id)
#endif
      {
        if (p_Dpb->fs_ltref[i]->is_long_term & 1)
        {
          p_Dpb->fs_ltref[i]->top_field->long_term_pic_num = 2 * p_Dpb->fs_ltref[i]->top_field->long_term_frame_idx + add_top;
        }
        if (p_Dpb->fs_ltref[i]->is_long_term & 2)
        {
          p_Dpb->fs_ltref[i]->bottom_field->long_term_pic_num = 2 * p_Dpb->fs_ltref[i]->bottom_field->long_term_frame_idx + add_bottom;
        }
      }
    }
  }
}
/*!
 ************************************************************************
 * \brief
 *    Initialize reference lists depending on current slice type
 *
 ************************************************************************
 */
void init_lists_i_slice(Slice *currSlice)
{
  currSlice->listXsize[0] = 0;
  currSlice->listXsize[1] = 0;
}



/*!
 ************************************************************************
 * \brief
 *    Initialize reference lists for a P Slice
 *
 ************************************************************************
 */
void init_lists_p_slice(Slice *currSlice)
{
  VideoParameters *p_Vid = currSlice->p_Vid;
  DecodedPictureBuffer *p_Dpb = currSlice->p_Dpb;

  unsigned int i, j;

  int list0idx = 0;
  int listltidx = 0;

#if (MVC_EXTENSION_ENABLE)
  InputParameters *p_Inp = currSlice->p_Inp;
  int interview_pos = 0;
#endif

  FrameStore **fs_list0;
  FrameStore **fs_listlt;

  for (i = 0; i < 6; i++)
  {
    currSlice->listX[i] = calloc(MAX_LIST_SIZE, sizeof (StorablePicture*)); // +1 for reordering
    if (NULL == currSlice->listX[i])
      no_mem_exit("init_dpb: currSlice->listX[i]");
  }

  for (j = 0; j < 6; j++)
  {
    for (i = 0; i < MAX_LIST_SIZE; i++)
    {
      currSlice->listX[j][i] = NULL;
    }
    currSlice->listXsize[j]=0;
  }

  // Calculate FrameNumWrap and PicNum
  if (currSlice->structure == FRAME)
  {
    for (i=0; i<p_Dpb->ref_frames_in_buffer; i++)
    {
#if (MVC_EXTENSION_ENABLE)
      if (p_Dpb->fs_ref[i]->is_used==3 && (p_Inp->num_of_views==1 || p_Dpb->fs_ref[i]->view_id==p_Vid->view_id))
#else
      if (p_Dpb->fs_ref[i]->is_used==3)
#endif
      {
        if ((p_Dpb->fs_ref[i]->frame->used_for_reference)&&(!p_Dpb->fs_ref[i]->frame->is_long_term))
        {
          currSlice->listX[0][list0idx++] = p_Dpb->fs_ref[i]->frame;
        }
      }
    }
    // order list 0 by PicNum
    qsort((void *)currSlice->listX[0], list0idx, sizeof(StorablePicture*), compare_pic_by_pic_num_desc);
    currSlice->listXsize[0] = (char) list0idx;
    //printf("listX[0] (PicNum): "); for (i=0; i<list0idx; i++){printf ("%d  ", currSlice->listX[0][i]->pic_num);} printf("\n");

    // long term handling
    for (i=0; i<p_Dpb->ltref_frames_in_buffer; i++)
    {
#if (MVC_EXTENSION_ENABLE)
      if (p_Dpb->fs_ltref[i]->is_used==3 && (p_Inp->num_of_views==1 || p_Dpb->fs_ref[i]->view_id==p_Vid->view_id))
#else
      if (p_Dpb->fs_ltref[i]->is_used==3)
#endif
      {
        if (p_Dpb->fs_ltref[i]->frame->is_long_term)
        {
          currSlice->listX[0][list0idx++] = p_Dpb->fs_ltref[i]->frame;
        }
      }
    }
    qsort((void *)&currSlice->listX[0][(short) currSlice->listXsize[0]], list0idx - currSlice->listXsize[0], sizeof(StorablePicture*), compare_pic_by_lt_pic_num_asc);
    currSlice->listXsize[0] = (char) list0idx;
    //printf("listX[0] currPoc=%d (Poc): ", currSlice->framepoc); for (i=0; i<(unsigned int) currSlice->listXsize[0]; i++){printf ("%d  ", currSlice->listX[0][i]->poc);} printf("\n");
  }
  else
  {
    fs_list0 = calloc(p_Dpb->size, sizeof (FrameStore*));
    if (NULL==fs_list0)
      no_mem_exit("init_lists: fs_list0");
    fs_listlt = calloc(p_Dpb->size, sizeof (FrameStore*));
    if (NULL==fs_listlt)
      no_mem_exit("init_lists: fs_listlt");

    for (i=0; i<p_Dpb->ref_frames_in_buffer; i++)
    {
#if (MVC_EXTENSION_ENABLE)
      if (p_Dpb->fs_ref[i]->is_reference && (p_Inp->num_of_views == 1 || p_Dpb->fs_ref[i]->view_id == p_Vid->view_id))
#else
      if (p_Dpb->fs_ref[i]->is_reference)
#endif
      {
        fs_list0[list0idx++] = p_Dpb->fs_ref[i];
      }
    }

    qsort((void *)fs_list0, list0idx, sizeof(FrameStore*), compare_fs_by_frame_num_desc);

    //printf("fs_list0 (FrameNum): "); for (i=0; i<list0idx; i++){printf ("%d  ", fs_list0[i]->frame_num_wrap);} printf("\n");

    currSlice->listXsize[0] = 0;
    gen_pic_list_from_frame_list(currSlice->structure, fs_list0, list0idx, currSlice->listX[0], &currSlice->listXsize[0], 0);

    //printf("listX[0] (PicNum): "); for (i=0; i < currSlice->listXsize[0]; i++){printf ("%d  ", currSlice->listX[0][i]->pic_num);} printf("\n");

    // long term handling
    for (i=0; i<p_Dpb->ltref_frames_in_buffer; i++)
    {
#if (MVC_EXTENSION_ENABLE)
      if (p_Inp->num_of_views == 1 || p_Dpb->fs_ltref[i]->view_id == p_Vid->view_id)
#endif
      fs_listlt[listltidx++]=p_Dpb->fs_ltref[i];
    }

    qsort((void *)fs_listlt, listltidx, sizeof(FrameStore*), compare_fs_by_lt_pic_idx_asc);

    gen_pic_list_from_frame_list(currSlice->structure, fs_listlt, listltidx, currSlice->listX[0], &currSlice->listXsize[0], 1);

    free(fs_list0);
    free(fs_listlt);
  }
  currSlice->listXsize[1] = 0;


#if (MVC_EXTENSION_ENABLE)
  // MVC interview pictures list
  if(p_Inp->num_of_views == 2 && p_Vid->view_id == 1)
  {
    StorablePicture *base_ref;
    if(currSlice->structure == FRAME)
    {
      currSlice->listXsize[0] = (char) imin (currSlice->listXsize[0], currSlice->num_ref_idx_active[LIST_0]);
      currSlice->listXsize[1] = (char) imin (currSlice->listXsize[1], currSlice->num_ref_idx_active[LIST_1]);
      list0idx = currSlice->listXsize[0];
      interview_pos = p_Dpb->used_size - 1;
      base_ref      = p_Dpb->fs[interview_pos]->frame;

      if (base_ref->used_for_reference || base_ref->inter_view_flag[0])
      {
        p_Dpb->fs[interview_pos]->frame_num_wrap = -99;
        currSlice->listX[0][list0idx++] = base_ref;
        currSlice->listXsize[0] = (char) list0idx;
      }
    }
    else  //FIELD
    {
      int cur_inter_view_flag = currSlice->structure != BOTTOM_FIELD ? 0 : 1;
      currSlice->listXsize[0] = (char) imin (currSlice->listXsize[0], currSlice->num_ref_idx_active[LIST_0]);
      currSlice->listXsize[1] = (char) imin (currSlice->listXsize[1], currSlice->num_ref_idx_active[LIST_1]);
      list0idx = currSlice->listXsize[0];
      interview_pos = (currSlice->structure != BOTTOM_FIELD) ? p_Dpb->used_size - 1: p_Dpb->used_size - 2; // top field is at the last position
      base_ref = currSlice->structure!=BOTTOM_FIELD?p_Dpb->fs[interview_pos]->top_field:p_Dpb->fs[interview_pos]->bottom_field;

      if (base_ref->used_for_reference || base_ref->inter_view_flag[cur_inter_view_flag])
      {
        p_Dpb->fs[interview_pos]->frame_num_wrap = -99;
        currSlice->listX[0][list0idx++] = base_ref;
        currSlice->listXsize[0] = (char) list0idx;
      }
    }
  }
#endif

  // set max size
#if (MVC_EXTENSION_ENABLE)
  if(p_Inp->num_of_views == 2 && p_Inp->MVCInterViewReorder && p_Vid->view_id == 1)
  {
    currSlice->listXsize[0] = (char) imin (currSlice->listXsize[0], currSlice->num_ref_idx_active[LIST_0] + 1);
    currSlice->listXsize[1] = (char) imin (currSlice->listXsize[1], currSlice->num_ref_idx_active[LIST_1]);
  }
  else
#endif
  {
    currSlice->listXsize[0] = (char) imin (currSlice->listXsize[0], currSlice->num_ref_idx_active[LIST_0]);
    currSlice->listXsize[1] = (char) imin (currSlice->listXsize[1], currSlice->num_ref_idx_active[LIST_1]);
  }

  // set the unused list entries to NULL
  for (i=currSlice->listXsize[0]; i< (MAX_LIST_SIZE) ; i++)
  {
    currSlice->listX[0][i] = NULL;
  }
  for (i=currSlice->listXsize[1]; i< (MAX_LIST_SIZE) ; i++)
  {
    currSlice->listX[1][i] = NULL;
  }

/*#if PRINTREFLIST
#if (MVC_EXTENSION_ENABLE)
  // print out for debug purpose
  if(p_Inp->num_of_views==2 && p_Vid->current_slice_nr==0)
  {
    if(currSlice->listXsize[0]>0)
    {
      printf("\n");
      printf(" ** (CurViewID:%d) %s Ref Pic List 0 ****\n", p_Vid->view_id, currSlice->structure==FRAME ? "FRM":(currSlice->structure==TOP_FIELD ? "TOP":"BOT"));
      for(i=0; i<(unsigned int)(currSlice->listXsize[0]); i++)	//ref list 0
      {
        printf("   %2d -> POC: %4d PicNum: %4d ViewID: %d\n", i, currSlice->listX[0][i]->poc, currSlice->listX[0][i]->pic_num, currSlice->listX[0][i]->view_id);
      }
    }
  }
#endif
#endif*/
}


/*!
 ************************************************************************
 * \brief
 *    Initialize reference lists for a B Slice
 *
 ************************************************************************
 */
void init_lists_b_slice(Slice *currSlice)
{
  VideoParameters *p_Vid = currSlice->p_Vid;
  DecodedPictureBuffer *p_Dpb = currSlice->p_Dpb;

  unsigned int i;
  int j, diff;

  int list0idx = 0;
  int list0idx_1 = 0;
  int listltidx = 0;

#if (MVC_EXTENSION_ENABLE)
  InputParameters *p_Inp = currSlice->p_Inp;
  int list1idx = 0;
  int interview_pos = 0;
#endif

  FrameStore **fs_list0;
  FrameStore **fs_list1;
  FrameStore **fs_listlt;

  StorablePicture *tmp_s;

  for (i = 0; i < 6; i++)
  {
    currSlice->listX[i] = calloc(MAX_LIST_SIZE, sizeof (StorablePicture*)); // +1 for reordering
    if (NULL == currSlice->listX[i])
      no_mem_exit("init_dpb: currSlice->listX[i]");
  }

  for (j = 0; j < 6; j++)
  {
    for (i = 0; i < MAX_LIST_SIZE; i++)
    {
      currSlice->listX[j][i] = NULL;
    }
    currSlice->listXsize[j]=0;
  }

  {
    // B-Slice
    if (currSlice->structure == FRAME)
    {
      for (i = 0; i < p_Dpb->ref_frames_in_buffer; i++)
      {
#if (MVC_EXTENSION_ENABLE)
        if (p_Dpb->fs_ref[i]->is_used==3 && (p_Inp->num_of_views==1 || p_Dpb->fs_ref[i]->view_id==p_Vid->view_id))
#else
        if (p_Dpb->fs_ref[i]->is_used==3)
#endif
        {
          if ((p_Dpb->fs_ref[i]->frame->used_for_reference)&&(!p_Dpb->fs_ref[i]->frame->is_long_term))
          {
            if (currSlice->framepoc > p_Dpb->fs_ref[i]->frame->poc)
            {
              currSlice->listX[0][list0idx++] = p_Dpb->fs_ref[i]->frame;
            }
          }
        }
      }
      qsort((void *)currSlice->listX[0], list0idx, sizeof(StorablePicture*), compare_pic_by_poc_desc);

      //get the backward reference picture (POC>current POC) in list0;
      list0idx_1 = list0idx;
      for (i=0; i<p_Dpb->ref_frames_in_buffer; i++)
      {
#if (MVC_EXTENSION_ENABLE)
        if (p_Dpb->fs_ref[i]->is_used==3 && (p_Inp->num_of_views==1 || p_Dpb->fs_ref[i]->view_id==p_Vid->view_id))
#else
        if (p_Dpb->fs_ref[i]->is_used==3)
#endif
        {
          if ((p_Dpb->fs_ref[i]->frame->used_for_reference)&&(!p_Dpb->fs_ref[i]->frame->is_long_term))
          {
            if (currSlice->framepoc < p_Dpb->fs_ref[i]->frame->poc)
            {
              currSlice->listX[0][list0idx++] = p_Dpb->fs_ref[i]->frame;
            }
          }
        }
      }
      qsort((void *)&currSlice->listX[0][list0idx_1], list0idx-list0idx_1, sizeof(StorablePicture*), compare_pic_by_poc_asc);

      for (j=0; j<list0idx_1; j++)
      {
        currSlice->listX[1][list0idx-list0idx_1+j]=currSlice->listX[0][j];
      }
      for (j=list0idx_1; j<list0idx; j++)
      {
        currSlice->listX[1][j-list0idx_1]=currSlice->listX[0][j];
      }

      currSlice->listXsize[0] = currSlice->listXsize[1] = (char) list0idx;

      //      printf("currSlice->listX[0] currPoc=%d (Poc): ", currSlice->framepoc); for (i=0; i<currSlice->listXsize[0]; i++){printf ("%d  ", currSlice->listX[0][i]->poc);} printf("\n");
      //      printf("currSlice->listX[1] currPoc=%d (Poc): ", currSlice->framepoc); for (i=0; i<currSlice->listXsize[1]; i++){printf ("%d  ", currSlice->listX[1][i]->poc);} printf("\n");

      // long term handling
      for (i=0; i<p_Dpb->ltref_frames_in_buffer; i++)
      {
#if (MVC_EXTENSION_ENABLE)
        if (p_Dpb->fs_ltref[i]->is_used==3 && (p_Inp->num_of_views==1 || p_Dpb->fs_ref[i]->view_id==p_Vid->view_id))
#else
        if (p_Dpb->fs_ltref[i]->is_used==3)
#endif
        {
          if (p_Dpb->fs_ltref[i]->frame->is_long_term)
          {
            currSlice->listX[0][list0idx]   = p_Dpb->fs_ltref[i]->frame;
            currSlice->listX[1][list0idx++] = p_Dpb->fs_ltref[i]->frame;
          }
        }
      }
      qsort((void *)&currSlice->listX[0][(short) currSlice->listXsize[0]], list0idx - currSlice->listXsize[0], sizeof(StorablePicture*), compare_pic_by_lt_pic_num_asc);
      qsort((void *)&currSlice->listX[1][(short) currSlice->listXsize[0]], list0idx - currSlice->listXsize[0], sizeof(StorablePicture*), compare_pic_by_lt_pic_num_asc);
      currSlice->listXsize[0] = currSlice->listXsize[1] = (char) list0idx;
    }
    else
    {
      fs_list0 = calloc(p_Dpb->size, sizeof (FrameStore*));
      if (NULL==fs_list0)
        no_mem_exit("init_lists: fs_list0");
      fs_list1 = calloc(p_Dpb->size, sizeof (FrameStore*));
      if (NULL==fs_list1)
        no_mem_exit("init_lists: fs_list1");
      fs_listlt = calloc(p_Dpb->size, sizeof (FrameStore*));
      if (NULL==fs_listlt)
        no_mem_exit("init_lists: fs_listlt");

      currSlice->listXsize[0] = 0;
      currSlice->listXsize[1] = 1;

      for (i=0; i<p_Dpb->ref_frames_in_buffer; i++)
      {
#if (MVC_EXTENSION_ENABLE)
        if (p_Dpb->fs_ref[i]->is_used && (p_Inp->num_of_views==1 || p_Dpb->fs_ref[i]->view_id==p_Vid->view_id))
#else
        if (p_Dpb->fs_ref[i]->is_used)
#endif
        {
          if (currSlice->ThisPOC >= p_Dpb->fs_ref[i]->poc)
          {
            fs_list0[list0idx++] = p_Dpb->fs_ref[i];
          }
        }
      }
      qsort((void *)fs_list0, list0idx, sizeof(FrameStore*), compare_fs_by_poc_desc);
      list0idx_1 = list0idx;
      for (i=0; i<p_Dpb->ref_frames_in_buffer; i++)
      {
#if (MVC_EXTENSION_ENABLE)
        if (p_Dpb->fs_ref[i]->is_used && (p_Inp->num_of_views==1 || p_Dpb->fs_ref[i]->view_id==p_Vid->view_id))
#else
        if (p_Dpb->fs_ref[i]->is_used)
#endif
        {
          if (currSlice->ThisPOC < p_Dpb->fs_ref[i]->poc)
          {
            fs_list0[list0idx++] = p_Dpb->fs_ref[i];
          }
        }
      }
      qsort((void *)&fs_list0[list0idx_1], list0idx-list0idx_1, sizeof(FrameStore*), compare_fs_by_poc_asc);

      for (j=0; j<list0idx_1; j++)
      {
        fs_list1[list0idx-list0idx_1+j]=fs_list0[j];
      }
      for (j=list0idx_1; j<list0idx; j++)
      {
        fs_list1[j-list0idx_1]=fs_list0[j];
      }

      //      printf("fs_list0 currPoc=%d (Poc): ", currSlice->ThisPOC); for (i=0; i<list0idx; i++){printf ("%d  ", fs_list0[i]->poc);} printf("\n");
      //      printf("fs_list1 currPoc=%d (Poc): ", currSlice->ThisPOC); for (i=0; i<list0idx; i++){printf ("%d  ", fs_list1[i]->poc);} printf("\n");

      currSlice->listXsize[0] = 0;
      currSlice->listXsize[1] = 0;
      gen_pic_list_from_frame_list(currSlice->structure, fs_list0, list0idx, currSlice->listX[0], &currSlice->listXsize[0], 0);
      gen_pic_list_from_frame_list(currSlice->structure, fs_list1, list0idx, currSlice->listX[1], &currSlice->listXsize[1], 0);

      //      printf("currSlice->listX[0] currPoc=%d (Poc): ", currSlice->framepoc); for (i=0; i<currSlice->listXsize[0]; i++){printf ("%d  ", currSlice->listX[0][i]->poc);} printf("\n");
      //      printf("currSlice->listX[1] currPoc=%d (Poc): ", currSlice->framepoc); for (i=0; i<currSlice->listXsize[1]; i++){printf ("%d  ", currSlice->listX[1][i]->poc);} printf("\n");

      // long term handling
      for (i=0; i<p_Dpb->ltref_frames_in_buffer; i++)
      {
        fs_listlt[listltidx++]=p_Dpb->fs_ltref[i];
      }

      qsort((void *)fs_listlt, listltidx, sizeof(FrameStore*), compare_fs_by_lt_pic_idx_asc);

      gen_pic_list_from_frame_list(currSlice->structure, fs_listlt, listltidx, currSlice->listX[0], &currSlice->listXsize[0], 1);
      gen_pic_list_from_frame_list(currSlice->structure, fs_listlt, listltidx, currSlice->listX[1], &currSlice->listXsize[1], 1);

      free(fs_list0);
      free(fs_list1);
      free(fs_listlt);
    }
  }

  if ((currSlice->listXsize[0] == currSlice->listXsize[1]) && (currSlice->listXsize[0] > 1))
  {
    // check if lists are identical, if yes swap first two elements of currSlice->listX[1]
    diff=0;
    for (j = 0; j< currSlice->listXsize[0]; j++)
    {
      if (currSlice->listX[0][j]!=currSlice->listX[1][j])
        diff=1;
    }
    if (!diff)
    {
      tmp_s = currSlice->listX[1][0];
      currSlice->listX[1][0] = currSlice->listX[1][1];
      currSlice->listX[1][1] = tmp_s;
    }
  }

#if (MVC_EXTENSION_ENABLE)
  // MVC interview pictures list
  if(p_Inp->num_of_views == 2 && p_Vid->view_id == 1)
  { 
    StorablePicture *base_ref;
    if(currSlice->structure == FRAME)
    {
      currSlice->listXsize[0] = (char) imin (currSlice->listXsize[0], currSlice->num_ref_idx_active[LIST_0]);
      currSlice->listXsize[1] = (char) imin (currSlice->listXsize[1], currSlice->num_ref_idx_active[LIST_1]);
      list0idx = currSlice->listXsize[0];
      list1idx = currSlice->listXsize[1];
      interview_pos = p_Dpb->used_size-1;
      base_ref = p_Dpb->fs[interview_pos]->frame;

      if (base_ref->used_for_reference || base_ref->inter_view_flag[0])
      {
        p_Dpb->fs[interview_pos]->frame_num_wrap = -99;
        currSlice->listX[0][list0idx++] = base_ref;
        currSlice->listX[1][list1idx++] = base_ref;
        currSlice->listXsize[0] = (char) list0idx;
        currSlice->listXsize[1] = (char) list1idx;
      }
    }
    else  //FIELD
    {
      int cur_inter_view_flag = currSlice->structure != BOTTOM_FIELD ? 0 : 1;
      currSlice->listXsize[0] = (char) imin (currSlice->listXsize[0], currSlice->num_ref_idx_active[LIST_0]);
      currSlice->listXsize[1] = (char) imin (currSlice->listXsize[1], currSlice->num_ref_idx_active[LIST_1]);
      list0idx = currSlice->listXsize[0];
      list1idx = currSlice->listXsize[1];     
      interview_pos = currSlice->structure!=BOTTOM_FIELD ? p_Dpb->used_size - 1: p_Dpb->used_size - 2; // top field is at the last position
      base_ref = currSlice->structure != BOTTOM_FIELD ? p_Dpb->fs[interview_pos]->top_field:p_Dpb->fs[interview_pos]->bottom_field;
      if (base_ref->used_for_reference || base_ref->inter_view_flag[cur_inter_view_flag])
      {
        p_Dpb->fs[interview_pos]->frame_num_wrap = -99;
        currSlice->listX[0][list0idx++] = base_ref;
        currSlice->listX[1][list1idx++] = base_ref;
        currSlice->listXsize[0] = (char) list0idx;
        currSlice->listXsize[1] = (char) list1idx;
      }
    }
  }
#endif

  // set max size
#if (MVC_EXTENSION_ENABLE)
  if(p_Inp->num_of_views == 2 && p_Inp->MVCInterViewReorder && p_Vid->view_id == 1)
  {
    currSlice->listXsize[0] = (char) imin (currSlice->listXsize[0], currSlice->num_ref_idx_active[LIST_0] + 1);
    currSlice->listXsize[1] = (char) imin (currSlice->listXsize[1], currSlice->num_ref_idx_active[LIST_1]);
  }
  else
#endif
  {
    currSlice->listXsize[0] = (char) imin (currSlice->listXsize[0], currSlice->num_ref_idx_active[LIST_0]);
    currSlice->listXsize[1] = (char) imin (currSlice->listXsize[1], currSlice->num_ref_idx_active[LIST_1]);
  }

  // set the unused list entries to NULL
  for (i=currSlice->listXsize[0]; i< (MAX_LIST_SIZE) ; i++)
  {
    currSlice->listX[0][i] = NULL;
  }
  for (i=currSlice->listXsize[1]; i< (MAX_LIST_SIZE) ; i++)
  {
    currSlice->listX[1][i] = NULL;
  }

/*#if PRINTREFLIST
#if (MVC_EXTENSION_ENABLE)
  // print out for debug purpose
  if(p_Inp->num_of_views==2 && p_Vid->current_slice_nr==0)
  {
    if((currSlice->listXsize[0]>0) || (currSlice->listXsize[1]>0))
      printf("\n");
    if(currSlice->listXsize[0]>0)
    {
      printf(" ** (CurViewID:%d) %s Ref Pic List 0 ****\n", p_Vid->view_id, currSlice->structure==FRAME ? "FRM":(currSlice->structure==TOP_FIELD ? "TOP":"BOT"));
      for(i=0; i<(unsigned int)(currSlice->listXsize[0]); i++)	//ref list 0
      {
        printf("   %2d -> POC: %4d PicNum: %4d ViewID: %d\n", i, currSlice->listX[0][i]->poc, currSlice->listX[0][i]->pic_num, currSlice->listX[0][i]->view_id);
      }
    }
    if(currSlice->listXsize[1]>0)
    {
      printf(" ** (CurViewID:%d) %s Ref Pic List 1 ****\n", p_Vid->view_id, currSlice->structure==FRAME ? "FRM":(currSlice->structure==TOP_FIELD ? "TOP":"BOT"));
      for(i=0; i<(unsigned int)(currSlice->listXsize[1]); i++)	//ref list 1
      {
        printf("   %2d -> POC: %4d PicNum: %4d ViewID: %d\n", i, currSlice->listX[1][i]->poc, currSlice->listX[1][i]->pic_num, currSlice->listX[1][i]->view_id);
      }
    }
  }
#endif
#endif*/
}

/*!
 ************************************************************************
 * \brief
 *    Initialize listX[2..5] from lists 0 and 1
 *    listX[2]: list0 for current_field==top
 *    listX[3]: list1 for current_field==top
 *    listX[4]: list0 for current_field==bottom
 *    listX[5]: list1 for current_field==bottom
 *
 ************************************************************************
 */
void init_mbaff_lists(Slice *currSlice)
{
  // for the time being listX is part of p_Vid
  unsigned j;
  int i;

  for (i=2;i<6;i++)
  {
    if(currSlice->listX[i])
    {
      for (j=0; j<MAX_LIST_SIZE; j++)
      {
        currSlice->listX[i][j] = NULL;
      }
    }
    currSlice->listXsize[i]=0;
  }

  for (i = 0; i < currSlice->listXsize[0]; i++)
  {
    currSlice->listX[2][2*i  ] = currSlice->listX[0][i]->top_field;
    currSlice->listX[2][2*i+1] = currSlice->listX[0][i]->bottom_field;
    currSlice->listX[4][2*i  ] = currSlice->listX[0][i]->bottom_field;
    currSlice->listX[4][2*i+1] = currSlice->listX[0][i]->top_field;
  }
  currSlice->listXsize[2] = currSlice->listXsize[4] = currSlice->listXsize[0] * 2;

  for (i = 0; i < currSlice->listXsize[1]; i++)
  {
    currSlice->listX[3][2*i  ] = currSlice->listX[1][i]->top_field;
    currSlice->listX[3][2*i+1] = currSlice->listX[1][i]->bottom_field;
    currSlice->listX[5][2*i  ] = currSlice->listX[1][i]->bottom_field;
    currSlice->listX[5][2*i+1] = currSlice->listX[1][i]->top_field;
  }
  currSlice->listXsize[3] = currSlice->listXsize[5] = currSlice->listXsize[1] * 2;
}

 /*!
 ************************************************************************
 * \brief
 *    Returns short term pic with given picNum
 *
 ************************************************************************
 */
StorablePicture *get_short_term_pic(Slice *currSlice, DecodedPictureBuffer *p_Dpb, int picNum)
{
  unsigned i;

  for (i = 0; i < p_Dpb->ref_frames_in_buffer; i++)
  {
    if (currSlice->structure == FRAME)
    {
      if (p_Dpb->fs_ref[i]->is_reference == 3)
        if ((!p_Dpb->fs_ref[i]->frame->is_long_term)&&(p_Dpb->fs_ref[i]->frame->pic_num == picNum))
          return p_Dpb->fs_ref[i]->frame;
    }
    else
    {
      if (p_Dpb->fs_ref[i]->is_reference & 1)
        if ((!p_Dpb->fs_ref[i]->top_field->is_long_term)&&(p_Dpb->fs_ref[i]->top_field->pic_num == picNum))
          return p_Dpb->fs_ref[i]->top_field;
      if (p_Dpb->fs_ref[i]->is_reference & 2)
        if ((!p_Dpb->fs_ref[i]->bottom_field->is_long_term)&&(p_Dpb->fs_ref[i]->bottom_field->pic_num == picNum))
          return p_Dpb->fs_ref[i]->bottom_field;
    }
  }
  return NULL;
}

/*!
 ************************************************************************
 * \brief
 *    Returns long term pic with given LongtermPicNum
 *
 ************************************************************************
 */
static StorablePicture *get_long_term_pic(Slice *currSlice, DecodedPictureBuffer *p_Dpb, int LongtermPicNum)
{
  unsigned i;

  for (i=0; i<p_Dpb->ltref_frames_in_buffer; i++)
  {
    if (currSlice->structure==FRAME)
    {
      if (p_Dpb->fs_ltref[i]->is_reference == 3)
        if ((p_Dpb->fs_ltref[i]->frame->is_long_term)&&(p_Dpb->fs_ltref[i]->frame->long_term_pic_num == LongtermPicNum))
          return p_Dpb->fs_ltref[i]->frame;
    }
    else
    {
      if (p_Dpb->fs_ltref[i]->is_reference & 1)
        if ((p_Dpb->fs_ltref[i]->top_field->is_long_term)&&(p_Dpb->fs_ltref[i]->top_field->long_term_pic_num == LongtermPicNum))
          return p_Dpb->fs_ltref[i]->top_field;
      if (p_Dpb->fs_ltref[i]->is_reference & 2)
        if ((p_Dpb->fs_ltref[i]->bottom_field->is_long_term)&&(p_Dpb->fs_ltref[i]->bottom_field->long_term_pic_num == LongtermPicNum))
          return p_Dpb->fs_ltref[i]->bottom_field;
    }
  }
  return NULL;
}

/*!
 ************************************************************************
 * \brief
 *    Reordering process for short-term reference pictures
 *
 ************************************************************************
 */
static void reorder_short_term(Slice *currSlice, DecodedPictureBuffer *p_Dpb, int cur_list, int picNumLX, int *refIdxLX)
{
  StorablePicture **RefPicListX = currSlice->listX[cur_list];
  int cIdx, nIdx;

  StorablePicture *picLX;

  picLX = get_short_term_pic(currSlice, p_Dpb, picNumLX);

  for( cIdx = currSlice->num_ref_idx_active[cur_list]; cIdx > *refIdxLX; cIdx-- )
    RefPicListX[ cIdx ] = RefPicListX[ cIdx - 1];

  RefPicListX[ (*refIdxLX)++ ] = picLX;

  nIdx = *refIdxLX;

  for( cIdx = *refIdxLX; cIdx <= currSlice->num_ref_idx_active[cur_list]; cIdx++ )
  {
    if (RefPicListX[ cIdx ])
      if( (RefPicListX[ cIdx ]->is_long_term ) ||  (RefPicListX[ cIdx ]->pic_num != picNumLX ))
        RefPicListX[ nIdx++ ] = RefPicListX[ cIdx ];
  }
}


/*!
 ************************************************************************
 * \brief
 *    Reordering process for long-term reference pictures
 *
 ************************************************************************
 */
static void reorder_long_term(Slice *currSlice, DecodedPictureBuffer *p_Dpb, StorablePicture **RefPicListX, int cur_list, int frame_no, int *refIdxLX)
{
  int cIdx, nIdx;
  int LongTermPicNum = currSlice->long_term_pic_idx[cur_list][frame_no];

  StorablePicture *picLX;

  picLX = get_long_term_pic(currSlice, p_Dpb, LongTermPicNum);

  for( cIdx = currSlice->num_ref_idx_active[cur_list]; cIdx > *refIdxLX; cIdx-- )
    RefPicListX[ cIdx ] = RefPicListX[ cIdx - 1];

  RefPicListX[ (*refIdxLX)++ ] = picLX;

  nIdx = *refIdxLX;

  for( cIdx = *refIdxLX; cIdx <= currSlice->num_ref_idx_active[cur_list]; cIdx++ )
    if( (!RefPicListX[ cIdx ]->is_long_term ) ||  (RefPicListX[ cIdx ]->long_term_pic_num != LongTermPicNum ))
      RefPicListX[ nIdx++ ] = RefPicListX[ cIdx ];
}



/*!
 ************************************************************************
 * \brief
 *    Reordering process for reference picture lists
 *
 ************************************************************************
 */
void reorder_ref_pic_list(Slice *currSlice, int cur_list)
{
  int *reordering_of_pic_nums_idc  = currSlice->reordering_of_pic_nums_idc[cur_list];
  int *abs_diff_pic_num_minus1 = currSlice->abs_diff_pic_num_minus1[cur_list];
  DecodedPictureBuffer *p_Dpb = currSlice->p_Dpb;
  int i;

  int maxPicNum, currPicNum, picNumLXNoWrap, picNumLXPred, picNumLX;
  int refIdxLX = 0;

  if (currSlice->structure==FRAME)
  {
    maxPicNum  = currSlice->max_frame_num;
    currPicNum = currSlice->frame_num;
  }
  else
  {
    maxPicNum  = 2 * currSlice->max_frame_num;
    currPicNum = 2 * currSlice->frame_num + 1;
  }

  picNumLXPred = currPicNum;

  for (i=0; reordering_of_pic_nums_idc[i]!=3; i++)
  {
    if (reordering_of_pic_nums_idc[i] > 3)
      error ("Invalid remapping_of_pic_nums_idc command", 500);

    if (reordering_of_pic_nums_idc[i] < 2)
    {
      if (reordering_of_pic_nums_idc[i] == 0)
      {
        if( picNumLXPred - ( abs_diff_pic_num_minus1[i] + 1 ) < 0 )
          picNumLXNoWrap = picNumLXPred - ( abs_diff_pic_num_minus1[i] + 1 ) + maxPicNum;
        else
          picNumLXNoWrap = picNumLXPred - ( abs_diff_pic_num_minus1[i] + 1 );
      }
      else // (remapping_of_pic_nums_idc[i] == 1)
      {
        if( picNumLXPred + ( abs_diff_pic_num_minus1[i] + 1 )  >=  maxPicNum )
          picNumLXNoWrap = picNumLXPred + ( abs_diff_pic_num_minus1[i] + 1 ) - maxPicNum;
        else
          picNumLXNoWrap = picNumLXPred + ( abs_diff_pic_num_minus1[i] + 1 );
      }
      picNumLXPred = picNumLXNoWrap;

      if( picNumLXNoWrap > currPicNum )
        picNumLX = picNumLXNoWrap - maxPicNum;
      else
        picNumLX = picNumLXNoWrap;

      reorder_short_term(currSlice, p_Dpb, cur_list, picNumLX, &refIdxLX);
    }
    else //(reordering_of_pic_nums_idc[i] == 2)
    {
      reorder_long_term (currSlice, p_Dpb, currSlice->listX[cur_list], cur_list, i, &refIdxLX);
    }
  }

  // that's a definition
  currSlice->listXsize[cur_list] = currSlice->num_ref_idx_active[cur_list];
}


/*!
 ************************************************************************
 * \brief
 *    Update the list of frame stores that contain reference frames/fields
 *
 ************************************************************************
 */
#if (MVC_EXTENSION_ENABLE)
void update_ref_list(DecodedPictureBuffer *p_Dpb)
{
  unsigned i, j;
  for (i=0, j=0; i<p_Dpb->used_size; i++)
  {
    if (is_short_term_reference(p_Dpb->fs[i]) && p_Dpb->fs[i]->view_id == p_Dpb->p_Vid->view_id)
    {
      p_Dpb->fs_ref[j++]=p_Dpb->fs[i];
    }
  }

  p_Dpb->ref_frames_in_buffer = j;

  while (j<p_Dpb->size)
  {
    p_Dpb->fs_ref[j++]=NULL;
  }
}
#else
void update_ref_list(DecodedPictureBuffer *p_Dpb)
{
  unsigned i, j;
  for (i=0, j=0; i<p_Dpb->used_size; i++)
  {
    if (is_short_term_reference(p_Dpb->fs[i]))
    {
      p_Dpb->fs_ref[j++]=p_Dpb->fs[i];
    }
  }

  p_Dpb->ref_frames_in_buffer = j;

  while (j<p_Dpb->size)
  {
    p_Dpb->fs_ref[j++]=NULL;
  }
}
#endif


/*!
 ************************************************************************
 * \brief
 *    Update the list of frame stores that contain long-term reference
 *    frames/fields
 *
 ************************************************************************
 */
void update_ltref_list(DecodedPictureBuffer *p_Dpb)
{
  unsigned i, j;
  for (i=0, j=0; i<p_Dpb->used_size; i++)
  {
#if (MVC_EXTENSION_ENABLE)
    if (is_long_term_reference(p_Dpb->fs[i]) && p_Dpb->fs[i]->view_id == p_Dpb->p_Vid->view_id)
#else
    if (is_long_term_reference(p_Dpb->fs[i]))
#endif
    {
      p_Dpb->fs_ltref[j++] = p_Dpb->fs[i];
    }
  }

  p_Dpb->ltref_frames_in_buffer = j;

  while (j<p_Dpb->size)
  {
    p_Dpb->fs_ltref[j++]=NULL;
  }
}

/*!
 ************************************************************************
 * \brief
 *    Perform Memory management for idr pictures
 *
 ************************************************************************
 */
static void idr_memory_management(DecodedPictureBuffer *p_Dpb, StorablePicture* p, FrameFormat *output)
{
  unsigned i;
  VideoParameters *p_Vid = p_Dpb->p_Vid;

  assert (p_Vid->currentPicture->idr_flag);

  if (p_Vid->no_output_of_prior_pics_flag)
  {
    // free all stored pictures
    for (i=0; i<p_Dpb->used_size; i++)
    {
      // reset all reference settings
      free_frame_store(p_Vid, p_Dpb->fs[i]);
      p_Dpb->fs[i] = alloc_frame_store();
    }
    for (i=0; i<p_Dpb->ref_frames_in_buffer; i++)
    {
      p_Dpb->fs_ref[i]=NULL;
    }
    for (i=0; i<p_Dpb->ltref_frames_in_buffer; i++)
    {
      p_Dpb->fs_ltref[i]=NULL;
    }
    p_Dpb->used_size=0;
  }
  else
  {
    flush_dpb(p_Dpb, output);
  }
  p_Dpb->last_picture = NULL;

  update_ref_list(p_Dpb);
  update_ltref_list(p_Dpb);
  p_Dpb->last_output_poc = INT_MIN;
#if (MVC_EXTENSION_ENABLE)
  p_Dpb->last_output_view_id = 0;
#endif

  if (p_Vid->long_term_reference_flag)
  {
    p_Dpb->max_long_term_pic_idx = 0;
    p->is_long_term           = 1;
    p->long_term_frame_idx    = 0;
  }
  else
  {
    p_Dpb->max_long_term_pic_idx = -1;
    p->is_long_term           = 0;
  }
}

/*!
 ************************************************************************
 * \brief
 *    Perform Sliding window decoded reference picture marking process
 *
 ************************************************************************
 */
static void sliding_window_memory_management(DecodedPictureBuffer *p_Dpb, StorablePicture* p)
{
  VideoParameters *p_Vid = p_Dpb->p_Vid;
  unsigned i;

  // if this is a reference pic with sliding sliding window, unmark first ref frame
#if (MVC_EXTENSION_ENABLE)
  if(p_Vid->p_Inp->num_of_views==2)
  {
    if (p_Dpb->ref_frames_in_buffer==(p_Vid->active_sps->num_ref_frames) - p_Dpb->ltref_frames_in_buffer)
    {
      for (i=0; i<p_Dpb->used_size;i++)
      {
        if (p_Dpb->fs[i]->is_reference  && (!(p_Dpb->fs[i]->is_long_term)) && p_Dpb->fs[i]->view_id==p_Vid->view_id)
        {
          unmark_for_reference(p_Dpb->fs[i]);
          update_ref_list(p_Dpb);
          break;
        }
      }
    }
  }
  else
#endif
  {
    if (p_Dpb->ref_frames_in_buffer==p_Vid->active_sps->num_ref_frames - p_Dpb->ltref_frames_in_buffer)
    {
      for (i=0; i<p_Dpb->used_size;i++)
      {
        if (p_Dpb->fs[i]->is_reference  && (!(p_Dpb->fs[i]->is_long_term)))
        {
          unmark_for_reference(p_Dpb->fs[i]);
          update_ref_list(p_Dpb);
          break;
        }
      }
    }
  }

  p->is_long_term = 0;
}

/*!
 ************************************************************************
 * \brief
 *    Calculate picNumX
 ************************************************************************
 */
static int get_pic_num_x (StorablePicture *p, int difference_of_pic_nums_minus1)
{
  int currPicNum;

  if (p->structure == FRAME)
    currPicNum = p->frame_num;
  else
    currPicNum = 2 * p->frame_num + 1;

  return currPicNum - (difference_of_pic_nums_minus1 + 1);
}


/*!
 ************************************************************************
 * \brief
 *    Adaptive Memory Management: Mark short term picture unused
 ************************************************************************
 */
static void mm_unmark_short_term_for_reference(DecodedPictureBuffer *p_Dpb, StorablePicture *p, int difference_of_pic_nums_minus1)
{
  int picNumX;

  unsigned i;
#if MVC_EXTENSION_ENABLE
  if(p_Dpb->p_Vid->active_sps->profile_idc >=MULTIVIEW_HIGH)
    picNumX = get_pic_num_x(p, (difference_of_pic_nums_minus1+1)*2-1);
  else
#endif
   picNumX = get_pic_num_x(p, difference_of_pic_nums_minus1);

  for (i=0; i<p_Dpb->ref_frames_in_buffer; i++)
  {
    if (p->structure == FRAME)
    {
      if ((p_Dpb->fs_ref[i]->is_reference==3) && (p_Dpb->fs_ref[i]->is_long_term==0))
      {
        if (p_Dpb->fs_ref[i]->frame->pic_num == picNumX)
        {
          unmark_for_reference(p_Dpb->fs_ref[i]);
          return;
        }
      }
    }
    else
    {
      if ((p_Dpb->fs_ref[i]->is_reference & 1) && (!(p_Dpb->fs_ref[i]->is_long_term & 1)))
      {
        if (p_Dpb->fs_ref[i]->top_field->pic_num == picNumX)
        {
          p_Dpb->fs_ref[i]->top_field->used_for_reference = 0;
          p_Dpb->fs_ref[i]->is_reference &= 2;
          if (p_Dpb->fs_ref[i]->is_used == 3)
          {
            p_Dpb->fs_ref[i]->frame->used_for_reference = 0;
          }
          return;
        }
      }
      if ((p_Dpb->fs_ref[i]->is_reference & 2) && (!(p_Dpb->fs_ref[i]->is_long_term & 2)))
      {
        if (p_Dpb->fs_ref[i]->bottom_field->pic_num == picNumX)
        {
          p_Dpb->fs_ref[i]->bottom_field->used_for_reference = 0;
          p_Dpb->fs_ref[i]->is_reference &= 1;
          if (p_Dpb->fs_ref[i]->is_used == 3)
          {
            p_Dpb->fs_ref[i]->frame->used_for_reference = 0;
          }
          return;
        }
      }
    }
  }
}


/*!
 ************************************************************************
 * \brief
 *    Adaptive Memory Management: Mark long term picture unused
 ************************************************************************
 */
static void mm_unmark_long_term_for_reference(DecodedPictureBuffer *p_Dpb, StorablePicture *p, int long_term_pic_num)
{
  unsigned i;
  //VideoParameters *p_Vid = p_Dpb->p_Vid;
  for (i=0; i<p_Dpb->ltref_frames_in_buffer; i++)
  {
    if (p->structure == FRAME)
    {
      if ((p_Dpb->fs_ltref[i]->is_reference==3) && (p_Dpb->fs_ltref[i]->is_long_term==3))
      {
        if (p_Dpb->fs_ltref[i]->frame->long_term_pic_num == long_term_pic_num)
        {
          unmark_for_long_term_reference(p_Dpb->fs_ltref[i]);
        }
      }
    }
    else
    {
      if ((p_Dpb->fs_ltref[i]->is_reference & 1) && ((p_Dpb->fs_ltref[i]->is_long_term & 1)))
      {
        if (p_Dpb->fs_ltref[i]->top_field->long_term_pic_num == long_term_pic_num)
        {
          p_Dpb->fs_ltref[i]->top_field->used_for_reference = 0;
          p_Dpb->fs_ltref[i]->top_field->is_long_term = 0;
          p_Dpb->fs_ltref[i]->is_reference &= 2;
          p_Dpb->fs_ltref[i]->is_long_term &= 2;
          if (p_Dpb->fs_ltref[i]->is_used == 3)
          {
            p_Dpb->fs_ltref[i]->frame->used_for_reference = 0;
            p_Dpb->fs_ltref[i]->frame->is_long_term = 0;
          }
          return;
        }
      }
      if ((p_Dpb->fs_ltref[i]->is_reference & 2) && ((p_Dpb->fs_ltref[i]->is_long_term & 2)))
      {
        if (p_Dpb->fs_ltref[i]->bottom_field->long_term_pic_num == long_term_pic_num)
        {
          p_Dpb->fs_ltref[i]->bottom_field->used_for_reference = 0;
          p_Dpb->fs_ltref[i]->bottom_field->is_long_term = 0;
          p_Dpb->fs_ltref[i]->is_reference &= 1;
          p_Dpb->fs_ltref[i]->is_long_term &= 1;
          if (p_Dpb->fs_ltref[i]->is_used == 3)
          {
            p_Dpb->fs_ltref[i]->frame->used_for_reference = 0;
            p_Dpb->fs_ltref[i]->frame->is_long_term = 0;
          }
          return;
        }
      }
    }
  }
}


/*!
 ************************************************************************
 * \brief
 *    Mark a long-term reference frame or complementary field pair unused for referemce
 ************************************************************************
 */
static void unmark_long_term_frame_for_reference_by_frame_idx(DecodedPictureBuffer *p_Dpb, int long_term_frame_idx)
{
  unsigned i;
  //VideoParameters *p_Vid = p_Dpb->p_Vid;
  for(i=0; i<p_Dpb->ltref_frames_in_buffer; i++)
  {
    if (p_Dpb->fs_ltref[i]->long_term_frame_idx == long_term_frame_idx)
      unmark_for_long_term_reference(p_Dpb->fs_ltref[i]);
  }
}

/*!
 ************************************************************************
 * \brief
 *    Mark a long-term reference field unused for reference only if it's not
 *    the complementary field of the picture indicated by picNumX
 ************************************************************************
 */
static void unmark_long_term_field_for_reference_by_frame_idx(DecodedPictureBuffer *p_Dpb, PictureStructure structure, int long_term_frame_idx, int mark_current, unsigned curr_frame_num, int curr_pic_num)
{
  VideoParameters *p_Vid = p_Dpb->p_Vid;
  unsigned i;
  
  assert(structure!=FRAME);
  if (curr_pic_num<0)
    curr_pic_num += (2 * p_Vid->max_frame_num);

  for(i=0; i<p_Dpb->ltref_frames_in_buffer; i++)
  {
    if (p_Dpb->fs_ltref[i]->long_term_frame_idx == long_term_frame_idx)
    {
      if (structure == TOP_FIELD)
      {
        if ((p_Dpb->fs_ltref[i]->is_long_term == 3))
        {
          unmark_for_long_term_reference(p_Dpb->fs_ltref[i]);
        }
        else
        {
          if ((p_Dpb->fs_ltref[i]->is_long_term == 1))
          {
            unmark_for_long_term_reference(p_Dpb->fs_ltref[i]);
          }
          else
          {
            if (mark_current)
            {
              if (p_Dpb->last_picture)
              {
                if ( ( p_Dpb->last_picture != p_Dpb->fs_ltref[i] )|| p_Dpb->last_picture->frame_num != curr_frame_num)
                  unmark_for_long_term_reference(p_Dpb->fs_ltref[i]);
              }
              else
              {
                unmark_for_long_term_reference(p_Dpb->fs_ltref[i]);
              }
            }
            else
            {
              if ((p_Dpb->fs_ltref[i]->frame_num) != (unsigned)(curr_pic_num >> 1))
              {
                unmark_for_long_term_reference(p_Dpb->fs_ltref[i]);
              }
            }
          }
        }
      }
      if (structure == BOTTOM_FIELD)
      {
        if ((p_Dpb->fs_ltref[i]->is_long_term == 3))
        {
          unmark_for_long_term_reference(p_Dpb->fs_ltref[i]);
        }
        else
        {
          if ((p_Dpb->fs_ltref[i]->is_long_term == 2))
          {
            unmark_for_long_term_reference(p_Dpb->fs_ltref[i]);
          }
          else
          {
            if (mark_current)
            {
              if (p_Dpb->last_picture)
              {
                if ( ( p_Dpb->last_picture != p_Dpb->fs_ltref[i] )|| p_Dpb->last_picture->frame_num != curr_frame_num)
                  unmark_for_long_term_reference(p_Dpb->fs_ltref[i]);
              }
              else
              {
                unmark_for_long_term_reference(p_Dpb->fs_ltref[i]);
              }
            }
            else
            {
              if ((p_Dpb->fs_ltref[i]->frame_num) != (unsigned)(curr_pic_num >> 1))
              {
                unmark_for_long_term_reference(p_Dpb->fs_ltref[i]);
              }
            }
          }
        }
      }
    }
  }
}


/*!
 ************************************************************************
 * \brief
 *    mark a picture as long-term reference
 ************************************************************************
 */
static void mark_pic_long_term(DecodedPictureBuffer *p_Dpb, StorablePicture* p, int long_term_frame_idx, int picNumX)
{
  unsigned i;
  int add_top, add_bottom;

  if (p->structure == FRAME)
  {
    for (i=0; i<p_Dpb->ref_frames_in_buffer; i++)
    {
      if (p_Dpb->fs_ref[i]->is_reference == 3)
      {
        if ((!p_Dpb->fs_ref[i]->frame->is_long_term)&&(p_Dpb->fs_ref[i]->frame->pic_num == picNumX))
        {
          p_Dpb->fs_ref[i]->long_term_frame_idx = p_Dpb->fs_ref[i]->frame->long_term_frame_idx
                                             = long_term_frame_idx;
          p_Dpb->fs_ref[i]->frame->long_term_pic_num = long_term_frame_idx;
          p_Dpb->fs_ref[i]->frame->is_long_term = 1;

          if (p_Dpb->fs_ref[i]->top_field && p_Dpb->fs_ref[i]->bottom_field)
          {
            p_Dpb->fs_ref[i]->top_field->long_term_frame_idx = p_Dpb->fs_ref[i]->bottom_field->long_term_frame_idx
                                                          = long_term_frame_idx;
            p_Dpb->fs_ref[i]->top_field->long_term_pic_num = long_term_frame_idx;
            p_Dpb->fs_ref[i]->bottom_field->long_term_pic_num = long_term_frame_idx;

            p_Dpb->fs_ref[i]->top_field->is_long_term = p_Dpb->fs_ref[i]->bottom_field->is_long_term
                                                   = 1;

          }
          p_Dpb->fs_ref[i]->is_long_term = 3;
          return;
        }
      }
    }
    printf ("Warning: reference frame for long term marking not found\n");
  }
  else
  {
    if (p->structure == TOP_FIELD)
    {
      add_top    = 1;
      add_bottom = 0;
    }
    else
    {
      add_top    = 0;
      add_bottom = 1;
    }
    for (i=0; i<p_Dpb->ref_frames_in_buffer; i++)
    {
      if (p_Dpb->fs_ref[i]->is_reference & 1)
      {
        if ((!p_Dpb->fs_ref[i]->top_field->is_long_term)&&(p_Dpb->fs_ref[i]->top_field->pic_num == picNumX))
        {
          if ((p_Dpb->fs_ref[i]->is_long_term) && (p_Dpb->fs_ref[i]->long_term_frame_idx != long_term_frame_idx))
          {
              printf ("Warning: assigning long_term_frame_idx different from other field\n");
          }

          p_Dpb->fs_ref[i]->long_term_frame_idx = p_Dpb->fs_ref[i]->top_field->long_term_frame_idx
                                             = long_term_frame_idx;
          p_Dpb->fs_ref[i]->top_field->long_term_pic_num = 2 * long_term_frame_idx + add_top;
          p_Dpb->fs_ref[i]->top_field->is_long_term = 1;
          p_Dpb->fs_ref[i]->is_long_term |= 1;
          if (p_Dpb->fs_ref[i]->is_long_term == 3)
          {
            p_Dpb->fs_ref[i]->frame->is_long_term = 1;
            p_Dpb->fs_ref[i]->frame->long_term_frame_idx = p_Dpb->fs_ref[i]->frame->long_term_pic_num = long_term_frame_idx;
          }
          return;
        }
      }
      if (p_Dpb->fs_ref[i]->is_reference & 2)
      {
        if ((!p_Dpb->fs_ref[i]->bottom_field->is_long_term)&&(p_Dpb->fs_ref[i]->bottom_field->pic_num == picNumX))
        {
          if ((p_Dpb->fs_ref[i]->is_long_term) && (p_Dpb->fs_ref[i]->long_term_frame_idx != long_term_frame_idx))
          {
              printf ("Warning: assigning long_term_frame_idx different from other field\n");
          }

          p_Dpb->fs_ref[i]->long_term_frame_idx = p_Dpb->fs_ref[i]->bottom_field->long_term_frame_idx
                                             = long_term_frame_idx;
          p_Dpb->fs_ref[i]->bottom_field->long_term_pic_num = 2 * long_term_frame_idx + add_bottom;
          p_Dpb->fs_ref[i]->bottom_field->is_long_term = 1;
          p_Dpb->fs_ref[i]->is_long_term |= 2;
          if (p_Dpb->fs_ref[i]->is_long_term == 3)
          {
            p_Dpb->fs_ref[i]->frame->is_long_term = 1;
            p_Dpb->fs_ref[i]->frame->long_term_frame_idx = p_Dpb->fs_ref[i]->frame->long_term_pic_num = long_term_frame_idx;
          }
          return;
        }
      }
    }
    printf ("Warning: reference field for long term marking not found\n");
  }
}


/*!
 ************************************************************************
 * \brief
 *    Assign a long term frame index to a short term picture
 ************************************************************************
 */
static void mm_assign_long_term_frame_idx(DecodedPictureBuffer *p_Dpb, StorablePicture* p, int difference_of_pic_nums_minus1, int long_term_frame_idx)
{
  int picNumX = get_pic_num_x(p, difference_of_pic_nums_minus1);

  // remove frames/fields with same long_term_frame_idx
  if (p->structure == FRAME)
  {
    unmark_long_term_frame_for_reference_by_frame_idx(p_Dpb, long_term_frame_idx);
  }
  else
  {
    unsigned i;
    PictureStructure structure = FRAME;

    for (i=0; i<p_Dpb->ref_frames_in_buffer; i++)
    {
      if (p_Dpb->fs_ref[i]->is_reference & 1)
      {
        if (p_Dpb->fs_ref[i]->top_field->pic_num == picNumX)
        {
          structure = TOP_FIELD;
          break;
        }
      }
      if (p_Dpb->fs_ref[i]->is_reference & 2)
      {
        if (p_Dpb->fs_ref[i]->bottom_field->pic_num == picNumX)
        {
          structure = BOTTOM_FIELD;
          break;
        }
      }
    }
    if (structure==FRAME)
    {
      error ("field for long term marking not found",200);
    }

    unmark_long_term_field_for_reference_by_frame_idx(p_Dpb, structure, long_term_frame_idx, 0, 0, picNumX);
  }

  mark_pic_long_term(p_Dpb, p, long_term_frame_idx, picNumX);
}

/*!
 ************************************************************************
 * \brief
 *    Set new max long_term_frame_idx
 ************************************************************************
 */
void mm_update_max_long_term_frame_idx(DecodedPictureBuffer *p_Dpb, int max_long_term_frame_idx_plus1)
{
  unsigned i;
  //VideoParameters *p_Vid = p_Dpb->p_Vid;
  p_Dpb->max_long_term_pic_idx = max_long_term_frame_idx_plus1 - 1;
  
  // check for invalid frames
  for (i=0; i<p_Dpb->ltref_frames_in_buffer; i++)
  {
    if (p_Dpb->fs_ltref[i]->long_term_frame_idx > p_Dpb->max_long_term_pic_idx)
    {
      unmark_for_long_term_reference(p_Dpb->fs_ltref[i]);
    }
  }
}


/*!
 ************************************************************************
 * \brief
 *    Mark all long term reference pictures unused for reference
 ************************************************************************
 */
static void mm_unmark_all_long_term_for_reference (DecodedPictureBuffer *p_Dpb)
{
  mm_update_max_long_term_frame_idx(p_Dpb, 0);
}

/*!
 ************************************************************************
 * \brief
 *    Mark all short term reference pictures unused for reference
 ************************************************************************
 */
static void mm_unmark_all_short_term_for_reference (DecodedPictureBuffer *p_Dpb)
{
  unsigned int i;
  //VideoParameters *p_Vid = p_Dpb->p_Vid;
  for (i=0; i<p_Dpb->ref_frames_in_buffer; i++)
  {
    unmark_for_reference(p_Dpb->fs_ref[i]);
  }
  update_ref_list(p_Dpb);
}


/*!
 ************************************************************************
 * \brief
 *    Mark the current picture used for long term reference
 ************************************************************************
 */
static void mm_mark_current_picture_long_term(DecodedPictureBuffer *p_Dpb, StorablePicture *p, int long_term_frame_idx)
{
  // remove long term pictures with same long_term_frame_idx
  if (p->structure == FRAME)
  {
    unmark_long_term_frame_for_reference_by_frame_idx(p_Dpb, long_term_frame_idx);
  }
  else
  {
    unmark_long_term_field_for_reference_by_frame_idx(p_Dpb, p->structure, long_term_frame_idx, 1, p->pic_num, 0);
  }

  p->is_long_term = 1;
  p->long_term_frame_idx = long_term_frame_idx;
}


/*!
 ************************************************************************
 * \brief
 *    Perform Adaptive memory control decoded reference picture marking process
 ************************************************************************
 */
static void adaptive_memory_management(DecodedPictureBuffer *p_Dpb, StorablePicture* p, FrameFormat *output)
{
  DecRefPicMarking_t *tmp_drpm;
  VideoParameters *p_Vid = p_Dpb->p_Vid;

  p_Vid->last_has_mmco_5 = 0;

  assert (!p_Vid->currentPicture->idr_flag);
  assert (p_Vid->adaptive_ref_pic_buffering_flag);

  while (p_Vid->dec_ref_pic_marking_buffer)
  {
    tmp_drpm = p_Vid->dec_ref_pic_marking_buffer;
    switch (tmp_drpm->memory_management_control_operation)
    {
      case 0:
        if (tmp_drpm->Next != NULL)
        {
          error ("memory_management_control_operation = 0 not last operation in buffer", 500);
        }
        break;
      case 1:
        mm_unmark_short_term_for_reference(p_Dpb, p, tmp_drpm->difference_of_pic_nums_minus1);
        update_ref_list(p_Dpb);
        break;
      case 2:
        mm_unmark_long_term_for_reference(p_Dpb, p, tmp_drpm->long_term_pic_num);
        update_ltref_list(p_Dpb);
        break;
      case 3:
        mm_assign_long_term_frame_idx(p_Dpb, p, tmp_drpm->difference_of_pic_nums_minus1, tmp_drpm->long_term_frame_idx);
        update_ref_list(p_Dpb);
        update_ltref_list(p_Dpb);
        break;
      case 4:
        mm_update_max_long_term_frame_idx (p_Dpb, tmp_drpm->max_long_term_frame_idx_plus1);
        update_ltref_list(p_Dpb);
        break;
      case 5:
        mm_unmark_all_short_term_for_reference(p_Dpb);
        mm_unmark_all_long_term_for_reference(p_Dpb);
        p_Vid->last_has_mmco_5 = 1;
        break;
      case 6:
        mm_mark_current_picture_long_term(p_Dpb, p, tmp_drpm->long_term_frame_idx);
        check_num_ref(p_Dpb);
        break;
      default:
        error ("invalid memory_management_control_operation in buffer", 500);
    }
    p_Vid->dec_ref_pic_marking_buffer = tmp_drpm->Next;
    free (tmp_drpm);
  }
  if ( p_Vid->last_has_mmco_5 )
  {
    p->pic_num = p->frame_num = 0;

    switch (p->structure)
    {
    case TOP_FIELD:
      {
        p->poc = p->top_poc = p_Vid->toppoc =0;
        break;
      }
    case BOTTOM_FIELD:
      {
        p->poc = p->bottom_poc = p_Vid->bottompoc = 0;
        break;
      }
    case FRAME:
      {
        p->top_poc    -= p->poc;
        p->bottom_poc -= p->poc;

        p_Vid->toppoc = p->top_poc;
        p_Vid->bottompoc = p->bottom_poc;

        p->poc = imin (p->top_poc, p->bottom_poc);
        p_Vid->framepoc = p->poc;
        break;
      }
    }
    p_Vid->ThisPOC = p->poc;
    flush_dpb(p_Dpb, output);
  }
}


/*!
 ************************************************************************
 * \brief
 *    Store a picture in DPB. This includes cheking for space in DPB and
 *    flushing frames.
 *    If we received a frame, we need to check for a new store, if we
 *    got a field, check if it's the second field of an already allocated
 *    store.
 *
 * \param p_Vid
 *    VideoParameters
 * \param p
 *    Picture to be stored
 * \param output
 *    FrameFormat for output
 *
 ************************************************************************
 */
void store_picture_in_dpb(DecodedPictureBuffer *p_Dpb, StorablePicture* p, FrameFormat *output)
{
  VideoParameters *p_Vid = p_Dpb->p_Vid;
  unsigned i;
  int poc, pos;
   
  // diagnostics
  //printf ("Storing (%s) non-ref pic with frame_num #%d\n", (p->type == FRAME)?"FRAME":(p->type == TOP_FIELD)?"TOP_FIELD":"BOTTOM_FIELD", p->pic_num);
  // if frame, check for new store,
  assert (p!=NULL);

  p->used_for_reference = (p_Vid->nal_reference_idc != NALU_PRIORITY_DISPOSABLE);

  p->type = p_Vid->type;

#if (MVC_EXTENSION_ENABLE)
  if(p_Vid->p_Inp->num_of_views==2)
  {
    p->view_id = p_Vid->view_id;

    if(p->structure==FRAME)
    {
      p->inter_view_flag[0] = p->inter_view_flag[1] = p_Vid->inter_view_flag[0];
      p->anchor_pic_flag[0] = p->anchor_pic_flag[1] = p_Vid->anchor_pic_flag[0];
    }
    else
    {
      if(p->structure==TOP_FIELD)
      {
        p->inter_view_flag[0] = p_Vid->inter_view_flag[0];
        p->anchor_pic_flag[0] = p_Vid->anchor_pic_flag[0];
      }
      else	// BOTTOM_FIELD
      {
        p->inter_view_flag[1] = p_Vid->inter_view_flag[1];
        p->anchor_pic_flag[1] = p_Vid->anchor_pic_flag[1];
      }
    }
  }
#endif

  p_Vid->last_has_mmco_5=0;
  p_Vid->last_pic_bottom_field = (p_Vid->structure == BOTTOM_FIELD);

  if (p_Vid->currentPicture->idr_flag)
    idr_memory_management(p_Dpb, p, output);
  else
  {
    // adaptive memory management
    if (p->used_for_reference && (p_Vid->adaptive_ref_pic_buffering_flag))
      adaptive_memory_management(p_Dpb, p, output);
  }

  if ((p->structure==TOP_FIELD)||(p->structure==BOTTOM_FIELD))
  {
    // check for frame store with same pic_number
    if (p_Dpb->last_picture)
    {
      if ((int)p_Dpb->last_picture->frame_num == p->pic_num)
      {
        if (((p->structure==TOP_FIELD)&&(p_Dpb->last_picture->is_used==2))||((p->structure==BOTTOM_FIELD)&&(p_Dpb->last_picture->is_used==1)))
        {
          if ((p->used_for_reference && (p_Dpb->last_picture->is_orig_reference!=0))||
              (!p->used_for_reference && (p_Dpb->last_picture->is_orig_reference==0)))
          {
            insert_picture_in_dpb(p_Vid, p_Dpb->last_picture, p);
            update_ref_list(p_Dpb);
            update_ltref_list(p_Dpb);
            dump_dpb(p_Dpb);
            p_Dpb->last_picture = NULL;
            return;
          }
        }
      }
    }
  }

  // this is a frame or a field which has no stored complementary field

  // sliding window, if necessary
  if ((!p_Vid->currentPicture->idr_flag)&&(p->used_for_reference && (!p_Vid->adaptive_ref_pic_buffering_flag)))
  {
    sliding_window_memory_management(p_Dpb, p);
  }

  // first try to remove unused frames
  if (p_Dpb->used_size==p_Dpb->size)
  {
    remove_unused_frame_from_dpb(p_Dpb);
  }

  // then output frames until one can be removed
  while (p_Dpb->used_size == p_Dpb->size)
  {
    // non-reference frames may be output directly
#if (MVC_EXTENSION_ENABLE)
    if (!p->used_for_reference && (p_Vid->p_Inp->num_of_views==1 || p->view_id==p_Vid->view_id))
#else
    if (!p->used_for_reference)
#endif
    {
      get_smallest_poc(p_Dpb, &poc, &pos);

#if (MVC_EXTENSION_ENABLE)
      if(p_Vid->p_Inp->num_of_views==2)
      {
        if (((-1==pos) || (p->poc < poc)) && ((p->pic_num&0x01)==0))
        {
          direct_output(p_Vid, p, output, p_Vid->p_dec);
          return;
        }
        if (((-1==pos) || (p->poc < poc)) && ((p->pic_num&0x01)==1))
        {
          direct_output(p_Vid, p, output, p_Vid->p_dec2);
          return;
        }
      }
      else
#endif
      {
        if ((-1==pos) || (p->poc < poc))
        {
          direct_output(p_Vid, p, output, p_Vid->p_dec);
          return;
        }
      }
    }
    // flush a frame
    output_one_frame_from_dpb(p_Dpb, output);
  }

  // check for duplicate frame number in short term reference buffer
  if ((p->used_for_reference)&&(!p->is_long_term))
  {
    for (i=0; i<p_Dpb->ref_frames_in_buffer; i++)
    {
      if (p_Dpb->fs_ref[i]->frame_num == p->frame_num)
      {
        error("duplicate frame_num in short-term reference picture buffer", 500);
      }
    }
  }
  // store at end of buffer
  insert_picture_in_dpb(p_Vid, p_Dpb->fs[p_Dpb->used_size],p);

  if (p->structure != FRAME)
  {
    p_Dpb->last_picture = p_Dpb->fs[p_Dpb->used_size];
  }
  else
  {
    p_Dpb->last_picture = NULL;
  }

  p_Dpb->used_size++;

  update_ref_list(p_Dpb);
  update_ltref_list(p_Dpb);

  check_num_ref(p_Dpb);

  dump_dpb(p_Dpb);
}


/*!
 ************************************************************************
 * \brief
 *    Insert the frame picture into the buffer if the top field has already
 *    been stored for the coding decision
 *
 * \param p_Vid
 *    VideoParameters
 * \param p
 *    StorablePicture to be inserted
 * \param output
 *    FrameFormat for output
 *
 ************************************************************************
 */
void replace_top_pic_with_frame(DecodedPictureBuffer *p_Dpb, StorablePicture* p, FrameFormat *output)
{
  VideoParameters *p_Vid = p_Dpb->p_Vid;
  FrameStore* fs = NULL;
  unsigned i, found;

  assert (p!=NULL);
  assert (p->structure==FRAME);

  p->used_for_reference = (p_Vid->nal_reference_idc != NALU_PRIORITY_DISPOSABLE);
  p->type = p_Vid->type;
  // upsample a reference picture
#if (MVC_EXTENSION_ENABLE)
  if (p->used_for_reference || (p_Vid->p_Inp->num_of_views==2 && p_Vid->view_id==0))
#else
  if (p->used_for_reference)
#endif
  {
    if( (p_Vid->p_Inp->separate_colour_plane_flag != 0) )
    {
      UnifiedOneForthPix_JV(p_Vid, 0, p);
      UnifiedOneForthPix_JV(p_Vid, 1, p);
      UnifiedOneForthPix_JV(p_Vid, 2, p);
    }
    else
    {
      UnifiedOneForthPix(p_Vid, p);
    }
  }

  found=0;

  for (i = 0; i < p_Dpb->used_size; i++)
  {
    if((p_Dpb->fs[i]->frame_num == p_Vid->frame_num) && (p_Dpb->fs[i]->is_used==1))
    {
      found=1;
      fs = p_Dpb->fs[i];
      break;
    }
  }

  if (!found)
  {
    // this should only happen for non-reference pictures when the dpb is full of reference pics
#if (MVC_EXTENSION_ENABLE)
    if(p_Vid->p_Inp->num_of_views==2)
    {
      if((p->pic_num&0x01)==0)		// view 0
        direct_output_paff(p_Vid, p, output, p_Vid->p_dec);
      else				// view 1
        direct_output_paff(p_Vid, p, output, p_Vid->p_dec2);
    }
    else
#endif
    direct_output_paff(p_Vid, p, output, p_Vid->p_dec);
  }
  else
  {
    free_storable_picture(p_Vid, fs->top_field);
    fs->top_field=NULL;
    fs->frame=p;
    fs->is_used = 3;
    if (p->used_for_reference)
    {
      fs->is_reference = 3;
      if (p->is_long_term)
      {
        fs->is_long_term = 3;
      }
    }
    // generate field views
    dpb_split_field(p_Vid, fs);
    update_ref_list(p_Dpb);
    update_ltref_list(p_Dpb);
  }
}


/*!
 ************************************************************************
 * \brief
 *    Insert the picture into the DPB. A free DPB position is necessary
 *    for frames, .
 *
 * \param p_Vid
 *    VideoParameters
 * \param fs
 *    FrameStore into which the picture will be inserted
 * \param p
 *    StorablePicture to be inserted
 *
 ************************************************************************
 */
static void insert_picture_in_dpb(VideoParameters *p_Vid, FrameStore* fs, StorablePicture* p)
{
  //  printf ("insert (%s) pic with frame_num #%d, poc %d\n", (p->structure == FRAME)?"FRAME":(p->structure == TOP_FIELD)?"TOP_FIELD":"BOTTOM_FIELD", p->pic_num, p->poc);
  assert (p!=NULL);
  assert (fs!=NULL);

  // upsample a reference picture
  if (p->used_for_reference)
  {    
    if( (p_Vid->p_Inp->separate_colour_plane_flag != 0) )
    {
      UnifiedOneForthPix_JV(p_Vid, 0, p);
      UnifiedOneForthPix_JV(p_Vid, 1, p);
      UnifiedOneForthPix_JV(p_Vid, 2, p);
    }
    else
    {
      UnifiedOneForthPix(p_Vid, p);
    }
  }
#if (MVC_EXTENSION_ENABLE)
  else if(p_Vid->p_Inp->num_of_views==2 && p->inter_view_flag[0])
  {
    UnifiedOneForthPix(p_Vid, p);
  }
#endif

  switch (p->structure)
  {
  case FRAME:
    fs->frame = p;
    fs->is_used = 3;
    if (p->used_for_reference)
    {
      fs->is_reference = 3;
      fs->is_orig_reference = 3;
      if (p->is_long_term)
      {
        fs->is_long_term = 3;
        fs->long_term_frame_idx = p->long_term_frame_idx;
      }
    }
    // generate field views
    dpb_split_field(p_Vid, fs);
#if (MVC_EXTENSION_ENABLE)
    if(p_Vid->p_Inp->num_of_views==2)
    {
      fs->anchor_pic_flag[0] = fs->anchor_pic_flag[1] = p->anchor_pic_flag[0];
      fs->inter_view_flag[0] = fs->inter_view_flag[1] = p->inter_view_flag[0];
    }
#endif
    break;
  case TOP_FIELD:
    fs->top_field = p;
    fs->is_used |= 1;
    if (p->used_for_reference)
    {
      fs->is_reference |= 1;
      fs->is_orig_reference |= 1;
      if (p->is_long_term)
      {
        fs->is_long_term |= 1;
        fs->long_term_frame_idx = p->long_term_frame_idx;
      }
    }
    if (fs->is_used == 3)
    {
      // generate frame view
      dpb_combine_field(p_Vid, fs);
    }
    else
    {
      fs->poc = p->poc;
      gen_field_ref_ids(p);
    }
#if (MVC_EXTENSION_ENABLE)
    if(p_Vid->p_Inp->num_of_views==2)
    {
      fs->anchor_pic_flag[0] = p->anchor_pic_flag[0];
      fs->inter_view_flag[0] = p->inter_view_flag[0];
    }
#endif
    break;
  case BOTTOM_FIELD:
    fs->bottom_field = p;
    fs->is_used |= 2;
    if (p->used_for_reference)
    {
      fs->is_reference |= 2;
      fs->is_orig_reference |= 2;
      if (p->is_long_term)
      {
        fs->is_long_term |= 2;
        fs->long_term_frame_idx = p->long_term_frame_idx;
      }
    }
    if (fs->is_used == 3)
    {
      // generate frame view
      dpb_combine_field(p_Vid, fs);
    }
    else
    {
      fs->poc = p->poc;
      gen_field_ref_ids(p);
    }
#if (MVC_EXTENSION_ENABLE)
    if(p_Vid->p_Inp->num_of_views==2)
    {
      fs->anchor_pic_flag[1] = p->anchor_pic_flag[1];
      fs->inter_view_flag[1] = p->inter_view_flag[1];
    }
#endif
    break;
  }
  fs->frame_num = p->pic_num;
  fs->is_output = p->is_output;
#if (MVC_EXTENSION_ENABLE)
  fs->view_id = p->view_id;
#endif
}

/*!
 ************************************************************************
 * \brief
 *    Check if one of the frames/fields in frame store is used for reference
 ************************************************************************
 */
static int is_used_for_reference(FrameStore* fs)
{
  if (fs->is_reference)
  {
    return 1;
  }

  if (fs->is_used == 3) // frame
  {
    if (fs->frame->used_for_reference)
    {
      return 1;
    }
  }

  if (fs->is_used & 1) // top field
  {
    if (fs->top_field)
    {
      if (fs->top_field->used_for_reference)
      {
        return 1;
      }
    }
  }

  if (fs->is_used & 2) // bottom field
  {
    if (fs->bottom_field)
    {
      if (fs->bottom_field->used_for_reference)
      {
        return 1;
      }
    }
  }
  return 0;
}


/*!
 ************************************************************************
 * \brief
 *    Check if one of the frames/fields in frame store is used for short-term reference
 ************************************************************************
 */
static int is_short_term_reference(FrameStore* fs)
{

  if (fs->is_used==3) // frame
  {
    if ((fs->frame->used_for_reference)&&(!fs->frame->is_long_term))
    {
      return 1;
    }
  }

  if (fs->is_used & 1) // top field
  {
    if (fs->top_field)
    {
      if ((fs->top_field->used_for_reference)&&(!fs->top_field->is_long_term))
      {
        return 1;
      }
    }
  }

  if (fs->is_used & 2) // bottom field
  {
    if (fs->bottom_field)
    {
      if ((fs->bottom_field->used_for_reference)&&(!fs->bottom_field->is_long_term))
      {
        return 1;
      }
    }
  }
  return 0;
}


/*!
 ************************************************************************
 * \brief
 *    Check if one of the frames/fields in frame store is used for short-term reference
 ************************************************************************
 */
static int is_long_term_reference(FrameStore* fs)
{

  if (fs->is_used==3) // frame
  {
    if ((fs->frame->used_for_reference)&&(fs->frame->is_long_term))
    {
      return 1;
    }
  }

  if (fs->is_used & 1) // top field
  {
    if (fs->top_field)
    {
      if ((fs->top_field->used_for_reference)&&(fs->top_field->is_long_term))
      {
        return 1;
      }
    }
  }

  if (fs->is_used & 2) // bottom field
  {
    if (fs->bottom_field)
    {
      if ((fs->bottom_field->used_for_reference)&&(fs->bottom_field->is_long_term))
      {
        return 1;
      }
    }
  }
  return 0;
}


/*!
 ************************************************************************
 * \brief
 *    remove one frame from DPB
 ************************************************************************
 */
static void remove_frame_from_dpb(DecodedPictureBuffer *p_Dpb, int pos)
{  
  VideoParameters *p_Vid = p_Dpb->p_Vid;
  FrameStore* fs = p_Dpb->fs[pos];
  FrameStore* tmp;
  unsigned i;

//  printf ("remove frame with frame_num #%d\n", fs->frame_num);
  switch (fs->is_used)
  {
  case 3:
    free_storable_picture(p_Vid, fs->frame);
    free_storable_picture(p_Vid, fs->top_field);
    free_storable_picture(p_Vid, fs->bottom_field);
    fs->frame=NULL;
    fs->top_field=NULL;
    fs->bottom_field=NULL;
    break;
  case 2:
    free_storable_picture(p_Vid, fs->bottom_field);
    fs->bottom_field=NULL;
    break;
  case 1:
    free_storable_picture(p_Vid, fs->top_field);
    fs->top_field=NULL;
    break;
  case 0:
    break;
  default:
    error("invalid frame store type",500);
  }
  fs->is_used = 0;
  fs->is_long_term = 0;
  fs->is_reference = 0;
  fs->is_orig_reference = 0;

  // move empty framestore to end of buffer
  tmp = p_Dpb->fs[pos];

  for (i=pos; i<p_Dpb->used_size-1;i++)
  {
    p_Dpb->fs[i] = p_Dpb->fs[i+1];
  }
  p_Dpb->fs[p_Dpb->used_size-1] = tmp;
  p_Dpb->used_size--;
}

/*!
 ************************************************************************
 * \brief
 *    find smallest POC in the DPB.
 ************************************************************************
 */
static void get_smallest_poc(DecodedPictureBuffer *p_Dpb, int *poc,int * pos)
{
  unsigned i;

  if (p_Dpb->used_size<1)
  {
    error("Cannot determine smallest POC, DPB empty.",150);
  }

  *pos=-1;
  *poc = INT_MAX;
  for (i = 0; i < p_Dpb->used_size; i++)
  {
    if ((*poc>p_Dpb->fs[i]->poc)&&(!p_Dpb->fs[i]->is_output))
    {
      *poc = p_Dpb->fs[i]->poc;
      *pos=i;

    }
  }
}

/*!
 ************************************************************************
 * \brief
 *    Remove a picture from DPB which is no longer needed.
 ************************************************************************
 */
static int flush_unused_frame_from_dpb(DecodedPictureBuffer *p_Dpb)
{
  unsigned i;

  // check for frames that were already output and no longer used for reference
  for (i = 0; i < p_Dpb->used_size; i++)
  {
    if (p_Dpb->fs[i]->is_output && (!is_used_for_reference(p_Dpb->fs[i])))
    {
      remove_frame_from_dpb(p_Dpb, i);
      return 1;
    }
  }
  return 0;
}

/*!
 ************************************************************************
 * \brief
 *    Remove a picture from DPB which is no longer needed.
 ************************************************************************
 */
static int remove_unused_frame_from_dpb(DecodedPictureBuffer *p_Dpb)
{
  unsigned i;

  // check for frames that were already output and no longer used for reference
  for (i = 0; i < p_Dpb->used_size; i++)
  {
    if (p_Dpb->fs[i]->is_output && (!is_used_for_reference(p_Dpb->fs[i])))
    {
      remove_frame_from_dpb(p_Dpb, i);
      return 1;
    }
  }
  return 0;
}

/*!
 ************************************************************************
 * \brief
 *    Output one picture stored in the DPB.
 ************************************************************************
 */
static void output_one_frame_from_dpb(DecodedPictureBuffer *p_Dpb, FrameFormat *output)
{
  VideoParameters *p_Vid = p_Dpb->p_Vid;
  InputParameters *p_Inp = p_Dpb->p_Inp;
  int poc, pos;
  //diagnostics
  if (p_Dpb->used_size<1)
  {
    error("Cannot output frame, DPB empty.",150);
  }

  // find smallest POC
  get_smallest_poc(p_Dpb, &poc, &pos);

  if(pos==-1)
  {
    error("no frames for output available", 150);
  }

  // call the output function
  //  printf ("output frame with frame_num #%d, poc %d (p_Dpb-> p_Dpb->size=%d, p_Dpb->used_size=%d)\n", p_Dpb->fs[pos]->frame_num, p_Dpb->fs[pos]->frame->poc, p_Dpb->size, p_Dpb->used_size);
#if (MVC_EXTENSION_ENABLE)
  if(p_Inp->num_of_views==2)
  {
    if(p_Dpb->fs[pos]->view_id==1)
      write_stored_frame(p_Vid, p_Dpb->fs[pos], output, p_Vid->p_dec2);
    else
      write_stored_frame(p_Vid, p_Dpb->fs[pos], output, p_Vid->p_dec);
  }
  else
#endif
    write_stored_frame(p_Vid, p_Dpb->fs[pos], output, p_Vid->p_dec);

  // if redundant picture in use, output POC may be not in ascending order
  if(p_Inp->redundant_pic_flag == 0)
  {
#if (MVC_EXTENSION_ENABLE)
    if (p_Dpb->last_output_poc >= poc && p_Dpb->fs[pos]->view_id == p_Dpb->last_output_view_id)
#else
    if (p_Dpb->last_output_poc >= poc)
#endif
    {
      error ("output POC must be in ascending order", 150);
    }
  }
  p_Dpb->last_output_poc = poc;
#if (MVC_EXTENSION_ENABLE)
  p_Dpb->last_output_view_id = p_Dpb->fs[pos]->view_id;
#endif

  // free frame store and move empty store to end of buffer
  if (!is_used_for_reference(p_Dpb->fs[pos]))
  {
    remove_frame_from_dpb(p_Dpb, pos);
  }
}



/*!
 ************************************************************************
 * \brief
 *    All stored picture are output. Should be called to empty the buffer
 ************************************************************************
 */
void flush_dpb(DecodedPictureBuffer *p_Dpb, FrameFormat *output)
{
  unsigned i;

  // diagnostics
  // printf("Flush remaining frames from the dpb. p_Dpb->size=%d, p_Dpb->used_size=%d\n",p_Dpb->size,p_Dpb->used_size);

  // mark all frames unused
  //VideoParameters *p_Vid = p_Dpb->p_Vid;
  for (i=0; i<p_Dpb->used_size; i++)
  {
    unmark_for_reference (p_Dpb->fs[i]);
  }

  while (flush_unused_frame_from_dpb(p_Dpb)) ;

  // output frames in POC order
  while (p_Dpb->used_size)
  {
    output_one_frame_from_dpb(p_Dpb, output);
  }

  p_Dpb->last_output_poc = INT_MIN;
#if (MVC_EXTENSION_ENABLE)
  p_Dpb->last_output_view_id = 0;
#endif
}


static void gen_field_ref_ids(StorablePicture *p)
{
  int i,j;
   //! Generate Frame parameters from field information.
  for (i = 0; i < (p->size_x >> 2); i++)
  {
    for (j = 0; j < (p->size_y >> 2); j++)
    {
        p->mv_info[j][i].field_frame=1;
    }
  }
}

/*!
 ************************************************************************
 * \brief
 *    Extract top field from a frame
 ************************************************************************
 */
void dpb_split_field(VideoParameters *p_Vid, FrameStore *fs)
{
  int i, j, k, ii, jj, jj4;
  int idiv,jdiv;
  int currentmb;
  int dummylist0,dummylist1;
  int twosz16 = 2*(fs->frame->size_x>>4);
  StorablePicture *frame = fs->frame;

  fs->poc = frame->poc;

  if (!p_Vid->active_sps->frame_mbs_only_flag)
  {
    fs->top_field    = alloc_storable_picture(p_Vid, TOP_FIELD,    frame->size_x, frame->size_y >> 1, frame->size_x_cr, frame->size_y_cr >> 1);
    fs->bottom_field = alloc_storable_picture(p_Vid, BOTTOM_FIELD, frame->size_x, frame->size_y >> 1, frame->size_x_cr, frame->size_y_cr >> 1);

    for (i = 0; i < (frame->size_y >> 1); i++)
    {
      memcpy(fs->top_field->imgY[i], frame->imgY[i*2], frame->size_x*sizeof(imgpel));
      memcpy(fs->bottom_field->imgY[i], frame->imgY[i*2 + 1], frame->size_x*sizeof(imgpel));
    }

    for (k = 0; k < 2; k++)
    {
      for (i=0; i< (frame->size_y_cr >> 1); i++)
      {
        memcpy(fs->top_field->imgUV[k][i], frame->imgUV[k][i*2], frame->size_x_cr*sizeof(imgpel));
        memcpy(fs->bottom_field->imgUV[k][i], frame->imgUV[k][i*2 + 1], frame->size_x_cr*sizeof(imgpel));
      }
    }
    if(frame->used_for_reference)
    {
      if( (p_Vid->p_Inp->separate_colour_plane_flag != 0) )
      {
        UnifiedOneForthPix_JV(p_Vid, 0, fs->top_field);
        UnifiedOneForthPix_JV(p_Vid, 1, fs->top_field);
        UnifiedOneForthPix_JV(p_Vid, 2, fs->top_field);
        UnifiedOneForthPix_JV(p_Vid, 0, fs->bottom_field);
        UnifiedOneForthPix_JV(p_Vid, 1, fs->bottom_field);
        UnifiedOneForthPix_JV(p_Vid, 2, fs->bottom_field);
      }
      else
      {
        UnifiedOneForthPix(p_Vid, fs->top_field);
        UnifiedOneForthPix(p_Vid, fs->bottom_field);
      }
    }
    fs->top_field->poc = frame->top_poc;
    fs->bottom_field->poc =  frame->bottom_poc;

    fs->top_field->frame_poc =  frame->frame_poc;

    fs->top_field->bottom_poc =fs->bottom_field->bottom_poc =  frame->bottom_poc;
    fs->top_field->top_poc =fs->bottom_field->top_poc =  frame->top_poc;
    fs->bottom_field->frame_poc =  frame->frame_poc;

    fs->top_field->used_for_reference = fs->bottom_field->used_for_reference
      = frame->used_for_reference;
    fs->top_field->is_long_term = fs->bottom_field->is_long_term
      = frame->is_long_term;
    fs->long_term_frame_idx = fs->top_field->long_term_frame_idx
      = fs->bottom_field->long_term_frame_idx
      = frame->long_term_frame_idx;

    fs->top_field->coded_frame = fs->bottom_field->coded_frame = 1;
    fs->top_field->mb_aff_frame_flag = fs->bottom_field->mb_aff_frame_flag
      = frame->mb_aff_frame_flag;

    frame->top_field    = fs->top_field;
    frame->bottom_field = fs->bottom_field;
    frame->frame = frame;
    fs->top_field->bottom_field = fs->bottom_field;
    fs->top_field->frame        = fs->frame;
    fs->top_field->top_field = fs->top_field;
    fs->bottom_field->top_field = fs->top_field;
    fs->bottom_field->frame     = frame;
    fs->bottom_field->bottom_field = fs->bottom_field;

    fs->top_field->chroma_format_idc = fs->bottom_field->chroma_format_idc = frame->chroma_format_idc;
    fs->top_field->chroma_mask_mv_x  = fs->bottom_field->chroma_mask_mv_x  = frame->chroma_mask_mv_x;
    fs->top_field->chroma_mask_mv_y  = fs->bottom_field->chroma_mask_mv_y  = frame->chroma_mask_mv_y;
    fs->top_field->chroma_shift_x    = fs->bottom_field->chroma_shift_x    = frame->chroma_shift_x;
    fs->top_field->chroma_shift_y    = fs->bottom_field->chroma_shift_y    = frame->chroma_shift_y;

  }
  else
  {
    fs->top_field=NULL;
    fs->bottom_field=NULL;
    frame->top_field=NULL;
    frame->bottom_field=NULL;
    frame->frame = frame;
  }


  if (!p_Vid->active_sps->frame_mbs_only_flag && fs->frame->mb_aff_frame_flag)
  {
    for (j=0 ; j< frame->size_y >> 3; j++)
    {
      jj = (j >> 2)*8 + (j & 0x03);
      jj4 = jj + 4;
      jdiv = (j >> 1);
      for (i=0 ; i < (frame->size_x >> 2); i++)
      {
        idiv=i >> 2;

        currentmb = twosz16*(jdiv >> 1)+ (idiv)*2 + (jdiv & 0x01);
        // Assign field mvs attached to MB-Frame buffer to the proper buffer
        if (fs->frame->motion.mb_field[currentmb])
        {
          fs->bottom_field->mv_info[j][i].field_frame = fs->top_field->mv_info[j][i].field_frame=1;
          fs->frame->mv_info[2*j][i].field_frame = fs->frame->mv_info[2*j+1][i].field_frame = 1;

          fs->bottom_field->mv_info[j][i].mv[LIST_0] = fs->frame->mv_info[jj4][i].mv[LIST_0];
          fs->bottom_field->mv_info[j][i].mv[LIST_1] = fs->frame->mv_info[jj4][i].mv[LIST_1];
          fs->bottom_field->mv_info[j][i].ref_idx[LIST_0] = fs->frame->mv_info[jj4][i].ref_idx[LIST_0];
          /*
          if(fs->bottom_field->mv_info[j][i].ref_idx[LIST_0] >=0)
            fs->bottom_field->mv_info[j][i].ref_pic[LIST_0] = currSlice->listX[4][(short) fs->bottom_field->mv_info[j][i].ref_idx[LIST_0]];
          else
            fs->bottom_field->mv_info[j][i].ref_pic[LIST_0] = NULL;
            */
          fs->bottom_field->mv_info[j][i].ref_idx[LIST_1] = fs->frame->mv_info[jj4][i].ref_idx[LIST_1];
          /*
          if(fs->bottom_field->mv_info[j][i].ref_idx[LIST_1] >=0)
            fs->bottom_field->mv_info[j][i].ref_pic[LIST_1] = currSlice->listX[5][(short) fs->bottom_field->mv_info[j][i].ref_idx[LIST_1]];
          else
            fs->bottom_field->mv_info[j][i].ref_pic[LIST_1] = NULL;
            */

          fs->top_field->mv_info[j][i].mv[LIST_0] = fs->frame->mv_info[jj][i].mv[LIST_0];
          fs->top_field->mv_info[j][i].mv[LIST_1] = fs->frame->mv_info[jj][i].mv[LIST_1];
          fs->top_field->mv_info[j][i].ref_idx[LIST_0] = fs->frame->mv_info[jj][i].ref_idx[LIST_0];
          /*
          if(fs->top_field->mv_info[j][i].ref_idx[LIST_0] >=0)
            fs->top_field->mv_info[j][i].ref_pic[LIST_0] = currSlice->listX[2][(short) fs->top_field->mv_info[j][i].ref_idx[LIST_0]];
          else
            fs->top_field->mv_info[j][i].ref_pic[LIST_0] = NULL;
            */
          fs->top_field->mv_info[j][i].ref_idx[LIST_1] = fs->frame->mv_info[jj][i].ref_idx[LIST_1];
          /*
          if(fs->top_field->mv_info[j][i].ref_idx[LIST_1] >=0)
            fs->top_field->mv_info[j][i].ref_pic[LIST_1] = currSlice->listX[3][(short) fs->top_field->mv_info[j][i].ref_idx[LIST_1]];
          else
            fs->top_field->mv_info[j][i].ref_pic[LIST_1] = NULL;
            */
        }
      }
    }
  }

  //! Generate field MVs from Frame MVs
  if (!p_Vid->active_sps->frame_mbs_only_flag)
  {
    for (j=0 ; j<fs->frame->size_y >> 3 ; j++)
    {
      jj = 2* RSD(j);
      jdiv = j >> 1;
      for (i=0 ; i<fs->frame->size_x >> 2 ; i++)
      {
        ii = RSD(i);
        idiv = i >> 2;

        currentmb = twosz16*(jdiv >> 1)+ (idiv)*2 + (jdiv & 0x01);

        if (!fs->frame->mb_aff_frame_flag  || !fs->frame->motion.mb_field[currentmb])
        {
          fs->frame->mv_info[2*j+1][i].field_frame = fs->frame->mv_info[2*j][i].field_frame = 0;

          fs->top_field->mv_info[j][i].field_frame = fs->bottom_field->mv_info[j][i].field_frame = 0;

          fs->top_field->mv_info[j][i].mv[LIST_0] = fs->bottom_field->mv_info[j][i].mv[LIST_0] = fs->frame->mv_info[jj][ii].mv[LIST_0];
          fs->top_field->mv_info[j][i].mv[LIST_1] = fs->bottom_field->mv_info[j][i].mv[LIST_1] = fs->frame->mv_info[jj][ii].mv[LIST_1];

          // Scaling of references is done here since it will not affect spatial direct (2*0 =0)
          if (fs->frame->mv_info[jj][ii].ref_idx[LIST_0] == -1)
            fs->top_field->mv_info[j][i].ref_idx[LIST_0] = fs->bottom_field->mv_info[j][i].ref_idx[LIST_0] = - 1;
          else
          {
            dummylist0=fs->top_field->mv_info[j][i].ref_idx[LIST_0] = fs->bottom_field->mv_info[j][i].ref_idx[LIST_0] = fs->frame->mv_info[jj][ii].ref_idx[LIST_0];
            //fs->top_field->mv_info[j][i].ref_pic[LIST_0] = fs->bottom_field->mv_info[j][i].ref_pic[LIST_0] = currSlice->listX[LIST_0][fs->frame->mv_info[jj][ii].ref_idx[LIST_0]];
          }

          if (fs->frame->mv_info[jj][ii].ref_idx[LIST_1] == -1)
            fs->top_field->mv_info[j][i].ref_idx[LIST_1] = fs->bottom_field->mv_info[j][i].ref_idx[LIST_1] = - 1;
          else
          {
            dummylist1=fs->top_field->mv_info[j][i].ref_idx[LIST_1] = fs->bottom_field->mv_info[j][i].ref_idx[LIST_1] = fs->frame->mv_info[jj][ii].ref_idx[LIST_1];
            //fs->top_field->mv_info[j][i].ref_pic[LIST_1] = fs->bottom_field->mv_info[j][i].ref_pic[LIST_1] =  currSlice->listX[LIST_1][fs->frame->mv_info[jj][ii].ref_idx[LIST_1]];
          }
        }
        else
        {
          fs->frame->mv_info[2*j+1][i].field_frame = fs->frame->mv_info[2*j][i].field_frame= fs->frame->motion.mb_field[currentmb];
        }
      }
    }
  }
  else
  {
    for (j=0 ; j<fs->frame->size_y >> 2 ; j++)
    {
      for (i=0 ; i<fs->frame->size_x >> 2 ; i++)
      {
        frame->mv_info[j][i].field_frame = 0;
      }
    }
  }
}


/*!
 ************************************************************************
 * \brief
 *    Generate a frame from top and bottom fields,
 *    YUV components and display information only
 ************************************************************************
 */
void dpb_combine_field_yuv(VideoParameters *p_Vid, FrameStore *fs)
{
  int i, j;

  fs->frame = alloc_storable_picture(p_Vid, FRAME, fs->top_field->size_x, fs->top_field->size_y*2, fs->top_field->size_x_cr, fs->top_field->size_y_cr*2);

  for (i=0; i<fs->top_field->size_y; i++)
  {
    memcpy(fs->frame->imgY[i*2],     fs->top_field->imgY[i]   , fs->top_field->size_x * sizeof(imgpel));     // top field
    memcpy(fs->frame->imgY[i*2 + 1], fs->bottom_field->imgY[i], fs->bottom_field->size_x * sizeof(imgpel)); // bottom field
  }

  for (j = 0; j < 2; j++)
  {
    for (i=0; i<fs->top_field->size_y_cr; i++)
    {
      memcpy(fs->frame->imgUV[j][i*2],     fs->top_field->imgUV[j][i],    fs->top_field->size_x_cr*sizeof(imgpel));
      memcpy(fs->frame->imgUV[j][i*2 + 1], fs->bottom_field->imgUV[j][i], fs->bottom_field->size_x_cr*sizeof(imgpel));
    }
  }

  fs->poc=fs->frame->poc =fs->frame->frame_poc = imin (fs->top_field->poc, fs->bottom_field->poc);

  fs->bottom_field->frame_poc=fs->top_field->frame_poc=fs->frame->poc;

  fs->bottom_field->top_poc=fs->frame->top_poc=fs->top_field->poc;
  fs->top_field->bottom_poc=fs->frame->bottom_poc=fs->bottom_field->poc;

  fs->frame->used_for_reference = (fs->top_field->used_for_reference && fs->bottom_field->used_for_reference );
  fs->frame->is_long_term = (fs->top_field->is_long_term && fs->bottom_field->is_long_term );

  if (fs->frame->is_long_term)
    fs->frame->long_term_frame_idx = fs->long_term_frame_idx;

  fs->frame->top_field    = fs->top_field;
  fs->frame->bottom_field = fs->bottom_field;
  fs->frame->frame = fs->frame;

  fs->frame->coded_frame = 0;

  fs->frame->chroma_format_idc = fs->top_field->chroma_format_idc;
  fs->frame->chroma_mask_mv_x  = fs->top_field->chroma_mask_mv_x;
  fs->frame->chroma_mask_mv_y  = fs->top_field->chroma_mask_mv_y;
  fs->frame->chroma_shift_x    = fs->top_field->chroma_shift_x;
  fs->frame->chroma_shift_y    = fs->top_field->chroma_shift_y;

  fs->frame->frame_cropping_flag = fs->top_field->frame_cropping_flag;
  if (fs->frame->frame_cropping_flag)
  {
    fs->frame->frame_cropping_rect_top_offset = fs->top_field->frame_cropping_rect_top_offset;
    fs->frame->frame_cropping_rect_bottom_offset = fs->top_field->frame_cropping_rect_bottom_offset;
    fs->frame->frame_cropping_rect_left_offset = fs->top_field->frame_cropping_rect_left_offset;
    fs->frame->frame_cropping_rect_right_offset = fs->top_field->frame_cropping_rect_right_offset;
  }

  fs->top_field->frame = fs->bottom_field->frame = fs->frame;
  fs->top_field->top_field = fs->top_field;
  fs->top_field->bottom_field = fs->bottom_field;
  fs->bottom_field->top_field = fs->top_field;
  fs->bottom_field->bottom_field = fs->bottom_field;
}


/*!
 ************************************************************************
 * \brief
 *    Generate a frame from top and bottom fields
 ************************************************************************
 */
void dpb_combine_field(VideoParameters *p_Vid, FrameStore *fs)
{
  int i,j, jj, jj4;
  int dummylist0, dummylist1;

  dpb_combine_field_yuv(p_Vid, fs);
  if (fs->frame->used_for_reference)
  {
    if( (p_Vid->p_Inp->separate_colour_plane_flag != 0) )
    {
      UnifiedOneForthPix_JV(p_Vid, 0, fs->frame);
      UnifiedOneForthPix_JV(p_Vid, 1, fs->frame);
      UnifiedOneForthPix_JV(p_Vid, 2, fs->frame);
    }
    else
    {
      UnifiedOneForthPix(p_Vid, fs->frame);
    }
  }

  //! Generate Frame parameters from field information.
  for (j=0 ; j<fs->top_field->size_y >> 2 ; j++)
  {
    jj = 8*(j >> 2) + (j & 0x03);
    jj4 = jj + 4;
    for (i=0 ; i<fs->top_field->size_x >> 2 ; i++)
    {
      fs->frame->mv_info[jj][i].field_frame= fs->frame->mv_info[jj4][i].field_frame = 1;

      fs->frame->mv_info[jj][i].mv[LIST_0] = fs->top_field->mv_info[j][i].mv[LIST_0];
      fs->frame->mv_info[jj][i].mv[LIST_1] = fs->top_field->mv_info[j][i].mv[LIST_1];

      dummylist0=fs->frame->mv_info[jj][i].ref_idx[LIST_0] = fs->top_field->mv_info[j][i].ref_idx[LIST_0];
      dummylist1=fs->frame->mv_info[jj][i].ref_idx[LIST_1] = fs->top_field->mv_info[j][i].ref_idx[LIST_1];


      fs->frame->mv_info[jj4][i].mv[LIST_0] = fs->bottom_field->mv_info[j][i].mv[LIST_0];
      fs->frame->mv_info[jj4][i].mv[LIST_1] = fs->bottom_field->mv_info[j][i].mv[LIST_1];

      dummylist0 = fs->frame->mv_info[jj4][i].ref_idx[LIST_0]  = fs->bottom_field->mv_info[j][i].ref_idx[LIST_0];
      dummylist1 = fs->frame->mv_info[jj4][i].ref_idx[LIST_1]  = fs->bottom_field->mv_info[j][i].ref_idx[LIST_1];

      fs->top_field->mv_info[j][i].field_frame = 1;
      fs->bottom_field->mv_info[j][i].field_frame = 1;
    }
  }
}


/*!
 ************************************************************************
 * \brief
 *    Allocate memory for buffering of reference picture reordering commands
 ************************************************************************
 */
void alloc_ref_pic_list_reordering_buffer(Slice *currSlice)
{  
  if (currSlice->slice_type!=I_SLICE && currSlice->slice_type != SI_SLICE)
  {
    int size = currSlice->num_ref_idx_active[LIST_0] + 1;
    if ((currSlice->reordering_of_pic_nums_idc[LIST_0] = calloc(size, sizeof(int)))==NULL) 
      no_mem_exit("alloc_ref_pic_list_reordering_buffer: remapping_of_pic_nums_idc_l0");
    if ((currSlice->abs_diff_pic_num_minus1[LIST_0] = calloc(size, sizeof(int)))==NULL) 
      no_mem_exit("alloc_ref_pic_list_reordering_buffer: abs_diff_pic_num_minus1_l0");
    if ((currSlice->long_term_pic_idx[LIST_0] = calloc(size, sizeof(int)))==NULL)
      no_mem_exit("alloc_ref_pic_list_reordering_buffer: long_term_pic_idx_l0");
  }
  else
  {
    currSlice->reordering_of_pic_nums_idc[LIST_0] = NULL;
    currSlice->abs_diff_pic_num_minus1[LIST_0] = NULL;
    currSlice->long_term_pic_idx[LIST_0] = NULL;
  }

  if (currSlice->slice_type == B_SLICE)
  {
    int size = currSlice->num_ref_idx_active[LIST_1] + 1;
    if ((currSlice->reordering_of_pic_nums_idc[LIST_1] = calloc(size, sizeof(int)))==NULL) 
      no_mem_exit("alloc_ref_pic_list_reordering_buffer: remapping_of_pic_nums_idc_l1");
    if ((currSlice->abs_diff_pic_num_minus1[LIST_1] = calloc(size, sizeof(int)))==NULL) 
      no_mem_exit("alloc_ref_pic_list_reordering_buffer: abs_diff_pic_num_minus1_l1");
    if ((currSlice->long_term_pic_idx[LIST_1] = calloc(size, sizeof(int)))==NULL) 
      no_mem_exit("alloc_ref_pic_list_reordering_buffer: long_term_pic_idx_l1");
  }
  else
  {
    currSlice->reordering_of_pic_nums_idc[LIST_1] = NULL;
    currSlice->abs_diff_pic_num_minus1[LIST_1] = NULL;
    currSlice->long_term_pic_idx[LIST_1] = NULL;
  }
}


/*!
 ************************************************************************
 * \brief
 *    Free memory for buffering of reference picture reordering commands
 ************************************************************************
 */
void free_ref_pic_list_reordering_buffer(Slice *currSlice)
{
  if (currSlice->reordering_of_pic_nums_idc[LIST_0])
    free(currSlice->reordering_of_pic_nums_idc[LIST_0]);
  if (currSlice->abs_diff_pic_num_minus1[LIST_0])
    free(currSlice->abs_diff_pic_num_minus1[LIST_0]);
  if (currSlice->long_term_pic_idx[LIST_0])
    free(currSlice->long_term_pic_idx[LIST_0]);

  currSlice->reordering_of_pic_nums_idc[LIST_0] = NULL;
  currSlice->abs_diff_pic_num_minus1[LIST_0] = NULL;
  currSlice->long_term_pic_idx[LIST_0] = NULL;

  if (currSlice->reordering_of_pic_nums_idc[LIST_1])
    free(currSlice->reordering_of_pic_nums_idc[LIST_1]);
  if (currSlice->abs_diff_pic_num_minus1[LIST_1])
    free(currSlice->abs_diff_pic_num_minus1[LIST_1]);
  if (currSlice->long_term_pic_idx[LIST_1])
    free(currSlice->long_term_pic_idx[LIST_1]);

  currSlice->reordering_of_pic_nums_idc[LIST_1] = NULL;
  currSlice->abs_diff_pic_num_minus1[LIST_1] = NULL;
  currSlice->long_term_pic_idx[LIST_1] = NULL;
}

/*!
 ************************************************************************
 * \brief
 *      Tian Dong
 *          June 13, 2002, Modifed on July 30, 2003
 *
 *      If a gap in frame_num is found, try to fill the gap
 * \param p_Vid
 *    VideoParameters structure
 * \param output
 *    FrameFormat for ouput
 *
 ************************************************************************
 */
void fill_frame_num_gap(VideoParameters *p_Vid, FrameFormat *output)
{
  int CurrFrameNum;
  int UnusedShortTermFrameNum;
  StorablePicture *picture = NULL;
  int nal_ref_idc_bak = p_Vid->nal_reference_idc;

//  printf("A gap in frame number is found, try to fill it.\n");

  
  p_Vid->nal_reference_idc = NALU_PRIORITY_LOW;

  UnusedShortTermFrameNum = (p_Vid->pre_frame_num + 1) % p_Vid->max_frame_num;
  CurrFrameNum = p_Vid->frame_num;

  while (CurrFrameNum != UnusedShortTermFrameNum)
  {
    picture = alloc_storable_picture (p_Vid, FRAME, p_Vid->width, p_Vid->height, p_Vid->width_cr, p_Vid->height_cr);
    picture->coded_frame = 1;
    picture->pic_num = UnusedShortTermFrameNum;
    picture->non_existing = 1;
    picture->is_output = 1;

    p_Vid->adaptive_ref_pic_buffering_flag = 0;

    store_picture_in_dpb(p_Vid->p_Dpb, picture, output);

    picture=NULL;
    UnusedShortTermFrameNum = (UnusedShortTermFrameNum + 1) % p_Vid->max_frame_num;
  }

  p_Vid->nal_reference_idc = nal_ref_idc_bak;
}

/*!
 ************************************************************************
 * \brief
 *    Allocate motion parameter memory for colocated structure
 *
 ************************************************************************
 */
void alloc_motion_params(MotionParams *ftype, int size_y, int size_x)
{
  get_mem3Dint64 (&(ftype->ref_pic_id), 2, size_y, size_x);
  get_mem3Dmv    (&(ftype->mv)        , 2, size_y, size_x);
  get_mem3D      ((byte****)(&(ftype->ref_idx)) , 2, size_y, size_x);
  get_mem2D      (&(ftype->moving_block) , size_y, size_x);
}

/*!
 ************************************************************************
 * \brief
 *    Allocate co-located memory
 *
 * \param size_x
 *    horizontal luma size
 * \param size_y
 *    vertical luma size
 * \param mb_adaptive_frame_field_flag
 *    flag that indicates macroblock adaptive frame/field coding
 *
 * \return
 *    the allocated StorablePicture structure
 ************************************************************************
 */
ColocatedParams* alloc_colocated(int size_x, int size_y, int mb_adaptive_frame_field_flag)
{
  ColocatedParams *s;

  s = calloc(1, sizeof(ColocatedParams));
  if (NULL == s)
    no_mem_exit("alloc_colocated: s");

  s->size_x = size_x;
  s->size_y = size_y;

  alloc_motion_params(&s->frame, size_y / BLOCK_SIZE, size_x / BLOCK_SIZE);

  if (mb_adaptive_frame_field_flag)
  {
    alloc_motion_params(&s->top   , size_y / (BLOCK_SIZE * 2), size_x / BLOCK_SIZE);
    alloc_motion_params(&s->bottom, size_y / (BLOCK_SIZE * 2), size_x / BLOCK_SIZE);
  }

  s->mb_adaptive_frame_field_flag  = mb_adaptive_frame_field_flag;

  return s;
}


/*!
 ************************************************************************
 * \brief
 *    Free motion parameter memory for colocated structure
 *
 ************************************************************************
 */
void free_motion_params(MotionParams *ftype)
{
  free_mem3Dint64 (ftype->ref_pic_id);
  free_mem3D      ((byte***)ftype->ref_idx);
  free_mem3Dmv    (ftype->mv);

  if (ftype->moving_block)
  {
    free_mem2D (ftype->moving_block);
    ftype->moving_block=NULL;
  }
}

/*!
 ************************************************************************
 * \brief
 *    Free co-located memory.
 *
 * \param p
 *    Picture to be freed
 *
 ************************************************************************
 */
void free_colocated(ColocatedParams* p)
{
  if (p)
  {
    free_motion_params(&p->frame);

    if (p->mb_adaptive_frame_field_flag)
    {
      free_motion_params(&p->top   );
      free_motion_params(&p->bottom);
    }

    free(p);

    p=NULL;
  }
}

/*!
 ************************************************************************
 * \brief
 *    Compute co-located motion info
 *
 ************************************************************************
 */

void compute_colocated(Slice *currSlice, StorablePicture **listX[6])
{
  VideoParameters *p_Vid = currSlice->p_Vid;
  int i, j;

  if (currSlice->direct_spatial_mv_pred_flag == 0)
  {
    for (j=0; j<2 + (currSlice->mb_aff_frame_flag * 4);j+=2)
    {
      for (i = 0; i < currSlice->listXsize[j];i++)
      {
        int prescale, iTRb, iTRp;

        if (j==0)
        {
          iTRb = iClip3( -128, 127, p_Vid->enc_picture->poc - listX[LIST_0 + j][i]->poc );
        }
        else if (j == 2)
        {
          iTRb = iClip3( -128, 127, p_Vid->enc_picture->top_poc - listX[LIST_0 + j][i]->poc );
        }
        else
        {
          iTRb = iClip3( -128, 127, p_Vid->enc_picture->bottom_poc - listX[LIST_0 + j][i]->poc );
        }

        iTRp = iClip3( -128, 127,  listX[LIST_1 + j][0]->poc - listX[LIST_0 + j][i]->poc);

        if (iTRp!=0)
        {
          prescale = ( 16384 + iabs( iTRp / 2 ) ) / iTRp;
          currSlice->mvscale[j][i] = iClip3( -1024, 1023, ( iTRb * prescale + 32 ) >> 6 ) ;
        }
        else
        {
          currSlice->mvscale[j][i] = 9999;
        }
      }
    }
  }
}



void ChangeLists(Slice *currSlice)
{
  int ref, currPOC;
  StorablePicture **pRefList;
  assert(currSlice->slice_type == B_SLICE);

  if (currSlice->structure == FRAME)
  {
    currPOC = currSlice->framepoc;
    pRefList = currSlice->listX[LIST_0];
    for(ref=0; ref < currSlice->listXsize[LIST_0]; ref++)
      if(pRefList[ref]->frame_poc > currPOC)
        break;
    currSlice->listXsize[LIST_0] = (char) ref;

    pRefList = currSlice->listX[LIST_1];
    for(ref=0; ref < currSlice->listXsize[LIST_1]; ref++)
      if(pRefList[ref]->frame_poc < currPOC)
        break;
    currSlice->listXsize[LIST_1] = (char) ref;
  }
  else
  {
    currPOC = currSlice->ThisPOC;
    pRefList = currSlice->listX[LIST_0];
    for(ref=0; ref < currSlice->listXsize[LIST_0]; ref++)
      if(pRefList[ref]->poc > currPOC)
        break;
    currSlice->listXsize[LIST_0] = (char) ref;

    pRefList = currSlice->listX[LIST_1];
    for(ref=0; ref < currSlice->listXsize[LIST_1]; ref++)
      if(pRefList[ref]->poc < currPOC)
        break;
    currSlice->listXsize[LIST_1] = (char) ref;
  }

  currSlice->num_ref_idx_active[LIST_0] = currSlice->listXsize[LIST_0];
  currSlice->num_ref_idx_active[LIST_1] = currSlice->listXsize[LIST_1];
}

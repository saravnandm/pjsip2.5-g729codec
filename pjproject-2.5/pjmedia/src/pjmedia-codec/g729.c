/* $Id: gsm.c 3664 2011-07-19 03:42:28Z nanang $ */
/*
   * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
   * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
   *
   * This program is free software; you can redistribute it and/or modify
   * it under the terms of the GNU General Public License as published by
   * the Free Software Foundation; either version 2 of the License, or
   * (at your option) any later version.
   *
   * This program is distributed in the hope that it will be useful,
   * but WITHOUT ANY WARRANTY; without even the implied warranty of
   * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   * GNU General Public License for more details.
   *
   * You should have received a copy of the GNU General Public License
   * along with this program; if not, write to the Free Software
   * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
   */
#include <pjmedia-codec/g729.h>
#include <pjmedia/codec.h>
#include <pjmedia/errno.h>
#include <pjmedia/endpoint.h>
#include <pjmedia/plc.h>
#include <pjmedia/port.h>
#include <pjmedia/silencedet.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/os.h>

/*
   * Only build this file if PJMEDIA_HAS_GSM_CODEC != 0
   */
#include "../../third_party/bcg729/include/bcg729/decoder.h"
#include "../../third_party/bcg729/include/bcg729/encoder.h"

/* We removed PLC in 0.6 (and re-enabled it again in 0.9!) */
#define PLC_DISABLED	0


/* Prototypes for GSM factory */
static pj_status_t g729_test_alloc( pjmedia_codec_factory *factory,
                                    				   const pjmedia_codec_info *id );
static pj_status_t g729_default_attr( pjmedia_codec_factory *factory,
                                      				     const pjmedia_codec_info *id,
                                      				     pjmedia_codec_param *attr );
static pj_status_t g729_enum_codecs( pjmedia_codec_factory *factory,
                                     				    unsigned *count,
                                     				    pjmedia_codec_info codecs[]);
static pj_status_t g729_alloc_codec( pjmedia_codec_factory *factory,
                                     				    const pjmedia_codec_info *id,
                                     				    pjmedia_codec **p_codec);
static pj_status_t g729_dealloc_codec( pjmedia_codec_factory *factory,
                                       				      pjmedia_codec *codec );

/* Prototypes for GSM implementation. */
static pj_status_t  g729_codec_init( pjmedia_codec *codec,
                                     				    pj_pool_t *pool );
static pj_status_t  g729_codec_open( pjmedia_codec *codec,
                                     				    pjmedia_codec_param *attr );
static pj_status_t  g729_codec_close( pjmedia_codec *codec );
static pj_status_t  g729_codec_modify(pjmedia_codec *codec,
                                       				     const pjmedia_codec_param *attr );
static pj_status_t  g729_codec_parse( pjmedia_codec *codec,
                                      				     void *pkt,
                                      				     pj_size_t pkt_size,
                                      				     const pj_timestamp *ts,
                                      				     unsigned *frame_cnt,
                                      				     pjmedia_frame frames[]);
static pj_status_t  g729_codec_encode( pjmedia_codec *codec,
                                       				      const struct pjmedia_frame *input,
                                       				      unsigned output_buf_len,
                                       				      struct pjmedia_frame *output);
static pj_status_t  g729_codec_decode( pjmedia_codec *codec,
                                       				      const struct pjmedia_frame *input,
                                       				      unsigned output_buf_len,
                                       				      struct pjmedia_frame *output);
#if !PLC_DISABLED
static pj_status_t  g729_codec_recover(pjmedia_codec *codec,
                                        				      unsigned output_buf_len,
                                        				      struct pjmedia_frame *output);
#endif

/* Definition for GSM codec operations. */
static pjmedia_codec_op g729_op =
{
        &g729_codec_init,
        &g729_codec_open,
        &g729_codec_close,
        &g729_codec_modify,
        &g729_codec_parse,
        &g729_codec_encode,
        &g729_codec_decode,
    #if !PLC_DISABLED
            &g729_codec_recover
    #else
            NULL
    #endif
};

/* Definition for GSM codec factory operations. */
static pjmedia_codec_factory_op g729_factory_op =
{
        &g729_test_alloc,
        &g729_default_attr,
        &g729_enum_codecs,
        &g729_alloc_codec,
        &g729_dealloc_codec,
        &pjmedia_codec_g729_deinit
};

/* GSM factory */
static struct g729_codec_factory
{
        pjmedia_codec_factory    base;
        pjmedia_endpt	    *endpt;
        pj_pool_t		    *pool;
        pj_mutex_t		    *mutex;
        pjmedia_codec	     codec_list;
} g729_codec_factory;

/* GSM codec private data. */
struct g729_data
{
        bcg729EncoderChannelContextStruct	*encoder;
        bcg729DecoderChannelContextStruct	*decoder;
        pj_bool_t		 plc_enabled;
    #if !PLC_DISABLED
            pjmedia_plc		*plc;
    #endif
        pj_bool_t		 vad_enabled;
        pjmedia_silence_det	*vad;
        pj_timestamp	 last_tx;
};



/*
   * Initialize and register GSM codec factory to pjmedia endpoint.
   */
PJ_DEF(pj_status_t) pjmedia_codec_g729_init( pjmedia_endpt *endpt )
{
        pjmedia_codec_mgr *codec_mgr;
        pj_status_t status;
    
        if (g729_codec_factory.pool != NULL)
        	return PJ_SUCCESS;
    
        /* Create GSM codec factory. */
        g729_codec_factory.base.op = &g729_factory_op;
        g729_codec_factory.base.factory_data = NULL;
        g729_codec_factory.endpt = endpt;
    
        g729_codec_factory.pool = pjmedia_endpt_create_pool(endpt, "g729", 4000,
                                                             						       4000);
        if (!g729_codec_factory.pool)
        	return PJ_ENOMEM;
    
        pj_list_init(&g729_codec_factory.codec_list);
    
        /* Create mutex. */
        status = pj_mutex_create_simple(g729_codec_factory.pool, "g729",
                                         				    &g729_codec_factory.mutex);
        if (status != PJ_SUCCESS)
        	goto on_error;
    
        /* Get the codec manager. */
        codec_mgr = pjmedia_endpt_get_codec_mgr(endpt);
        if (!codec_mgr) {
        	status = PJ_EINVALIDOP;
        	goto on_error;
            }
    
        /* Register codec factory to endpoint. */
        status = pjmedia_codec_mgr_register_factory(codec_mgr,
                                                     						&g729_codec_factory.base);
        if (status != PJ_SUCCESS)
        	goto on_error;
    
        /* Done. */
        return PJ_SUCCESS;
    
    on_error:
        pj_pool_release(g729_codec_factory.pool);
        g729_codec_factory.pool = NULL;
        return status;
    }



/*
   * Unregister GSM codec factory from pjmedia endpoint and deinitialize
   * the GSM codec library.
   */
PJ_DEF(pj_status_t) pjmedia_codec_g729_deinit(void)
{
        pjmedia_codec_mgr *codec_mgr;
        pj_status_t status;
    
        if (g729_codec_factory.pool == NULL)
        	return PJ_SUCCESS;
    
        /* We don't want to deinit if there's outstanding codec. */
        /* This is silly, as we'll always have codec in the list if
                 we ever allocate a codec! A better behavior maybe is to
                 deallocate all codecs in the list.
              pj_mutex_lock(g729_codec_factory.mutex);
              if (!pj_list_empty(&g729_codec_factory.codec_list)) {
          	pj_mutex_unlock(g729_codec_factory.mutex);
          	return PJ_EBUSY;
              }
              */
    
        /* Get the codec manager. */
        codec_mgr = pjmedia_endpt_get_codec_mgr(g729_codec_factory.endpt);
        if (!codec_mgr) {
        	pj_pool_release(g729_codec_factory.pool);
        	g729_codec_factory.pool = NULL;
        	return PJ_EINVALIDOP;
            }
    
        /* Unregister GSM codec factory. */
        status = pjmedia_codec_mgr_unregister_factory(codec_mgr,
                                                       						  &g729_codec_factory.base);
    
        /* Destroy mutex. */
        pj_mutex_destroy(g729_codec_factory.mutex);
    
        /* Destroy pool. */
        pj_pool_release(g729_codec_factory.pool);
        g729_codec_factory.pool = NULL;
    
        return status;
}

/*
   * Check if factory can allocate the specified codec.
   */
static pj_status_t g729_test_alloc( pjmedia_codec_factory *factory,
                                    				   const pjmedia_codec_info *info )
{
        PJ_UNUSED_ARG(factory);
    
        /* Check payload type. */
        if (info->pt != PJMEDIA_RTP_PT_G729)
        	return PJMEDIA_CODEC_EUNSUP;
    
        /* Ignore the rest, since it's static payload type. */
    
        return PJ_SUCCESS;
    }

/*
   * Generate default attribute.
   */
static pj_status_t g729_default_attr (pjmedia_codec_factory *factory,
                                       				      const pjmedia_codec_info *id,
                                       				      pjmedia_codec_param *attr )
{
        PJ_UNUSED_ARG(factory);
        PJ_UNUSED_ARG(id);
    
        pj_bzero(attr, sizeof(pjmedia_codec_param));
        attr->info.clock_rate = 8000;
        attr->info.channel_cnt = 1;
        attr->info.avg_bps = 8000;
        attr->info.max_bps = 8000;
        attr->info.pcm_bits_per_sample = 16;
        attr->info.frm_ptime = 10;
        attr->info.pt = PJMEDIA_RTP_PT_G729;
    
        attr->setting.frm_per_pkt = 2;
        attr->setting.vad = 1;
    #if !PLC_DISABLED
            attr->setting.plc = 1;
        #endif
    
            /* Default all other flag bits disabled. */
    
            return PJ_SUCCESS;
    }

/*
   * Enum codecs supported by this factory (i.e. only GSM!).
   */
static pj_status_t g729_enum_codecs(pjmedia_codec_factory *factory,
                                     				    unsigned *count,
                                     				    pjmedia_codec_info codecs[])
{
        PJ_UNUSED_ARG(factory);
        PJ_ASSERT_RETURN(codecs && *count > 0, PJ_EINVAL);
    
        pj_bzero(&codecs[0], sizeof(pjmedia_codec_info));
        codecs[0].encoding_name = pj_str("G729");
        codecs[0].pt = PJMEDIA_RTP_PT_G729;
        codecs[0].type = PJMEDIA_TYPE_AUDIO;
        codecs[0].clock_rate = 8000;
        codecs[0].channel_cnt = 1;
    
        *count = 1;
    
        return PJ_SUCCESS;
}

/*
   * Allocate a new GSM codec instance.
   */
static pj_status_t g729_alloc_codec( pjmedia_codec_factory *factory,
                                     				    const pjmedia_codec_info *id,
                                     				    pjmedia_codec **p_codec)
{
        pjmedia_codec *codec;
        struct g729_data *g729_data;
        pj_status_t status;
    
        PJ_ASSERT_RETURN(factory && id && p_codec, PJ_EINVAL);
        PJ_ASSERT_RETURN(factory == &g729_codec_factory.base, PJ_EINVAL);
    
    
        pj_mutex_lock(g729_codec_factory.mutex);
    
        /* Get free nodes, if any. */
        if (!pj_list_empty(&g729_codec_factory.codec_list)) {
        	codec = g729_codec_factory.codec_list.next;
        	pj_list_erase(codec);
        } else {
            codec = PJ_POOL_ZALLOC_T(g729_codec_factory.pool, pjmedia_codec);
            PJ_ASSERT_RETURN(codec != NULL, PJ_ENOMEM);
            codec->op = &g729_op;
            codec->factory = factory;
            
            g729_data = PJ_POOL_ZALLOC_T(g729_codec_factory.pool, struct g729_data);
            codec->codec_data = g729_data;
            
            #if !PLC_DISABLED
                /* Create PLC */
                status = pjmedia_plc_create(g729_codec_factory.pool, 8000,
                                                				    80, 0, &g729_data->plc);
            	if (status != PJ_SUCCESS) {
                	    pj_mutex_unlock(g729_codec_factory.mutex);
                	    return status;
                }
            #endif
            
            /* Create silence detector */
            status = pjmedia_silence_det_create(g729_codec_factory.pool,
                                                8000, 80,
                                                &g729_data->vad);
            if (status != PJ_SUCCESS) {
                pj_mutex_unlock(g729_codec_factory.mutex);
                return status;
            }
        }
    
        pj_mutex_unlock(g729_codec_factory.mutex);
    
        *p_codec = codec;
    return PJ_SUCCESS;
}

/*
   * Free codec.
   */
static pj_status_t g729_dealloc_codec( pjmedia_codec_factory *factory,
                                      pjmedia_codec *codec )
{
    struct g729_data *g729_data;
    int i;
    
    PJ_ASSERT_RETURN(factory && codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &g729_codec_factory.base, PJ_EINVAL);
    
    g729_data = (struct g729_data*) codec->codec_data;
    
    /* Close codec, if it's not closed. */
    g729_codec_close(codec);
    
    #if !PLC_DISABLED
        /* Clear left samples in the PLC, since codec+plc will be reused
         * next time.
         */
        for (i=0; i<2; ++i) {
            pj_int16_t frame[80];
            pjmedia_zero_samples(frame, PJ_ARRAY_SIZE(frame));
            pjmedia_plc_save(g729_data->plc, frame);
        }
    #else
        PJ_UNUSED_ARG(i);
    #endif
    
    /* Re-init silence_period */
    pj_set_timestamp32(&g729_data->last_tx, 0, 0);
    
    /* Put in the free list. */
    pj_mutex_lock(g729_codec_factory.mutex);
    pj_list_push_front(&g729_codec_factory.codec_list, codec);
    pj_mutex_unlock(g729_codec_factory.mutex);
    
    return PJ_SUCCESS;
}

/*
   * Init codec.
   */
static pj_status_t g729_codec_init( pjmedia_codec *codec,
                                   pj_pool_t *pool )
{
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(pool);
    return PJ_SUCCESS;
}

/*
   * Open codec.
   */
static pj_status_t g729_codec_open( pjmedia_codec *codec,
                                   pjmedia_codec_param *attr )
{
    struct g729_data *g729_data = (struct g729_data*) codec->codec_data;
    
    pj_assert(g729_data != NULL);
    pj_assert(g729_data->encoder == NULL && g729_data->decoder == NULL);
    
    g729_data->encoder = initBcg729EncoderChannel();
    if (!g729_data->encoder)
        return PJMEDIA_CODEC_EFAILED;
    
    g729_data->decoder = initBcg729DecoderChannel();
    if (!g729_data->decoder)
        return PJMEDIA_CODEC_EFAILED;
    
    g729_data->vad_enabled = (attr->setting.vad != 0);
    g729_data->plc_enabled = (attr->setting.plc != 0);
    
    return PJ_SUCCESS;
}

/*
   * Close codec.
   */
static pj_status_t g729_codec_close( pjmedia_codec *codec )
{
    struct g729_data *g729_data = (struct g729_data*) codec->codec_data;

    pj_assert(g729_data != NULL);

    if (g729_data->encoder) {
        closeBcg729EncoderChannel(g729_data->encoder);
        g729_data->encoder = NULL;
    }
    if (g729_data->decoder) {
        closeBcg729DecoderChannel(g729_data->decoder);
        g729_data->decoder = NULL;
    }
    
    return PJ_SUCCESS;
}


/*
   * Modify codec settings.
   */
static pj_status_t  g729_codec_modify(pjmedia_codec *codec,
                                      const pjmedia_codec_param *attr )
{
    struct g729_data *g729_data = (struct g729_data*) codec->codec_data;
    
    pj_assert(g729_data != NULL);
    pj_assert(g729_data->encoder != NULL && g729_data->decoder != NULL);

    g729_data->vad_enabled = (attr->setting.vad != 0);
    g729_data->plc_enabled = (attr->setting.plc != 0);
    
    return PJ_SUCCESS;
}


/*
   * Get frames in the packet.
   */
static pj_status_t  g729_codec_parse( pjmedia_codec *codec,
                                     void *pkt,
                                     pj_size_t pkt_size,
                                     const pj_timestamp *ts,
                                     unsigned *frame_cnt,
                                     pjmedia_frame frames[])
{
    unsigned count = 0;

    PJ_UNUSED_ARG(codec);
    
    PJ_ASSERT_RETURN(frame_cnt, PJ_EINVAL);
    
    while (pkt_size >= 10 && count < *frame_cnt) {
        frames[count].type = PJMEDIA_FRAME_TYPE_AUDIO;
        frames[count].buf = pkt;
        frames[count].size = 10;
        frames[count].timestamp.u64 = ts->u64 + count * 80;
        
        pkt = ((char*)pkt) + 10;
        pkt_size -= 10;
        
        ++count;
    }
    
    *frame_cnt = count;
    return PJ_SUCCESS;
}

/*
   * Encode frame.
   */
static pj_status_t g729_codec_encode( pjmedia_codec *codec,
                                     const struct pjmedia_frame *input,
                                     unsigned output_buf_len,
                                     struct pjmedia_frame *output)
{
    struct g729_data *g729_data = (struct g729_data*) codec->codec_data;
    pj_int16_t *pcm_in;
    unsigned in_size;
    
    pj_assert(g729_data && input && output);

    pcm_in = (pj_int16_t*)input->buf;
    in_size = input->size;
    
    PJ_ASSERT_RETURN(in_size % 160 == 0, PJMEDIA_CODEC_EPCMFRMINLEN);
    PJ_ASSERT_RETURN(output_buf_len >= 10 * in_size/160,
                     PJMEDIA_CODEC_EFRMTOOSHORT);
    
    /* Detect silence */
    if (g729_data->vad_enabled) {
        pj_bool_t is_silence;
        pj_int32_t silence_duration;
        
        silence_duration = pj_timestamp_diff32(&g729_data->last_tx,
                                               &input->timestamp);
        
        is_silence = pjmedia_silence_det_detect(g729_data->vad,
                                                (const pj_int16_t*) input->buf,
                                                (input->size >> 1),
                                                NULL);
        if (is_silence &&
            (PJMEDIA_CODEC_MAX_SILENCE_PERIOD == -1 ||
             silence_duration < PJMEDIA_CODEC_MAX_SILENCE_PERIOD*8000/1000))
        {
            output->type = PJMEDIA_FRAME_TYPE_NONE;
            output->buf = NULL;
            output->size = 0;
            output->timestamp = input->timestamp;
            return PJ_SUCCESS;
        } else {
            g729_data->last_tx = input->timestamp;
        }
    }
    
    /* Encode */
    output->size = 0;
    while (in_size >= 160) {
        bcg729Encoder(g729_data->encoder, pcm_in,
                      (unsigned char*)output->buf + output->size);
        pcm_in += 80;
        output->size += 10;
        in_size -= 160;
    }
    
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->timestamp = input->timestamp;
    
    return PJ_SUCCESS;
}

/*
   * Decode frame.
   */
static pj_status_t g729_codec_decode( pjmedia_codec *codec,
                                      const struct pjmedia_frame *input,
                                      unsigned output_buf_len,
                                      struct pjmedia_frame *output)
{
    struct g729_data *g729_data = (struct g729_data*) codec->codec_data;
    
    pj_assert(g729_data != NULL);
    PJ_ASSERT_RETURN(input && output, PJ_EINVAL);

    if (output_buf_len < 160)
        return PJMEDIA_CODEC_EPCMTOOSHORT;
    
    if (input->size < 10)
        return PJMEDIA_CODEC_EFRMTOOSHORT;
    
    bcg729Decoder(g729_data->decoder,
                  (unsigned char*)input->buf,
                  0, /*frame erasure flag*/
                  (short*)output->buf);
    
    output->size = 160;
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->timestamp = input->timestamp;
    
    #if !PLC_DISABLED
        if (g729_data->plc_enabled)
            pjmedia_plc_save( g729_data->plc, (pj_int16_t*)output->buf);
        #endif
    
    return PJ_SUCCESS;
}


#if !PLC_DISABLED
/*
   * Recover lost frame.
   */
static pj_status_t  g729_codec_recover(pjmedia_codec *codec,
                                       unsigned output_buf_len,
                                       struct pjmedia_frame *output)
{
    struct g729_data *g729_data = (struct g729_data*) codec->codec_data;
    
    PJ_ASSERT_RETURN(g729_data->plc_enabled, PJ_EINVALIDOP);
    
    PJ_ASSERT_RETURN(output_buf_len >= 160, PJMEDIA_CODEC_EPCMTOOSHORT);
    
    pjmedia_plc_generate(g729_data->plc, (pj_int16_t*)output->buf);
    output->size = 160;
    
    return PJ_SUCCESS;
}
#endif


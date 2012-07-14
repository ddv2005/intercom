#include "echo_canceller.h"
#include <pjmedia/delaybuf.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/list.h>
#include <pj/log.h>
#include <pj/math.h>
#include <pj/pool.h>
#include <math.h>

#define THIS_FILE   "echo_canceller.cpp"

/*
 * Create the echo canceller.
 */
pjs_echo_canceller::pjs_echo_canceller(pj_pool_t *pool_, unsigned clock_rate,
		unsigned samples_per_frame_, unsigned tail_ms, unsigned latency_ms,
		unsigned options) {
	int sampling_rate = clock_rate;
	unsigned ptime, lat_cnt;
	unsigned delay_buf_opt = 0;

	lat_ready = PJ_FALSE;
	/* Create new pool and instantiate and init the EC */
	pool = pj_pool_create(pool_->factory, "ec%p", 256, 256, NULL);
	lock = new PPJ_SemaphoreLock(pool, NULL, 1, 1);
	samples_per_frame = samples_per_frame_;
	frm_buf = (pj_int16_t*) pj_pool_alloc(pool, samples_per_frame << 1);

    state = speex_echo_state_init(samples_per_frame,
    					clock_rate * tail_ms / 1000);

    speex_echo_ctl(state, SPEEX_ECHO_SET_SAMPLING_RATE,
		   &sampling_rate);

    preprocess = speex_preprocess_state_init(samples_per_frame,
						   clock_rate);
    tmp_frame = (pj_int16_t*) pj_pool_zalloc(pool, 2*samples_per_frame);

    speex_preprocess_ctl(preprocess, SPEEX_PREPROCESS_SET_ECHO_STATE,
 			state);

	pj_list_init(&lat_buf);
	pj_list_init(&lat_free);

	PJ_LOG(5, (THIS_FILE, "Creating echo canceler"));

	/* Create latency buffers */
	ptime = samples_per_frame * 1000 / clock_rate;
	if (latency_ms < ptime) {
		/* Give at least one frame delay to simplify programming */
		latency_ms = ptime;
	}
	lat_cnt = latency_ms / ptime;
	while (lat_cnt--) {
		struct frame *frm;

		frm = (struct frame*) pj_pool_alloc(pool,
				(samples_per_frame << 1) + sizeof(struct frame));
		pj_list_push_back(&lat_free, frm);
	}

	/* Create delay buffer to compensate drifts */
	if (options & PJMEDIA_ECHO_USE_SIMPLE_FIFO)
		delay_buf_opt |= PJMEDIA_DELAY_BUF_SIMPLE_FIFO;
	pjmedia_delay_buf_create(pool, NULL, clock_rate, samples_per_frame,
			1, (PJMEDIA_SOUND_BUFFER_COUNT + 1) * ptime, delay_buf_opt,
			&delay_buf);
	PJ_LOG(4, (THIS_FILE, "ECHO canceller created, clock_rate=%d, channel=%d, "
	"samples per frame=%d, tail length=%d ms, "
	"latency=%d ms", clock_rate, 1, samples_per_frame, tail_ms, latency_ms));
}

/*
 * Destroy the Echo Canceller.
 */
pjs_echo_canceller::~pjs_echo_canceller() {
	if (delay_buf) {
		pjmedia_delay_buf_destroy(delay_buf);
	}
	delete lock;

    if (preprocess) {
    	speex_preprocess_state_destroy(preprocess);
    }

	if (state) {
    	speex_echo_state_destroy(state);
    }


	pj_pool_release(pool);
}

/*
 * Reset the echo canceller.
 */
void pjs_echo_canceller::reset() {
	PPJ_WaitAndLock wl(*lock);
	while (!pj_list_empty(&lat_buf)) {
		struct frame *frm;
		frm = lat_buf.next;
		pj_list_erase(frm);
		pj_list_push_back(&lat_free, frm);
	}
	lat_ready = PJ_FALSE;
	speex_echo_state_reset(state);
	pjmedia_delay_buf_reset(delay_buf);
}

/*
 * Let the Echo Canceller know that a frame has been played to the speaker.
 */
pj_status_t pjs_echo_canceller::playback(pj_int16_t *play_frm, unsigned size) {
	/* Playing frame should be stored, as it will be used by echo_capture()
	 * as reference frame, delay buffer is used for storing the playing frames
	 * as in case there was clock drift between mic & speaker.
	 *
	 * Ticket #830:
	 * Note that pjmedia_delay_buf_put() may modify the input frame and those
	 * modified frames may not be smooth, i.e: if there were two or more
	 * consecutive pjmedia_delay_buf_get() before next pjmedia_delay_buf_put(),
	 * so we'll just feed the delay buffer with the copy of playing frame,
	 * instead of the original playing frame. However this will cause the EC
	 * uses slight 'different' frames (for reference) than actually played
	 * by the speaker.
	 */
	if(samples_per_frame!=size)
	{
		PJ_LOG(1, (THIS_FILE, "WRONG SIZE ON PLAYBACK %d != %d",size,samples_per_frame));
		return -1;
	}

	PPJ_WaitAndLock wl(*lock);

	pjmedia_copy_samples(frm_buf, play_frm, samples_per_frame);
	pjmedia_delay_buf_put(delay_buf, frm_buf);

	if (!lat_ready) {
		/* We've not built enough latency in the buffer, so put this frame
		 * in the latency buffer list.
		 */
		struct frame *frm;

		if (pj_list_empty(&lat_free)) {
			lat_ready = PJ_TRUE;
			PJ_LOG(4, (THIS_FILE, "Latency bufferring complete"));
			return PJ_SUCCESS;
		}

		frm = lat_free.prev;
		pj_list_erase(frm);

		/* Move one frame from delay buffer to the latency buffer. */
		pjmedia_delay_buf_get(delay_buf, frm_buf);
		pjmedia_copy_samples(frm->buf, frm_buf, samples_per_frame);
		pj_list_push_back(&lat_buf, frm);
	}

	return PJ_SUCCESS;
}

/*
 * Let the Echo Canceller knows that a frame has been captured from
 * the microphone.
 */
pj_status_t pjs_echo_canceller::capture(pj_int16_t *rec_frm, unsigned size) {
	struct frame *oldest_frm;
	pj_status_t status, rc;

	if(samples_per_frame!=size)
	{
		PJ_LOG(1, (THIS_FILE, "WRONG SIZE ON CAPTURE %d != %d",size,samples_per_frame));
		return -1;
	}
	for (unsigned i = 0; i < samples_per_frame; i++)
	{

		REAL f = hp00.highpass(rec_frm[i]);
		f = hp0.highpass(f);
		rec_frm[i] = round(f);
	}

	PPJ_WaitAndLock wl(*lock);
	if (!lat_ready) {
		/* Prefetching to fill in the desired latency */
		PJ_LOG(4, (THIS_FILE, "Prefetching.."));
		return PJ_SUCCESS;
	}

	/* Retrieve oldest frame from the latency buffer */
	oldest_frm = lat_buf.next;
	pj_list_erase(oldest_frm);

	lock->release();

    speex_echo_cancellation(state, (const spx_int16_t*)rec_frm,
			    (const spx_int16_t*)oldest_frm->buf,
			    (spx_int16_t*)tmp_frame);


    /* Preprocess output */
    speex_preprocess_run(preprocess, (spx_int16_t*)tmp_frame);
    pjmedia_copy_samples(rec_frm, tmp_frame, samples_per_frame);

	status = PJ_SUCCESS;
	/* Cancel echo using this reference frame */
	lock->acquire();

	/* Move one frame from delay buffer to the latency buffer. */
	rc = pjmedia_delay_buf_get(delay_buf, oldest_frm->buf);
	if (rc != PJ_SUCCESS) {
		/* Ooops.. no frame! */
		PJ_LOG(4,
				(THIS_FILE, "No frame from delay buffer. This will upset EC later"));
		pjmedia_zero_samples(oldest_frm->buf, samples_per_frame);
	}
	pj_list_push_back(&lat_buf, oldest_frm);

	return status;
}


#ifndef ECHO_C_H_
#define ECHO_C_H_
#include <pj/list.h>
#include <pjmedia/delaybuf.h>
#include <pjmedia/echo.h>
#include <pj_lock.h>
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>


#define PJAEC_LONG_HIST_SIZE_S	5

#define PJS_FMATH_SHIFT	14
#define PJS_FMATH_Q32(v,bits) (((pj_int32_t)v)<< bits)
#define PJS_FMATH_CONST32(c,bits) ((pj_int32_t) ((c) *((((pj_int32_t)1)<< bits))))
#define PJS_FMATH_EXT_CONST(c) PJS_FMATH_CONST32(c,PJS_FMATH_SHIFT)
#define PJS_FMATH_ONE_CONST (((pj_int32_t)1)<<PJS_FMATH_SHIFT)
#define PJS_FMATH_FLOAT(p) (p/(float)PJS_FMATH_ONE_CONST)

typedef float REAL;
const REAL MAXPCM = 32767.0f;

class IIR_HP {
  pj_int32_t x;
  const static pj_int32_t a0 = PJS_FMATH_EXT_CONST(0.01f);

public:
   IIR_HP() { x = 0; };
   pj_int16_t highpass(pj_int16_t in) {
	pj_int32_t ii = PJS_FMATH_Q32(in, PJS_FMATH_SHIFT);
    /* Highpass = Signal - Lowpass. Lowpass = Exponential Smoothing */
    x += ((pj_int64_t)a0 * (ii - x)) >> PJS_FMATH_SHIFT;
    return (ii - x) >> PJS_FMATH_SHIFT;
  };
};


/* 13 taps FIR Finite Impulse Response filter
 * Coefficients calculated with
 * www.dsptutor.freeuk.com/KaiserFilterDesign/KaiserFilterDesign.html
 */
class FIR_HP13 {
  REAL z[14];
public:
   FIR_HP13() { memset(this, 0, sizeof(FIR_HP13)); };
  REAL highpass(REAL in) {
	  static const REAL a[14] = {
	    // Kaiser Window FIR Filter, Filter type: High pass
	    // Passband: 300.0 - 4000.0 Hz, Order: 12
	    // Transition band: 100.0 Hz, Stopband attenuation: 10.0 dB
	    -0.043183226f, -0.046636667f, -0.049576525f, -0.051936015f,
	    -0.053661242f, -0.054712527f, 0.82598513f, -0.054712527f,
	    -0.053661242f, -0.051936015f, -0.049576525f, -0.046636667f,
	    -0.043183226f, 0.0f
	  };
    memmove(z+1, z, 13*sizeof(REAL));
    z[0] = in;
    REAL sum0 = 0.0, sum1 = 0.0;
    int j;

    for (j = 0; j < 14; j+= 2) {
      // optimize: partial loop unrolling
      sum0 += a[j] * z[j];
      sum1 += a[j+1] * z[j+1];
    }
    return sum0+sum1;
  }
};


struct frame {
	PJ_DECL_LIST_MEMBER(struct frame);
	short buf[1];
};

class pjs_level
{
protected:
	bool		 m_init;
	pj_uint16_t	 m_pos;
	pj_uint16_t	 m_size;
	pj_uint16_t	*m_hist;
public:
	pjs_level(pj_uint16_t size):m_size(size)
	{
		m_pos = 0;
		m_init = true;
		m_hist = (pj_uint16_t *)malloc(size*sizeof(pj_uint16_t));
	}

	~pjs_level()
	{
		free(m_hist);
	}

	void reset()
	{
		m_init = true;
		m_pos = 0;
	}

	pj_uint16_t get_min_level()
	{
		pj_uint16_t	result = 0;
		if(m_hist&&(!m_init||(m_pos>0)))
		{
			result = 32767;
			pj_uint16_t	max_index = m_init?m_pos:m_size;
			for(pj_uint16_t i = 0; i<max_index; i++)
			{
				if(result>m_hist[i])
					result = m_hist[i];

			}
		}
		return result;
	}

	pj_uint16_t get_max_level()
	{
		pj_uint16_t	result = 32767;
		if(m_hist&&(!m_init||(m_pos>0)))
		{
			result = 0;
			pj_uint16_t	max_index = m_init?m_pos:m_size;
			for(pj_uint16_t i = 0; i<max_index; i++)
			{
				if(result<m_hist[i])
					result = m_hist[i];

			}
		}
		return result;
	}


	void add_level(pj_uint16_t level)
	{
		m_hist[m_pos] = level;
		m_pos++;
		if(m_pos>=m_size)
		{
			m_init = false;
			m_pos = 0;
		}
	}

	static pj_uint16_t calc_level(pj_int16_t *data, pj_uint16_t size)
	{
		pj_uint32_t result=0;
		for (pj_uint16_t i = 0; i < size; i ++)
		{
			result += abs(data[i]);
		}
		result /= size;
		return (pj_uint16_t)result;
	}
};

class pjs_echo_canceller {
protected:
	pj_pool_t *pool;
	unsigned samples_per_frame;

	pj_bool_t lat_ready; /* lat_buf has been filled in.	    */
	struct frame lat_buf; /* Frame queue for delayed playback	    */
	struct frame lat_free; /* Free frame list.			    */

	pjmedia_delay_buf *delay_buf;
	pj_int16_t *frm_buf;

	IIR_HP hp00;             // DC-level remove Highpass
	FIR_HP13 hp0;                 // 300Hz cut-off Highpass

    SpeexEchoState	 	 *state;
    SpeexPreprocessState *preprocess;
    pj_int16_t *tmp_frame;

	PPJ_SemaphoreLock *lock;
public:
	pjs_echo_canceller(pj_pool_t *pool_, unsigned clock_rate,
			unsigned samples_per_frame_, unsigned tail_ms, unsigned latency_ms,
			unsigned options);
	~pjs_echo_canceller();
	void reset();
	pj_status_t playback(pj_int16_t *play_frm, unsigned size);
	pj_status_t capture(pj_int16_t *rec_frm, unsigned size);
};

#endif

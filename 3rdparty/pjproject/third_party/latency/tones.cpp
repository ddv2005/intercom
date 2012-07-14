#include "tones.h"

#define PJMEDIA_TONEGEN_FIXED_POINT_CORDIC_LOOP  10

/* Cordic algorithm with 28 bit size, from:
 * http://www.dcs.gla.ac.uk/~jhw/cordic/
 * Speed = 742 usec to generate 1 second, 8KHz dual-tones (2.66GHz P4).
 *         (PJMEDIA_TONEGEN_FIXED_POINT_CORDIC_LOOP=7)
 *         approx. 6.01 MIPS
 *
 *         ARM926EJ-S results:
 *	        loop=7:   8,943 usec/1.77 MIPS
 *		loop=8:   9,872 usec/1.95 MIPS
 *          loop=10: 11,662 usec/2.31 MIPS
 *          loop=12: 13,561 usec/2.69 MIPS
 */
#define CORDIC_1K		0x026DD3B6
#define CORDIC_HALF_PI	0x06487ED5
#define CORDIC_PI		(CORDIC_HALF_PI * 2)
#define CORDIC_MUL_BITS	26
#define CORDIC_MUL		(1 << CORDIC_MUL_BITS)
#define CORDIC_NTAB		28
#define CORDIC_LOOP		PJMEDIA_TONEGEN_FIXED_POINT_CORDIC_LOOP

static int cordic_ctab[] =
{ 0x03243F6A, 0x01DAC670, 0x00FADBAF, 0x007F56EA, 0x003FEAB7, 0x001FFD55,
		0x000FFFAA, 0x0007FFF5, 0x0003FFFE, 0x0001FFFF, 0x0000FFFF, 0x00007FFF,
		0x00003FFF, 0x00001FFF, 0x00000FFF, 0x000007FF, 0x000003FF, 0x000001FF,
		0x000000FF, 0x0000007F, 0x0000003F, 0x0000001F, 0x0000000F, 0x00000007,
		0x00000003, 0x00000001, 0x00000000, 0x00000000 };

static pj_int32_t cordic(pj_int32_t theta, unsigned n)
{
	unsigned k;
	int d;
	pj_int32_t tx;
	pj_int32_t x = CORDIC_1K, y = 0, z = theta;

	for (k = 0; k < n; ++k)
	{
#if 0
		d = (z>=0) ? 0 : -1;
#else
		/* Only slightly (~2.5%) faster, but not portable? */
		d = z >> 27;
#endif
		tx = x - (((y >> k) ^ d) - d);
		y = y + (((x >> k) ^ d) - d);
		z = z - ((cordic_ctab[k] ^ d) - d);
		x = tx;
	}
	return y;
}

/* Note: theta must be uint32 here */
static pj_int32_t cordic_sin(pj_uint32_t theta, unsigned n)
{
	if (theta < CORDIC_HALF_PI)
		return cordic(theta, n);
	else
		if (theta < CORDIC_PI)
			return cordic(CORDIC_HALF_PI - (theta - CORDIC_HALF_PI), n);
		else
			if (theta < CORDIC_PI + CORDIC_HALF_PI)
				return -cordic(theta - CORDIC_PI, n);
			else
				if (theta < 2 * CORDIC_PI)
					return -cordic(
							CORDIC_HALF_PI - (theta - 3 * CORDIC_HALF_PI), n);
				else
				{
					pj_assert(!"Invalid cordic_sin() value");
					return 0;
				}
}

#define VOL(var,v)		(((v) * var.vol) >> 15)
#define GEN_INIT(var,R,F,A)	gen_init(&var, R, F, A)
#define GEN_SAMP(val,var)	val = gen_samp(&var)

static void gen_init(struct gen *var, unsigned R, unsigned F, unsigned A)
{
	var->add = 2 * CORDIC_PI / R * F;
	var->c = 0;
	var->vol = A;
}

PJ_INLINE(short) gen_samp(struct gen *var)
{
	pj_int32_t val;
	val = cordic_sin(var->c, CORDIC_LOOP);
	/*val = (val * 32767) / CORDIC_MUL;
	 *val = VOL((*var), val);
	 */
	val = ((val >> 10) * var->vol) >> 16;
	var->c += var->add;
	if (var->c > 2 * CORDIC_PI)
		var->c -= (2 * CORDIC_PI);
	return (short) val;
}

void init_generate_dual_tone(struct gen_state *state,
				    unsigned clock_rate,
				    unsigned freq1,
				    unsigned freq2,
				    unsigned vol)
{
    GEN_INIT(state->tone1,clock_rate,freq1,vol);
    GEN_INIT(state->tone2,clock_rate,freq2,vol);
}


void generate_dual_tone(struct gen_state *state,
			       unsigned channel_count,
			       unsigned samples,
			       short buf[])
{
    short *end = buf + samples;

    if (channel_count==1) {
	int val, val2;
	while (buf < end) {
	    GEN_SAMP(val, state->tone1);
	    GEN_SAMP(val2, state->tone2);
	    *buf++ = (short)((val+val2) >> 1);
	}
    } else if (channel_count == 2) {
	int val, val2;
	while (buf < end) {

	    GEN_SAMP(val, state->tone1);
	    GEN_SAMP(val2, state->tone2);
	    val = (val + val2) >> 1;

	    *buf++ = (short)val;
	    *buf++ = (short)val;
	}
    }
}

//===========================
#include <math.h>
tone_detector::tone_detector(pj_uint32_t clock_rate,pj_uint32_t freq)
{
	m_coef = 2.0f * cosf(2.0f * M_PI * (float)freq/(float)clock_rate);
}

pj_int32_t tone_detector::detect(short *data,unsigned size)
{
    int i;
    float s, s_prev1 = 0.0f, s_prev2 = 0.0f;

    for (i = 0; i < size; i++) {
        s = data[i]/32768.0 + (m_coef * s_prev1) - s_prev2;
        s_prev2 = s_prev1;
        s_prev1 = s;
    }

    return (s_prev1 * s_prev1) + (s_prev2 * s_prev2)
         - (s_prev1 * s_prev2 * m_coef);
}

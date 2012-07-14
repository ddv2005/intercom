#ifndef TONES_H_
#define TONES_H_
#include "common.h"

struct gen
{
	unsigned add;
	pj_uint32_t c;
	unsigned vol;
};

struct gen_state
{
    struct gen tone1;
    struct gen tone2;
};

void init_generate_dual_tone(struct gen_state *state,
				    unsigned clock_rate,
				    unsigned freq1,
				    unsigned freq2,
				    unsigned vol);
void generate_dual_tone(struct gen_state *state,
			       unsigned channel_count,
			       unsigned samples,
			       short buf[]);

class tone_detector
{
protected:
	float       m_coef;
public:
	tone_detector(pj_uint32_t clock_rate,pj_uint32_t freq);
	pj_int32_t detect(short *data,unsigned size);
};

#endif /* TONES_H_ */

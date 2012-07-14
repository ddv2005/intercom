#ifndef SND_GAINCONTROL_H_
#define SND_GAINCONTROL_H_
#include "common.h"
#include <string>

//#define DEBUG_GAIN	1
#define SND_RVOL_COUNT	25
#define SND_AGC_SHIFT	16
#define SND_AGC_CONST32I(c,bits) ((pj_int32_t) ((c) *((((pj_int32_t)1)<< bits))))
#define SND_AGC_CONST32(c,bits) ((pj_int32_t) (.5 + (c) *((((pj_int32_t)1)<< bits))))
#define SND_AGC_EXT_CONST(c) SND_AGC_CONST32(c,SND_AGC_SHIFT)
#define SND_AGC_EXT_CONSTI(c) SND_AGC_CONST32I(c,SND_AGC_SHIFT)
#define SND_AGC_ONE_CONST (((pj_int32_t)1)<<SND_AGC_SHIFT)
class snd_agc
{
protected:
	pj_int32_t f_gain;
	pj_int32_t f_max_gain;
	pj_int32_t f_min_gain;
	pj_int16_t f_RVolsAGC[SND_RVOL_COUNT];
	pj_uint16_t f_RVolsAGCPos;
	pj_int16_t f_averagePower;
	pj_int16_t f_revAveragePower;
	pj_int16_t f_targetLevel;
	pj_int16_t f_expectedMaxLevel;
	std::string f_name;
	pj_uint16_t f_levels[32768];

	pj_int16_t calcPowerLevel(pj_int16_t *data, pj_uint16_t size)
	{
		pj_uint32_t result=0;
		for (pj_uint16_t i = 0; i < size; i ++)
		{
			result += abs(data[i]);
		}
		result /= size;
		return (pj_int16_t)result;
	}

	pj_int16_t calcMaxLevel(pj_int16_t *data, pj_uint16_t size)
	{
		pj_int16_t result = 0;
		for (pj_uint16_t i = 0; i <size; i ++)
		{
			pj_int16_t v = abs(data[i]);
			if (v > result)
			{
				result = v;
			}
		}
		return result;
	}
public:
	snd_agc(const char * name,pj_int32_t max_gain,pj_int32_t min_gain, pj_int16_t expectedMaxLevel,pj_int16_t targetLevel):f_max_gain(SND_AGC_EXT_CONSTI(max_gain)),f_min_gain(SND_AGC_ONE_CONST/min_gain),f_targetLevel(targetLevel),f_name(name)
	{
		f_expectedMaxLevel = expectedMaxLevel;
		f_gain = SND_AGC_ONE_CONST;
		f_RVolsAGCPos = 0;
		f_averagePower = 0;
		f_revAveragePower = 0;
		memset(f_RVolsAGC,0,sizeof(f_RVolsAGC));
	}

	// return = signal power
	pj_int16_t process(pj_int16_t *data, pj_uint16_t size, pj_int16_t rev_power, pj_uint16_t *level)
	{
		pj_uint32_t i;
		pj_int16_t powerLevel=min(32767,(pj_int32_t)calcPowerLevel(data,size)*(pj_int32_t)f_targetLevel/(pj_int32_t)f_expectedMaxLevel);
		pj_uint8_t action = 1; // 0 - No change, 1 - up, 2 - down , 3 - down to 1
		pj_int32_t prev_gain = f_gain;
		pj_int16_t maxLevel = calcMaxLevel(data,size);

		if(level)
		{
			*level = 0;
			memset(f_levels,0,sizeof(f_levels));
			for (pj_int32_t i = size-1; i>=0; i--)
			{
				pj_int16_t v = abs(data[i]);
				f_levels[v]++;
			}

			int l = size >> 3;
			int c = 0;
			int s = 0;
			for (pj_int32_t i = 32767; i>=0; i--)
			{
				c+=f_levels[i];
				s+=i*f_levels[i];
				if(c>l)
				{
					*level = s/c;
					break;
				}
			}
		}

		f_RVolsAGC[f_RVolsAGCPos++] = maxLevel;
	    if(f_RVolsAGCPos>=SND_RVOL_COUNT)
	    	f_RVolsAGCPos=0;
	    pj_int16_t bufMaxLevel = 0;
	    for(i=0; i<SND_RVOL_COUNT; i++)
	    	if(bufMaxLevel<f_RVolsAGC[i])
	    		bufMaxLevel=f_RVolsAGC[i];
	    if(bufMaxLevel>0)
	    {
			f_averagePower = (pj_int16_t)((((pj_int32_t)f_averagePower)*972+52*(pj_int32_t)powerLevel)>>10);
			if(rev_power>0)
				f_revAveragePower = (pj_int16_t)((((pj_int32_t)f_revAveragePower)*972+52*(pj_int32_t)rev_power)>>10);
			else
				f_revAveragePower = 0;

			if(maxLevel<800)
				action = 0;
			else
			{
				if(rev_power>0)
				{
					if(f_averagePower<1)
						f_averagePower = 1;
					pj_int32_t ratio = f_revAveragePower/f_averagePower;
					if(ratio>10)
					{
						action = 2;
					}
					else
					{
						if(ratio>5)
						{
							action = 3;
						}
						else
						{
							if(ratio>2)
								action = 0;
						}
					}
				}
			}

			pj_int32_t maxGain = (((pj_int32_t)f_targetLevel) << SND_AGC_SHIFT)/bufMaxLevel;
			if( maxGain<SND_AGC_ONE_CONST)
				maxGain = SND_AGC_ONE_CONST;
			if(maxGain<prev_gain)
				prev_gain = maxGain;

			switch(action)
			{
				case 1:
				{
					if(maxGain>((f_gain*SND_AGC_CONST32(1.1,10))>>10))
					{
						if(f_gain>SND_AGC_CONST32(0.96,SND_AGC_SHIFT))
							f_gain += SND_AGC_CONST32(0.05,SND_AGC_SHIFT);
						else
							f_gain = SND_AGC_ONE_CONST;
					}
					break;
				}

				case 2:
				{
					f_gain -= SND_AGC_CONST32(0.05,SND_AGC_SHIFT);
					break;
				}

				case 3:
				{
					if(f_gain > SND_AGC_ONE_CONST)
						f_gain -= SND_AGC_CONST32(0.05,SND_AGC_SHIFT);
					if(f_gain < SND_AGC_ONE_CONST)
						f_gain = SND_AGC_ONE_CONST;
					break;
				}
			}

			if(maxGain<((f_gain*SND_AGC_CONST32(0.95,10))>>10))
			{
				f_gain = (maxGain*SND_AGC_CONST32(0.90,10))>>10;
			}
			if(maxGain<f_gain)
			{
				f_gain = maxGain;
			}
			if(f_gain<f_min_gain)
			{
				f_gain = f_min_gain;
			}
			else
				if(f_gain>f_max_gain)
					f_gain = f_max_gain;
#if DEBUG_GAIN==1
			PJ_LOG_(1,("SND","%s gain: %f, power: %d, rev power: %d",f_name.c_str(),f_gain/65536.0,f_averagePower,f_revAveragePower));
#endif
			if(f_gain != SND_AGC_ONE_CONST )
			{
				for(pj_int32_t i=0; i<size; i++)
				{
					pj_int32_t f =  ((((pj_int32_t)data[i])*f_gain)>>SND_AGC_SHIFT);
					data[i] = (pj_int16_t) f;
				}
			}
	    }
		return powerLevel;
	}
};


#endif /* SND_GAINCONTROL_H_ */

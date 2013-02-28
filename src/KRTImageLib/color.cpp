#include "color.h"



void KColor::ConvertToDWORD(DWORD& clr) const
{
	BYTE br = BYTE(r * 255.5f);
	BYTE bg = BYTE(g * 255.5f);
	BYTE bb = BYTE(b * 255.5f);
	if (r < 0) 
		br = 0;
	else if (r > 1.0f)
		br = 255;
	if (g < 0) 
		bg = 0;
	else if (g > 1.0f)
		bg = 255;
	if (b < 0) 
		bb = 0;
	else if (b > 1.0f)
		bb = 255;
	clr = (br << 16) | (bg << 8) | bb | 0xff000000;
}

float KColor::DiffRatio(const KColor& dst) const
{
	float dr = fabs(r - dst.r);
	float dg = fabs(g - dst.g);
	float db = fabs(b - dst.b);
	float t = (dr + dg + db);
	float b = fabs(dst.r + dst.g + dst.b);
	if (t != 0)
		return (t / b);
	else
		return 0;
}

void KColor::Lerp(const KColor& ref, float value)
{
	r = r*(1.0f-value) + ref.r*value;
	g = g*(1.0f-value) + ref.g*value;
	b = b*(1.0f-value) + ref.b*value;
}

void KColor::Lerp(const KColor& ref, const KColor& value)
{
	r = r*(1.0f-value.r) + ref.r*value.r;
	g = g*(1.0f-value.g) + ref.g*value.g;
	b = b*(1.0f-value.b) + ref.b*value.b;
}
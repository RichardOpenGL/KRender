#pragma once
#include <common/defines/typedefs.h>
#include <common/defines/config.h>

class KRT_IMAGE_API BitmapObject
{
public:
	enum PixelFormat {
		eRGBA8,
		eRGB8,
		eRGB32F,
		eRGBA32F,
		eR32F
	};

	BitmapObject(void);
	~BitmapObject(void);


	void SetBitmapData(void* pData, 
		UINT32 bpp, UINT32 pitch,
		UINT32 channel_cnt,
		UINT32 w, UINT32 h,
		bool auto_free_mem);
	void SetPixel(void* pPixel, UINT32 x, UINT32 y);
	void* GetPixel(UINT32 x, UINT32 y);
	const void* GetPixel(UINT32 x, UINT32 y) const;
	void CopyFrom(const BitmapObject& src, 
		UINT32 dstX, UINT32 dstY,
		UINT32 srcX, UINT32 srcY,
		UINT32 w, UINT32 h);
	void CopyFromRGB32F(const BitmapObject& src, 
		UINT32 dstX, UINT32 dstY,
		UINT32 srcX, UINT32 srcY,
		UINT32 w, UINT32 h);
	void ClearByChecker();
	void ClearByZero();

	bool Save(const char* filename);
	bool Load(const char* filename, std::vector<BitmapObject*>* mipmap_list);

	static BitmapObject* CreateBitmap(UINT32 w, UINT32 h, PixelFormat format);
public:

	PixelFormat mFormat;
	BYTE* mpData;
	UINT32 mBpp;
	UINT32 mPitch;
	UINT32 mWidth;
	UINT32 mHeight;

	UINT32 mChannelCnt;
	UINT32 mBitsPerChannel;

	bool mAutoFreeMem;
	bool mIsFixedPoint;

	
};


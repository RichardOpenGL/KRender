#pragma once
#include "../shader/surface_shader.h"
#include "../image/basic_map.h"

class PhongSurface : public ISurfaceShader
{
public:

	struct PARAM {
		KColor mDiffuse;
		KColor mSpecular;
		float mPower;
		KColor mOpacity;
	};
	PARAM mParam;

	std::string mDiffuseMapFileName;
	Texture::Tex2D* mDiffuseMap;

	std::string mNormalMapFileName;

	PhongSurface(const char* name);
	virtual ~PhongSurface() {}

	virtual void Shade(const SurfaceContext& shadingCtx, KColor& out_clr) const;
	virtual void SetParam(const char* paramName, void* pData, UINT32 dataSize);

	virtual bool Save(FILE* pFile);
	virtual bool Load(FILE* pFile);

};

class MirrorSurface : public ISurfaceShader
{
public:
	MirrorSurface(const char* name);
	virtual ~MirrorSurface();

	struct PARAM {
		KVec3 mBaseColor;
		float mReflectGain;
	};
	PARAM mParam;

	virtual void Shade(const SurfaceContext& shadingCtx, KColor& out_clr) const;
	virtual void SetParam(const char* paramName, void* pData, UINT32 dataSize);

	virtual bool Save(FILE* pFile);
	virtual bool Load(FILE* pFile);
};

class AttributeDiagnoseSurface : public ISurfaceShader
{
public:
	AttributeDiagnoseSurface(const char* name);
	virtual ~AttributeDiagnoseSurface();
	
	enum Mode {eShowNormal, eShowUV};
	Mode mMode;

	virtual void Shade(const SurfaceContext& shadingCtx, KColor& out_clr) const;
	virtual void SetParam(const char* paramName, void* pData, UINT32 dataSize);

	virtual bool Save(FILE* pFile);
	virtual bool Load(FILE* pFile);
};


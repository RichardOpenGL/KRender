#include "shader_api.h"
#include "../util/HelperFunc.h"
#include "../sampling/halton2d.h"
#include "../animation/animated_transform.h"
#include "../scene/KKDBBoxScene.h"

#include <assert.h>

#define RAND_SEQUENCE_LEN 1023
extern UINT32 AREA_LIGHT_SAMP_CNT;

void RenderBuffers::SetImageSize(UINT32 w, UINT32 h, KRT_ImageFormat pixelFormat, void* pUserBuf)
{
	switch (pixelFormat) {
	case kRGB_8:
		output_image.reset(BitmapObject::CreateBitmap(w, h, BitmapObject::eRGB8, pUserBuf));
		break;
	case kRGBA_8:
		output_image.reset(BitmapObject::CreateBitmap(w, h, BitmapObject::eRGBA8, pUserBuf));
		break;
	case kRGBA_32F:
		output_image.reset(BitmapObject::CreateBitmap(w, h, BitmapObject::eRGBA32F, pUserBuf));
		break;
	case kRGB_32F:
		output_image.reset(BitmapObject::CreateBitmap(w, h, BitmapObject::eRGB32F, pUserBuf));
		break;
	case kR_32F:
		output_image.reset(BitmapObject::CreateBitmap(w, h, BitmapObject::eR32F, pUserBuf));
		break;
	}
	sampled_count_pp.resize(w * h);
	random_seed_pp.resize(w * h);
	for (UINT32 i = 0; i < random_seed_pp.size(); ++i) {
		sampled_count_pp[i] = 0;
		random_seed_pp[i] = (UINT32)rand() % RAND_SEQUENCE_LEN;
	}

	for (UINT32 y = 0; y < h; ++y) {
		void *pPixel = output_image->GetPixelPtr(0, y);
		memset(pPixel, 0, output_image->mPitch);
	}

	random_sequence.resize(RAND_SEQUENCE_LEN);
	for (UINT32 i = 0; i < RAND_SEQUENCE_LEN; ++i)
		random_sequence[i] = Rand_0_1();
}

void RenderBuffers::AddSamples(UINT32 x, UINT32 y, UINT32 sampleCnt, const KColor& avgClr, float alpha)
{
	KPixel curPixel = output_image->GetPixel(x, y);
	UINT32 sampled_count = GetSampledCount(x, y);
	float fSampledCnt = (float)sampled_count - sampleCnt;
	float fSampleCnt = (float)sampleCnt;

	KColor tempClr0 = curPixel.color;
	tempClr0.Scale(fSampledCnt);
	KColor tempClr1(avgClr);
	tempClr1.Scale(fSampleCnt);
	tempClr0.Add(tempClr1);
	tempClr0.Scale(1.0f / (fSampledCnt + fSampleCnt));
	curPixel.color = tempClr0;

	float newAlpha = (alpha*fSampleCnt  + curPixel.alpha*fSampledCnt) / (fSampledCnt + fSampleCnt);
	curPixel.alpha = newAlpha;
	output_image->SetPixel(x, y, curPixel);
}

UINT32 RenderBuffers::GetSampledCount(UINT32 x, UINT32 y) const
{
	return sampled_count_pp[y * output_image->mWidth + x];
}

void RenderBuffers::IncreaseSampledCount(UINT32 x, UINT32 y, UINT32 sampleCnt)
{
	sampled_count_pp[y * output_image->mWidth + x] += sampleCnt;
}

UINT32 RenderBuffers::GetRandomSeed(UINT32 x, UINT32 y) const
{
	return random_seed_pp[y * output_image->mWidth + x];
}

float RenderBuffers::GetPixelRandom(UINT32 x, UINT32 y, UINT32 offset) const
{
	UINT32 randSeed = GetRandomSeed(x, y);
	float res = random_sequence[(randSeed+offset) % RAND_SEQUENCE_LEN];
	return res;
}




#define RS_SCREEN_X_OFFSET	0
#define RS_SCREEN_Y_OFFSET	1
#define RS_DOF_X_OFFSET		2
#define RS_DOF_Y_OFFSET		3
#define RS_TIME_OFFSET		4
#define RS_LIGHT_OFFSET		5

float RenderBuffers::GetPixelRandom(UINT32 x, UINT32 y, UINT32 offset, float min, float max) const
{
	return nvmath::lerp(GetPixelRandom(x, y, offset), min, max);
}

KVec2 RenderBuffers::RS_Image(UINT32 x, UINT32 y) const
{
	KVec2 warp;
	float fx = (float)x;
	float fy = (float)y;
	KVec2 hPt;
	UINT32 sampledCnt = GetSampledCount(x, y);
	
	warp[0] = GetPixelRandom(x, y, RS_SCREEN_X_OFFSET);
	warp[1] = GetPixelRandom(x, y, RS_SCREEN_Y_OFFSET);

	KVec2 min(fx - 0.5f, fy - 0.5f);
	KVec2 max(fx + 0.5f, fy + 0.5f);

	Sampling::Halton2D(&hPt, sampledCnt, 1, warp, min, max);
	return hPt;
}

KVec2 RenderBuffers::RS_DOF(UINT32 x, UINT32 y, const KVec2& apertureSize) const
{
	KVec2 warp;
	float fx = (float)x;
	float fy = (float)y;
	KVec2 hPt;
	UINT32 sampledCnt = GetSampledCount(x, y);
	
	warp[0] = GetPixelRandom(x, y, RS_DOF_X_OFFSET);
	warp[1] = GetPixelRandom(x, y, RS_DOF_Y_OFFSET);

	KVec2 halfApertureSize = apertureSize;
	halfApertureSize *= 0.5f;
	KVec2 min(-halfApertureSize[0], halfApertureSize[0]);
	KVec2 max(-halfApertureSize[1], halfApertureSize[1]);

	Sampling::Halton2D(&hPt, sampledCnt, 1, warp, min, max);
	return hPt;
}

KVec2 RenderBuffers::RS_AreaLight(UINT32 x, UINT32 y, UINT32 lightIdx, UINT32 sampleIdx) const
{
	KVec2 warp;
	float fx = (float)x;
	float fy = (float)y;
	KVec2 hPt;
	UINT32 sampledCnt = GetSampledCount(x, y);
	UINT32 lightOffset = lightIdx * 2;

	warp[0] = GetPixelRandom(x, y, RS_LIGHT_OFFSET + lightOffset);
	warp[1] = GetPixelRandom(x, y, RS_LIGHT_OFFSET + 1 + lightOffset);

	KVec2 min(-1, -1);
	KVec2 max(1, 1);

	Sampling::Halton2D(&hPt, sampledCnt * AREA_LIGHT_SAMP_CNT + sampleIdx, 1, warp, min, max);
	return hPt;
}

float RenderBuffers::RS_MotionBlur(UINT32 x, UINT32 y) const
{
	KVec2 warp;
	float fx = (float)x;
	float fy = (float)y;
	KVec2 hPt;
	UINT32 sampledCnt = GetSampledCount(x, y);
	
	warp[0] = GetPixelRandom(x, y, RS_TIME_OFFSET);
	warp[1] = GetPixelRandom(x, y, RS_TIME_OFFSET);

	KVec2 min(0, 0);
	KVec2 max(1, 1);

	Sampling::Halton2D(&hPt, sampledCnt, 1, warp, min, max);
	return hPt[0];
}

const BitmapObject* RenderBuffers::GetOutputImagePtr() const
{
	return output_image.get();
}

TracingInstance::TracingInstance(const KKDBBoxScene* scene, const RenderBuffers* pBuffers)
{
	mpScene = scene;
	mpRenderBuffers = pBuffers;
	mCurPixel_X = INVALID_INDEX;
	mCurPixel_Y = INVALID_INDEX;
	mIsPixelSampling = false;
	mBounceDepth = 0;
}

TracingInstance::~TracingInstance()
{

}

const KKDBBoxScene* TracingInstance::GetScenePtr() const
{
	return mpScene;
}

KVec2 TracingInstance::GetAreaLightSample(UINT32 lightIdx, UINT32 sampleIdx) const
{
	if (mIsPixelSampling)
		return mpRenderBuffers->RS_AreaLight(mCurPixel_X, mCurPixel_Y, lightIdx, sampleIdx);
	else {
		KVec2 min(0, 0);
		KVec2 max(1, 1);
		KVec2 hPt;
		Sampling::Halton2D(&hPt, lightIdx*AREA_LIGHT_SAMP_CNT + sampleIdx, 1, KVec2(0,0), min, max);
		return KVec2(Rand_0_1(), Rand_0_1());
	}
}

void TracingInstance::SetCurrentPixel(UINT32 x, UINT32 y)
{
	mCurPixel_X = x;
	mCurPixel_Y = y;
	mIsPixelSampling = true;
}

void TracingInstance::IncBounceDepth()
{
	++mBounceDepth;
}

void TracingInstance::DecBounceDepth()
{
	assert(mBounceDepth > 0);
	--mBounceDepth;
}

UINT32 TracingInstance::GetBoundDepth() const
{
	return mBounceDepth;
}

KColor TracingInstance::GetBackGroundColor(const KVec3& dir) const
{
	return KColor(0,0,0);
}

void TracingInstance::CalcuShadingContext(const KRay& hitRay, const IntersectContext& hit_ctx, ShadingContext& out_shading_ctx) const
{
	const KKDBBoxScene* pScene = mpScene;
	const KScene* pKDScene = pScene->GetNodeKDScene(hit_ctx.bbox_node_idx);
	KAnimation::LocalTRSFrame::LclTRS nodeTRS;
	pScene->GetNodeTransform(nodeTRS, hit_ctx.bbox_node_idx, mCameraContext.inMotionTime);
	const KQuat& scene_rot = nodeTRS.node_rot;
	const KAccelTriangle* pTri = pKDScene->GetAccelTriData(hit_ctx.tri_id);
	UINT32 mesh_idx = pTri->GetMeshIdx();
	UINT32 node_idx = pTri->GetNodeIdx();
	UINT32 tri_idx = pTri->mTriIdx;
	const KTriMesh* pMesh = pKDScene->GetMesh(mesh_idx);
	const KNode* pNode = pKDScene->GetNode(node_idx);
	const UINT32* pn_idx = pMesh->mFaces[tri_idx].pn_idx;
	
	out_shading_ctx.excluding_bbox = hit_ctx.bbox_node_idx;
	out_shading_ctx.excluding_tri = hit_ctx.tri_id;
	assert(out_shading_ctx.excluding_bbox < NOT_HIT_INDEX);
	assert(out_shading_ctx.excluding_tri < NOT_HIT_INDEX);
	
	const KVec3* vert_pos[3] = {
		&pMesh->mVertPN[pMesh->mFaces[tri_idx].pn_idx[0]].pos,
		&pMesh->mVertPN[pMesh->mFaces[tri_idx].pn_idx[1]].pos,
		&pMesh->mVertPN[pMesh->mFaces[tri_idx].pn_idx[2]].pos
	};

	if (!pMesh->mVertTT.empty()) {
		KTriMesh::TT_Data tt_data;
		pMesh->InterpolateTT(tri_idx, hit_ctx, tt_data);
		out_shading_ctx.uv.uv = tt_data.texcoord;
		out_shading_ctx.tangent.tangent = tt_data.tangent;
		out_shading_ctx.tangent.binormal = tt_data.binormal;
		out_shading_ctx.hasUV = 1;
	}
	else
		out_shading_ctx.hasUV = 0;

	KVec3 temp_vec;
	// interpolate the normal
	temp_vec =	pMesh->mVertPN[pn_idx[0]].nor * hit_ctx.w +
				pMesh->mVertPN[pn_idx[1]].nor * hit_ctx.u +
				pMesh->mVertPN[pn_idx[2]].nor * hit_ctx.v;
	out_shading_ctx.normal = temp_vec * pNode->GetObjectRot();
	temp_vec = out_shading_ctx.normal * scene_rot;
	out_shading_ctx.normal = temp_vec;
	float rcp_len_nor = 1.0f / nvmath::length(temp_vec);
	out_shading_ctx.normal *= rcp_len_nor;

	// interpolate the position
	out_shading_ctx.position = pMesh->mVertPN[pn_idx[0]].pos * hit_ctx.w +
						 pMesh->mVertPN[pn_idx[1]].pos * hit_ctx.u +
						 pMesh->mVertPN[pn_idx[2]].pos * hit_ctx.v;
	Vec3TransformCoord(temp_vec, out_shading_ctx.position, pNode->GetObjectTM());
	Vec3TransformCoord(out_shading_ctx.position, temp_vec, nodeTRS.node_tm);
	
	const nvmath::Quatf world_rot = pNode->GetObjectRot() * scene_rot;
	KVec3 edge[3];
	edge[0] = (*vert_pos[1] - *vert_pos[0]) * world_rot;
	edge[1] = (*vert_pos[2] - *vert_pos[1]) * world_rot;
	edge[2] = (*vert_pos[0] - *vert_pos[2]) * world_rot;
	float edge_len_sqr[3];
	edge_len_sqr[0] = nvmath::lengthSquared(edge[0]);
	edge_len_sqr[1] = nvmath::lengthSquared(edge[1]);
	edge_len_sqr[2] = nvmath::lengthSquared(edge[2]);
	float edge_len[3];
	edge_len[0] = sqrt(edge_len_sqr[0]);
	edge_len[1] = sqrt(edge_len_sqr[1]);
	edge_len[2] = sqrt(edge_len_sqr[2]);
	// Calculate the face normal and triangle size
	out_shading_ctx.face_size = edge_len[0] + edge_len[1] + edge_len[2];
	out_shading_ctx.face_size *= 0.05f;
	// Move the shading position along it normal for a epsilon distance.
	out_shading_ctx.position += (out_shading_ctx.normal * out_shading_ctx.face_size * 0.0001f);

	KVec3 face_nor;
	face_nor = edge[0] ^ edge[1];
	out_shading_ctx.face_normal = face_nor;
	out_shading_ctx.face_normal.normalize();

	KVec3 rayDir = hitRay.GetDir();
	rayDir.normalize(); // Normalize the ray direction because it's not normalized

	// Calculate the ddx and ddy
	if (!out_shading_ctx.is_primary_ray && out_shading_ctx.hasUV) {

		KVec3 u_dir, v_dir;
		float rayCoverage = 1.0f;

		float det = fabs(rayDir * out_shading_ctx.face_normal);
		if (det < 0.98f) {
			v_dir = rayDir ^ out_shading_ctx.face_normal;
			v_dir *= (rayCoverage * 2.0f);
			u_dir = v_dir ^ out_shading_ctx.face_normal;
			if (det < 0.0001f) det = 0.0001f;
			u_dir /= det;
		}
		else {
			u_dir = mCameraContext.mEyeRayGen.mHorizonVec * rayCoverage * 2.0f;
			v_dir = mCameraContext.mEyeRayGen.mViewUp * rayCoverage * 2.0f;
		}

		const KVec2* uv_pos[3];
		pMesh->GetUV(tri_idx, uv_pos);
		const KVec2* uv_crd[3] = {(const KVec2*)uv_pos[0], (const KVec2*)uv_pos[1], (const KVec2*)uv_pos[2]};
		KVec2 uv_edge[3];
		uv_edge[0] = *uv_crd[1] - *uv_crd[0];
		uv_edge[1] = *uv_crd[2] - *uv_crd[1];
		uv_edge[2] = *uv_crd[0] - *uv_crd[2];
		float uv_edge_len[3];
		uv_edge_len[0] = nvmath::lengthSquared(uv_edge[0]) / edge_len_sqr[0];
		uv_edge_len[1] = nvmath::lengthSquared(uv_edge[1]) / edge_len_sqr[1];
		uv_edge_len[2] = nvmath::lengthSquared(uv_edge[2]) / edge_len_sqr[2];
		int det_edge;
		if (uv_edge_len[0] >= uv_edge_len[1] && uv_edge_len[0] >= uv_edge_len[2])
			det_edge = 0;
		else if (uv_edge_len[1] >= uv_edge_len[0] && uv_edge_len[1] >= uv_edge_len[2])
			det_edge = 1;
		else 
			det_edge = 2;
		
		int edge_idx0 = (det_edge + 2) % 3;
		int edge_idx1 = (det_edge + 1) % 3;
		float tu[2];
		tu[0] = (edge[edge_idx0] * u_dir) * uv_edge_len[edge_idx0];
		tu[1] = (edge[edge_idx1] * u_dir) * uv_edge_len[edge_idx1];
		float tv[2];
		tv[0] = (edge[edge_idx0] * v_dir) * uv_edge_len[edge_idx0];
		tv[1] = (edge[edge_idx1] * v_dir) * uv_edge_len[edge_idx1];

		float* out_du = (float*)&out_shading_ctx.uv.du;
		float* out_dv = (float*)&out_shading_ctx.uv.dv;
		float div_value = (uv_edge[edge_idx0][0]*uv_edge[edge_idx1][1]) - (uv_edge[edge_idx1][0]*uv_edge[edge_idx0][1]);
		if (fabs(div_value) > 0.001f) {
			float scale_value = 0.5f / div_value;
			out_du[0] = (tu[0]*uv_edge[edge_idx1][1] - tu[1]*uv_edge[edge_idx0][1]) * scale_value;
			out_du[1] = -(tu[0]*uv_edge[edge_idx1][0] - tu[1]*uv_edge[edge_idx0][0]) * scale_value;

			out_dv[0] = (tv[0]*uv_edge[edge_idx1][1] - tv[1]*uv_edge[edge_idx0][1]) * scale_value;
			out_dv[1] = -(tv[0]*uv_edge[edge_idx1][0] - tv[1]*uv_edge[edge_idx0][0]) * scale_value;
		}
		else {
			out_du[0] = 0.0001f;
			out_du[1] = 0.0f;

			out_dv[0] = 0.0f;
			out_dv[1] = 0.0001f;
		}

	}

	// Get the surface shader
	ISurfaceShader* pSurfShader = pNode->mpSurfShader;
	out_shading_ctx.surface_shader = pSurfShader;

	out_shading_ctx.out_vec = -rayDir;
}

const ISurfaceShader* TracingInstance::GetSurfaceShader(const IntersectContext& hit_ctx) const
{
	const KKDBBoxScene* pScene = mpScene;
	const KScene* pKDScene = pScene->GetNodeKDScene(hit_ctx.bbox_node_idx);
	const KAccelTriangle* pTri = pKDScene->GetAccelTriData(hit_ctx.tri_id);
	UINT32 node_idx = pTri->GetNodeIdx();
	const KNode* pNode = pKDScene->GetNode(node_idx);
	return pNode->mpSurfShader;
}

void TracingInstance::CalcuHitInfo(const IntersectContext& hit_ctx, IntersectInfo& out_info) const
{
	const KKDBBoxScene* pScene = mpScene;
	const KScene* pKDScene = pScene->GetNodeKDScene(hit_ctx.bbox_node_idx);
	KAnimation::LocalTRSFrame::LclTRS nodeTRS;
	pScene->GetNodeTransform(nodeTRS, hit_ctx.bbox_node_idx, mCameraContext.inMotionTime);
	const KQuat& scene_rot = nodeTRS.node_rot;
	const KAccelTriangle* pTri = pKDScene->GetAccelTriData(hit_ctx.tri_id);
	UINT32 mesh_idx = pTri->GetMeshIdx();
	UINT32 node_idx = pTri->GetNodeIdx();
	UINT32 tri_idx = pTri->mTriIdx;
	const KTriMesh* pMesh = pKDScene->GetMesh(mesh_idx);
	const KNode* pNode = pKDScene->GetNode(node_idx);
	const UINT32* nor_idx = pMesh->mFaces[tri_idx].pn_idx;
	const UINT32* pos_idx = pMesh->mFaces[tri_idx].pn_idx;
	const nvmath::Quatf world_rot = pNode->GetObjectRot() * scene_rot;

	out_info.pos = pMesh->mVertPN[pos_idx[0]].pos * hit_ctx.w +
		pMesh->mVertPN[pos_idx[1]].pos * hit_ctx.u +
		pMesh->mVertPN[pos_idx[2]].pos * hit_ctx.v;
	KVec3 temp_vec;
	Vec3TransformCoord(temp_vec, out_info.pos, pNode->GetObjectTM());
	Vec3TransformCoord(out_info.pos, temp_vec, nodeTRS.node_tm);

	// interpolate the normal
	temp_vec =	pMesh->mVertPN[nor_idx[0]].nor * hit_ctx.w +
		pMesh->mVertPN[nor_idx[1]].nor * hit_ctx.u +
		pMesh->mVertPN[nor_idx[2]].nor * hit_ctx.v;
	KVec3 normal = temp_vec * pNode->GetObjectRot();
	out_info.nor = normal * scene_rot;
	out_info.nor.normalize();

	const KVec3* vert_pos[3] = {
		&pMesh->mVertPN[pMesh->mFaces[tri_idx].pn_idx[0]].pos,
		&pMesh->mVertPN[pMesh->mFaces[tri_idx].pn_idx[1]].pos,
		&pMesh->mVertPN[pMesh->mFaces[tri_idx].pn_idx[2]].pos
	};
	KVec3 edge[3];
	edge[0] = (*vert_pos[1] - *vert_pos[0]);
	edge[1] = (*vert_pos[2] - *vert_pos[1]);
	KVec3 nor;
	nor = edge[0] ^ edge[1];
	nor.normalize();
	out_info.face_nor = nor * world_rot;

	out_info.bbox_node_idx = hit_ctx.bbox_node_idx;
	out_info.tri_id = hit_ctx.tri_id;
}
	
bool TracingInstance::CastRay(const KRay& ray, IntersectContext& out_ctx) const
{
	if (mpScene->IntersectRay_KDTree(ray, out_ctx, mCameraContext.inMotionTime)) {

		// update the distance that the ray has traveled.
		out_ctx.travel_distance += out_ctx.t;

		return true;
	}
	else
		return false;
}

bool TracingInstance::IsPointOccluded(const KRay& ray, float len) const
{
	IntersectContext ctx;
	if (CastRay(ray, ctx)) {
		float diff = ctx.t - len;
		float epsilon = mpScene->GetSceneEpsilon();
		if (diff > epsilon)
			return false;
		else
			return true;
	}
	else
		return false;
}

bool KSC_Shader::Load(const char* shaderFile)
{
	mpUniformData = NULL;
	FILE* f = NULL;
	fopen_s(&f, shaderFile, "r");
	if (f == NULL)
		return false;
	char line[256];
	// The first line must be "Shader = xxx", where xxx should be the file that contains KSC code.
	fgets(line, 256, f);
	char kscFile[256];
	if (sscanf_s(line, "Shader%*[ ]=%*[ ]%s", kscFile) != 1)
		return false;
	std::string shaderPath = "./shader/";
	shaderPath += kscFile;
	ModuleHandle shaderModule = KSC_Compile(shaderPath.c_str());
	if (shaderModule == NULL) {
		printf("Shader compilation failed with following error:\n");
		printf(KSC_GetLastErrorMsg());
		return false;
	}
		
	FunctionHandle shadeFunc = KSC_GetFunctionHandleByName("Shade", shaderModule);
	if (shadeFunc == NULL) {
		printf("Shade function does not exist in KSC code.\n");
		return false;
	}

	// The Shade function should have three arguments:
	// arg0 - the reference to structure that contains uniform data
	// arg1 - shader input data
	// arg2 - shader output data
	if (KSC_GetFunctionArgumentCount(shadeFunc) != 3) {
		printf("Incorrect argument count for Shade function.\n");
		return false;
	}

	KSC_TypeInfo arg0TypeInfo = KSC_GetFunctionArgumentType(shadeFunc, 0);
	if (!arg0TypeInfo.isRef || !arg0TypeInfo.isKSCLayout) {
		printf("Incorrect type for argument 0.\n");
		return false;
	}
	for (int i = 1; i < 3; ++i) {
		KSC_TypeInfo typeInfo = KSC_GetFunctionArgumentType(shadeFunc, i);
		if (!typeInfo.isRef) {
			printf("Incorrect type for argument %d.\n", i);
			return false;
		}
	}

	// Perform the additional check
	if (!Validate(shadeFunc)) {
		printf("Shade function validation failed.\n");
		return false;
	}

	while (!feof(f)) {
		// Parse each line and set the initial uniform values
		fgets(line, 256, f);
		
	}
	return true;
}

bool KSC_Shader::Validate(FunctionHandle shadeFunc)
{
	return true;
}

KSC_Shader::~KSC_Shader()
{

}

void KSC_Shader::Execute(void* inData, void* outData)
{

}
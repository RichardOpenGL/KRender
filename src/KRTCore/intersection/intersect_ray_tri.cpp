#include "intersect_ray_tri.h"
#include <xmmintrin.h>
#include <tmmintrin.h>


/* Ray-Triangle Intersection Test Routines          */
/* Different optimizations of my and Ben Trumbore's */
/* code from journals of graphics tools (JGT)       */
/* http://www.acm.org/jgt/                          */
/* by Tomas Moller, May 2000                        */

#define EPSILON 0.000001
#define CROSS(dest,v1,v2) \
          dest[0]=v1[1]*v2[2]-v1[2]*v2[1]; \
          dest[1]=v1[2]*v2[0]-v1[0]*v2[2]; \
          dest[2]=v1[0]*v2[1]-v1[1]*v2[0];
#define DOT(v1,v2) (v1[0]*v2[0]+v1[1]*v2[1]+v1[2]*v2[2])
#define SUB(dest,v1,v2) \
          dest[0]=v1[0]-v2[0]; \
          dest[1]=v1[1]-v2[1]; \
          dest[2]=v1[2]-v2[2]; 

/* the original jgt code */
int intersect_triangle(const float* orig, const float* dir,
		       const float* vert0, const float* vert1, const float* vert2,
		       float *t, float *u, float *v)
{
   float edge1[3], edge2[3], tvec[3], pvec[3], qvec[3], nface[3];
   float det,inv_det;

   /* find vectors for two edges sharing vert0 */
   SUB(edge1, vert1, vert0);
   SUB(edge2, vert2, vert0);

   /* begin calculating determinant - also used to calculate U parameter */
   CROSS(pvec, dir, edge2);
   CROSS(nface, edge1, edge2);
   if (DOT(nface, dir) > 0)
	   return 0; // Backward face, ignore it

   /* if determinant is near zero, ray lies in plane of triangle */
   det = DOT(edge1, pvec);

   if (det > -EPSILON && det < EPSILON)
     return 0;
   inv_det = 1.0f / det;

   /* calculate distance from vert0 to ray origin */
   SUB(tvec, orig, vert0);

   /* calculate U parameter and test bounds */
   *u = DOT(tvec, pvec) * inv_det;
   if (*u < 0.0 || *u > 1.0)
     return 0;

   /* prepare to test V parameter */
   CROSS(qvec, tvec, edge1);

   /* calculate V parameter and test bounds */
   *v = DOT(dir, qvec) * inv_det;
   if (*v < 0.0 || *u + *v > 1.0)
     return 0;

   /* calculate t, ray intersects triangle */
   *t = DOT(edge2, qvec) * inv_det;

   return 1;
}



bool RayIntersect(const KRay& ray, const KAccleTriVertPos& tri, UINT32 tri_id, IntersectContext& ctx)
{
	float t = 0;
	float u, v;
	if (intersect_triangle(ray.GetOrg().getPtr(), ray.GetDir().getPtr(), 
		tri.mVertPos[0].getPtr(), tri.mVertPos[1].getPtr(), tri.mVertPos[2].getPtr(), 
		&t, &u, &v)) {
			if (t > 0 && ctx.t > t) {
				ctx.t = t;
				ctx.u = u;
				ctx.v = v;
				ctx.w = 1.0f - u - v;
				ctx.tri_id = tri_id;
				return true;
			}
			else
				return false;
	}
	else
		return false;
}

// The following code is from web site: http://lucille.atso-net.jp/svn/angelina/isect/wald/raytri.cpp
// http://code.google.com/p/raytracersquare/source/browse/RT2_branch/Intersections.cpp
#include <stdio.h>

/* config */
//#define ENABLE_EARLY_EXIT


static void normalize(float v[3])
{
	float norm, inv_norm;

	norm = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
	norm = sqrt(norm);

	if (norm > 1.0e-5) {
		inv_norm = 1.0f / norm;
		v[0] *= inv_norm;
		v[1] *= inv_norm;
		v[2] *= inv_norm;
	}
}

static void vcross(float v[3], float a[3], float b[3])
{
	v[0] = a[1] * b[2] - a[2] * b[1];
	v[1] = a[2] * b[0] - a[0] * b[2];
	v[2] = a[0] * b[1] - a[1] * b[0];

	normalize(v);
}

// should be 16 byte aligned
static const UINT32 modulo3[]  = {0, 1, 2, 0, 1};

inline static bool intersect_one_ray(const KAccelTriangleOpt &acc, const KRay &ray, IntersectContext& ctx)
{
	// determine U and V axii         
	const int uAxis = modulo3[acc.k + 1];        
	const int vAxis = modulo3[acc.k + 2];          
	
	// start high-latency division as early as possible         
	const float nd = 1.0f / (ray.GetDir()[acc.k] + acc.n_u * ray.GetDir()[uAxis] + acc.n_v * ray.GetDir()[vAxis]); 
	if (nd * acc.E_F > 0)
		return false; // Backward face, skip it.

	const float f = (acc.n_d - ray.GetOrg()[acc.k] - acc.n_u * ray.GetOrg()[uAxis] - acc.n_v * ray.GetOrg()[vAxis]) * nd;                  
	
	// check for valid distance.         
	if (f < EPSILON || f > ctx.t) 
		return false;          
	
	// compute hitpoint positions on uv plane         
	const float hu = (ray.GetOrg()[uAxis] + f * ray.GetDir()[uAxis]);         
	const float hv = (ray.GetOrg()[vAxis] + f * ray.GetDir()[vAxis]);                  
	// check first barycentric coordinate         
	ctx.u = (hv * acc.b_nu + hu * acc.b_nv + acc.b_d);                  
	if (ctx.u <= 0)         return false;         
	
	// check second barycentric coordinate         
	ctx.v = (hv * acc.c_nu + hu * acc.c_nv + acc.c_d);          
	//      std::cout << "Fast: " << mue << std::endl;                  
	
	if (ctx.v <= 0.)         return false;         
	// check third barycentric coordinate         
	if (ctx.u + ctx.v >= 1.)         return false;         
	// have a valid hitpoint here. store it.  
	ctx.t = f;
	return true; 
}

#define X 0
#define Y 1
#define Z 2
void PrecomputeAccelTri(const KAccleTriVertPos& tri, UINT32 tri_id, KAccelTriangleOpt &triAccel)
{
	// calculate triAccel-struture         
	KVec3 c = tri.mVertPos[1] - tri.mVertPos[0];
	KVec3 b = tri.mVertPos[2] - tri.mVertPos[0];                 
	// calculate normal-vector         
	KVec3 normal = c ^ b;

	//projection-dimension         
	triAccel.k = (fabs(normal[0]) > fabs(normal[1])) ? ((fabs(normal[0]) > fabs(normal[2])) ? X : Z) : ((fabs(normal[1]) > fabs(normal[2])) ? Y : Z);                  
	KVec3 nn = normal / normal[triAccel.k];                  
	// n_u, n_v and n_d         
	const int uAxis = modulo3[triAccel.k + 1];         
	const int vAxis = modulo3[triAccel.k + 2];         
	triAccel.n_u = nn[uAxis];         
	triAccel.n_v = nn[vAxis];    
	KVec3 tempVec(tri.mVertPos[0][X], tri.mVertPos[0][Y], tri.mVertPos[0][Z]);
	triAccel.n_d = tempVec * nn;          
	
	// first line equation         
	float reci = 1.0f / (b[uAxis] * c[vAxis] - b[vAxis] * c[uAxis]);         
	triAccel.b_nu = b[uAxis] * reci;         triAccel.b_nv = -b[vAxis] * reci;         
	triAccel.b_d = (b[vAxis] * tri.mVertPos[0][uAxis] - b[uAxis] * tri.mVertPos[0][vAxis]) * reci;         
	
	// second line equation         
	triAccel.c_nu = c[uAxis] * -reci;         
	triAccel.c_nv = c[vAxis] * reci;         
	triAccel.c_d = (c[vAxis] * tri.mVertPos[0][uAxis] - c[uAxis] * tri.mVertPos[0][vAxis]) * -reci; 

	// triangle id
	triAccel.tri_id = tri_id;

	// calculate the max edge length
	float le0 = nvmath::lengthSquared(tri.mVertPos[0] - tri.mVertPos[1]);
	float le1 = nvmath::lengthSquared(tri.mVertPos[0] - tri.mVertPos[2]);
	float le2 = nvmath::lengthSquared(tri.mVertPos[2] - tri.mVertPos[1]);
	float maxl = (le0 > le1 ? le0 : le1);
	maxl = (maxl > le2 ? maxl : le2);
	triAccel.E_F = sqrtf(maxl);
	if (normal[triAccel.k] < 0)
		triAccel.E_F = -triAccel.E_F;

}

bool RayIntersect(const KRay& ray, const KAccelTriangleOpt& tri, IntersectContext& ctx)
{
	if (intersect_one_ray(tri, ray, ctx)) {
		ctx.w = 1.0f - ctx.u - ctx.v;
		ctx.tri_id = tri.tri_id;
		return true;
	}
	return false;

}


//   Reference:
//   Carsten Benthin,
//   "Realtime Ray Tracing on current CPU Architectures"
//   Ph.D. thesis
//

typedef UINT32 uint32;



typedef struct Ray4
{
	vec4f dir[3];
	vec4f org[3];
} Ray4;

typedef struct Hit4
{
	vec4f t, u, v;
	vec4f mask;
} Hit4;

static const UINT32 SIGN_BIT = 0x80000000;
static const vec4f vone			= {1, 1, 1, 1};

// Do 1 ray - 4 triangle test 
inline static bool intersect_four_tri(
		// output 
		IntersectContext& ctx, 

		// input
		const KRay& ray,
		const KAccelTriangleOpt1r4t* acc, UINT32 tri4_cnt)
{

	// determine U and V axis        
	vec4 ray_dir_k, ray_dir_u, ray_dir_v;
	vec4 ray_org_k, ray_org_u, ray_org_v;
	vec4f old_t = vec4_splats(ctx.t);
	vec4f old_u = vec4_zero();
	vec4f old_v = vec4_zero();
	vec4f old_tri_id = vec4_zero();
	vec4f temp_vec0, temp_vec1;
	vec4f mask;
	vec4 self_tri_id; 
	const vec4f sign_bit = vec4_splats(*(float*)&SIGN_BIT);

	self_tri_id.asFloat4 = vec4_splats(*((float*)&ctx.self_tri_id));
	
	for (UINT32 i = 0; i < tri4_cnt; ++i) {

		const vec4i& k_shuffled = acc[i].k;
		ray_dir_k.asUINT4 = _mm_shuffle_epi8(ray.GetDirVec4().asUINT4, k_shuffled);
		ray_org_k.asUINT4 = _mm_shuffle_epi8(ray.GetOrgVec4().asUINT4, k_shuffled);

		ray_dir_u.asUINT4 = _mm_shuffle_epi8(ray.mDir_shift1.asUINT4, k_shuffled);
		ray_org_u.asUINT4 = _mm_shuffle_epi8(ray.mOrign_shift1.asUINT4, k_shuffled);

		ray_dir_v.asUINT4 = _mm_shuffle_epi8(ray.mDir_shift2.asUINT4, k_shuffled);
		ray_org_v.asUINT4 = _mm_shuffle_epi8(ray.mOrign_shift2.asUINT4, k_shuffled);

		//---------------start high-latency division as early as possible--------------------
		//const float nd = 1.0f / (ray.GetDir()[acc.k] + acc.n_u * ray.GetDir()[uAxis] + acc.n_v * ray.GetDir()[vAxis]);         
		temp_vec0 = vec4_madd(acc[i].n_u, ray_dir_u.asFloat4, ray_dir_k.asFloat4);
		const vec4f rcp_nd = vec4_madd(acc[i].n_v, ray_dir_v.asFloat4, temp_vec0);
		temp_vec0 = vec4_mul(rcp_nd, acc[i].E_F);
		mask = vec4_cmpge(vec4_zero(), temp_vec0);

		//const float f = (acc.n_d - ray.GetOrg()[acc.k] - acc.n_u * ray.GetOrg()[uAxis] - acc.n_v * ray.GetOrg()[vAxis]) * nd;  
		temp_vec0 = vec4_madd(ray_org_u.asFloat4, acc[i].n_u, ray_org_k.asFloat4);
		temp_vec1 = vec4_madd(acc[i].n_v, ray_org_v.asFloat4, temp_vec0);
		temp_vec0 = vec4_sub(acc[i].n_d, temp_vec1);
		const vec4f f = vec4_div(temp_vec0, rcp_nd);
		//----------------check for valid distance. ------------------------------------------        
		//if (f < EPSILON || f > t) return false;   
		temp_vec0 = _mm_cmpneq_ps(self_tri_id.asFloat4, acc[i].tri_id);
		mask = vec4_and(mask, vec4_and(vec4_cmpgt(old_t, f), vec4_cmpgt(f, vec4_zero())));
		mask = vec4_and(mask, temp_vec0);
		if (!vec4_gather(mask)) 
			continue;
		
		// compute hitpoint positions on uv plane         
		//const float hu = (ray.GetOrg()[uAxis] + f * ray.GetDir()[uAxis]); 
		const vec4f hu = vec4_madd(f, ray_dir_u.asFloat4, ray_org_u.asFloat4);
		//const float hv = (ray.GetOrg()[vAxis] + f * ray.GetDir()[vAxis]);                  
		const vec4f hv = vec4_madd(f, ray_dir_v.asFloat4, ray_org_v.asFloat4);

		// calculate uv test epsilon
		vec4f uv_epsilon = vec4_mul(f, _mm_or_ps(acc[i].E_F, sign_bit));
		//vec4f uv_eps_zero = vec4_sub(vec4_zero(), uv_epsilon);

		//----------------check first barycentric coordinate  --------------------------------       
		//u = (hv * acc.b_nu + hu * acc.b_nv + acc.b_d);                  
		temp_vec0 = vec4_madd(hu, acc[i].b_nv, acc[i].b_d);
		const vec4f u = vec4_madd(hv, acc[i].b_nu, temp_vec0);
		//if (u <= 0.)         return false;    
		mask = vec4_and(mask, vec4_cmpge(u, uv_epsilon));
		
		//-----------------check second barycentric coordinate --------------------------------
		//v = (hv * acc.c_nu + hu * acc.c_nv + acc.c_d);  
		temp_vec0 = vec4_madd(hu, acc[i].c_nv, acc[i].c_d);
		const vec4f v = vec4_madd(hv, acc[i].c_nu, temp_vec0);

		//if (v <= 0.)         return false;  
		mask = vec4_and(mask, vec4_cmpge(v, uv_epsilon));
		//-----------------check third barycentric coordinate ----------------------------------
		//if (u + v >= 1.)         return false; 
		temp_vec0 = vec4_sub(vone, vec4_add(u, v));
		mask = vec4_and(mask, vec4_cmpge(temp_vec0, uv_epsilon));
		
		old_t = vec4_sel(old_t, f, mask);
		old_u = vec4_sel(old_u, u, mask);
		old_v = vec4_sel(old_v, v, mask);
		old_tri_id = vec4_sel(old_tri_id, acc[i].tri_id, mask);

	}
	
	// have a valid hit point here. store it.  
	float min_t = ctx.t;
	int min_idx = -1;
	for (int i = 0; i < 4; ++i) {
		if (min_t > vec4_f(old_t, i)) {
			min_t = vec4_f(old_t, i);
			min_idx = i;
		}
	}

	if (min_idx != -1) {
		ctx.t = vec4_f(old_t, min_idx);
		ctx.u = vec4_f(old_u, min_idx);
		ctx.v = vec4_f(old_v, min_idx);
		ctx.w = 1.0f - ctx.u - ctx.v;
		ctx.tri_id = vec4_i(old_tri_id, min_idx);
		return true;
	}

	return false;
}


bool RayIntersect4Tri(const KRay& ray, const KAccelTriangleOpt1r4t* tri, UINT32 tri4_cnt, IntersectContext& ctx)
{
	if (intersect_four_tri(ctx, ray, tri, tri4_cnt)) {
		return true;
	}
	else
		return false;
}

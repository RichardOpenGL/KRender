#include "AbcLoader.h"


#include <Alembic/AbcCoreHDF5/All.h>
#include <Alembic/Abc/ISchemaObject.h>
#include <Alembic/AbcGeom/IPolyMesh.h>
#include <Alembic/AbcGeom/ICamera.h>
#include <Alembic/AbcGeom/ILight.h>
#include <Alembic/AbcCoreAbstract/TimeSampling.h>

#include "../scene/KKDBBoxScene.h"
#include <assert.h>


AbcLoader::AbcLoader()
{
	mCurTime = 0;
	mpScene = NULL;
	mSampleDuration = 1.0 / 24.0;
}

AbcLoader::~AbcLoader()
{

}

bool AbcLoader::Load(const char* filename, KSceneSet& scene)
{
	mpScene = &scene;
	Abc::IArchive archive(Alembic::AbcCoreHDF5::ReadArchive(), filename);

	std::string appName;
	std::string libraryVersionString;
	Alembic::Util::uint32_t libraryVersion;
	std::string whenWritten;
	std::string userDescription;
	Abc::GetArchiveInfo(archive,
				appName,
				libraryVersionString,
				libraryVersion,
				whenWritten,
				userDescription);
	if (appName != "") {
		std::cout << ">>> file written by: " << appName << std::endl;
		std::cout << ">>> using Alembic : " << libraryVersionString << std::endl;
		std::cout << ">>> written on : " << whenWritten << std::endl;
		std::cout << ">>> user description : " << userDescription << std::endl;
		std::cout << std::endl;
	}

	Abc::IObject topObj(archive, Abc::kTop);

	ProcessNode(topObj);
	return true;
}

void AbcLoader::ProcessNode(const Abc::IObject& obj, int treeDepth)
{
	if ( !obj ) { return; }

    // IObject has no explicit time sampling, but its children may.
    size_t numChildren = obj.getNumChildren();
    for (size_t i = 0; i < numChildren; ++i) {
        const Abc::ObjectHeader &ohead = obj.getChildHeader(i);

		for (int s = 0; s < treeDepth; ++s) std::cout << " ";

        if ( AbcG::IPolyMesh::matches(ohead) ) {
            AbcG::IPolyMesh pmesh(obj, ohead.getName());
            if (pmesh) {
				std::cout << "mesh: " << pmesh.getName() << std::endl; 
				ProcessMesh(pmesh);
            }
        }
        else if (AbcG::IPoints::matches(ohead)) {
            AbcG::IPoints points(obj, ohead.getName());
            if (points) {
                std::cout << "points: " << points.getName() << std::endl; 
            }
        }
        else if (AbcG::ICurves::matches(ohead) ) {
            AbcG::ICurves curves(obj, ohead.getName());
            if (curves) {
                std::cout << "curves: " << curves.getName() << std::endl; 
            }
        }
        else if (AbcG::INuPatch::matches(ohead)) {
            AbcG::INuPatch nuPatch(obj, ohead.getName());
            if (nuPatch) {
				std::cout << "nuPatch: " << nuPatch.getName() << std::endl; 
            }
        }
        else if (AbcG::IXform::matches(ohead)) {
            AbcG::IXform xform(obj, ohead.getName());
            if (xform) {
				std::cout << "xform: " << xform.getName() << std::endl;
            }
        }
        else if (AbcG::ISubD::matches(ohead)) {
            AbcG::ISubD subd(obj, ohead.getName());
            if (subd) {
                std::cout << "subd: " << subd.getName() << std::endl; 
            }
        }
		else if (AbcG::ICamera::matches(ohead)) {
			AbcG::ICamera camera(obj, ohead.getName());
			if (camera) {
                std::cout << "camera: " << camera.getName() << std::endl; 
            }
		}
		else if (AbcG::ILight::matches(ohead)) {
			AbcG::ILight light(obj, ohead.getName());
			if (light) {
                std::cout << "light: " << light.getName() << std::endl; 
            }
		}
       
        {
			// Look for the children of current object
			Abc::IObject childObj(obj, ohead.getName());
			ProcessNode(childObj, treeDepth + 1);
        }
    }
}


KScene* AbcLoader::GetXformStaticScene(const Abc::IObject& obj, KMatrix4& mat)
{
	Imath::M44d localMat;
	localMat.makeIdentity();
	AbcA::ObjectReaderPtr animNode;
	for (Abc::IObject node = obj; node.valid(); node = node.getParent()) {

		if (AbcG::IXform::matches(node.getHeader())) {
			AbcG::IXform xform(node, Abc::kWrapExisting);
			
			if (!xform.getSchema().isConstant()) {
				animNode = xform.getPtr();
				break;
			}
			localMat *= xform.getSchema().getValue().getMatrix();
		}
	}


	if (mXformNodes.find(animNode.get()) == mXformNodes.end()) {
		UINT32 sceneIdx;
		KScene* xformScene = mpScene->AddKDScene(sceneIdx);
		mXformNodes[animNode.get()] = xformScene;
		if (animNode.get()) {
			AbcG::IXform xform(animNode, Abc::kWrapExisting);
			KMatrix4 sceneNodeFrames[2];
			GetXformWorldTransform(xform, sceneNodeFrames);
			UINT32 sceneNodeIdx = mpScene->SceneNode_Create(sceneIdx);
			mpScene->SceneNodeTM_SetMovingNode(sceneNodeIdx, sceneNodeFrames[0], sceneNodeFrames[1]);
		}
		else {
			// Create the default node with identify transform.
			UINT32 sceneNodeIdx = mpScene->SceneNode_Create(sceneIdx);
			mpScene->SceneNodeTM_SetStaticNode(sceneNodeIdx, nvmath::cIdentity44f);
		}

	}

	ConvertMatrix(localMat, mat);
	KScene* xformScene = mXformNodes[animNode.get()];
	return xformScene;
}

void AbcLoader::ProcessMesh(const AbcG::IPolyMesh& mesh)
{
	KMatrix4 localMat;
	KScene* pScene = GetXformStaticScene(mesh, localMat);
	assert(pScene);

	UINT32 meshIdx = pScene->AddMesh();
	KTriMesh* pMesh = pScene->GetMesh(meshIdx);
	assert(pMesh);

	UINT32 nodeIdx = pScene->AddNode();
	KNode* pNode = pScene->GetNode(nodeIdx);
	pNode->mMesh.push_back(meshIdx);
	pScene->SetNodeTM(nodeIdx, localMat);
	pNode->mpSurfShader = NULL;

	ConvertMesh(mesh.getSchema(), mCurTime, *pMesh);
}

void AbcLoader::ConvertMatrix(const Imath::M44d& ilmMat, KMatrix4& mat)
{
	for (int i = 0; i < 16; ++i)
		((float*)&mat)[i] = (float)ilmMat.getValue()[i];
}

bool AbcLoader::ConvertMesh(const AbcG::IPolyMeshSchema& meshSchema, Abc::chrono_t cur_t, KTriMesh& outMesh)
{
	Abc::Int32ArraySamplePtr startFrameFaceIdx;
	Abc::Int32ArraySamplePtr startFrameFaceCnts;
	bool isAnimating = !meshSchema.isConstant();

	size_t frameCnt = isAnimating ? 2 : 1;
	for (size_t frame_i = 0; frame_i < frameCnt; ++frame_i) {

		Abc::chrono_t t = cur_t + double(frame_i) * mSampleDuration;
		// Get the data of vertex positions and faces
		//
		AbcG::IPolyMeshSchema::Sample meshSample_0;
		AbcG::IPolyMeshSchema::Sample meshSample_1;
		Abc::ISampleSelector ss_0(t, Abc::ISampleSelector::kFloorIndex);
		Abc::ISampleSelector ss_1(t, Abc::ISampleSelector::kCeilIndex);
		std::pair<Abc::index_t, Abc::chrono_t> idx_0 = meshSchema.getTimeSampling()->getFloorIndex(t, meshSchema.getNumSamples());
		std::pair<Abc::index_t, Abc::chrono_t> idx_1 = meshSchema.getTimeSampling()->getCeilIndex(t, meshSchema.getNumSamples());
		float alpha = (idx_0.first != idx_1.first) ? (float(t - idx_0.second) / float(idx_1.second - idx_0.second)) : 0;

		meshSchema.get(meshSample_0, idx_0.first);
		meshSchema.get(meshSample_1, idx_1.first);

		Abc::Int32ArraySamplePtr faceIdx_0 = meshSample_0.getFaceIndices();
		Abc::Int32ArraySamplePtr faceCnts_0 = meshSample_0.getFaceCounts();

		Abc::Int32ArraySamplePtr faceIdx_1 = meshSample_1.getFaceIndices();
		Abc::Int32ArraySamplePtr faceCnts_1 = meshSample_1.getFaceCounts();

		Abc::P3fArraySamplePtr vertPos_0 = meshSample_0.getPositions();
		Abc::P3fArraySamplePtr vertPos_1 = meshSample_1.getPositions();

		// Determine if the mesh's topology has been changed
		if (!CompareAbcArrary(faceIdx_0, faceIdx_1) ||
			!CompareAbcArrary(faceCnts_0, faceCnts_1) ||
			vertPos_0->size() != vertPos_1->size()) {
			// Force to use the floor sample if topology is changed
			vertPos_1 = vertPos_0;
		}		

		if (frame_i == 0) {
			startFrameFaceIdx = faceIdx_0;
			startFrameFaceCnts = faceCnts_0;
		}
		else {
			if (!CompareAbcArrary(faceIdx_0, startFrameFaceIdx) ||
				!CompareAbcArrary(faceCnts_0, startFrameFaceCnts)) {
				// Topology has been changed, have to consider this mesh as static one 
				// since motion blur for topology-changing goemetry is not supported.
				std::cout << "Warning: geometry topology has been changed for mesh:" << meshSchema.getName() << std::endl;
				outMesh.MakeAsStatic();
				break;
			}		
		}

		// Calculate the triangle count
		size_t triCnt = 0;
		size_t faceCnt = faceCnts_0->size();
		size_t polyVertCnt = 0;
		for (size_t fi = 0; fi < faceCnt; ++fi) {
			assert((*faceCnts_0)[fi] > 2);
			triCnt += ((*faceCnts_0)[fi] - 2);

			polyVertCnt += (*faceCnts_0)[fi];
		}
		outMesh.mFaces.resize(triCnt);

		//
		// Now for normal data
		//
		AbcG::IN3fGeomParam normParam = meshSchema.getNormalsParam();
		if (normParam.getNumSamples() > 0) { 

			// There is at least one normal sample, so take use of it

			if (normParam.getScope() != Alembic::AbcGeom::kVertexScope &&
				normParam.getScope() != Alembic::AbcGeom::kVaryingScope &&
				normParam.getScope() != Alembic::AbcGeom::kFacevaryingScope ){
				std::cout << "Normal vector has an unsupported scope, skipping mesh: " << meshSchema.getName() << std::endl;
				return false;
			}

			Alembic::AbcGeom::IN3fGeomParam::Sample normSamp_0;
			Alembic::AbcGeom::IN3fGeomParam::Sample normSamp_1;
			normParam.getExpanded(normSamp_0, idx_0.first);
			normParam.getExpanded(normSamp_1, idx_1.first);
			Alembic::Abc::N3fArraySamplePtr normVal_0 = normSamp_0.getVals();
			Alembic::Abc::N3fArraySamplePtr normVal_1 = normSamp_1.getVals();
			size_t normCnt_0 = normVal_0->size();
			size_t normCnt_1 = normVal_1->size();

			if ((normParam.getScope() == Alembic::AbcGeom::kVertexScope || 
				normParam.getScope() == Alembic::AbcGeom::kVaryingScope) &&
				normCnt_0 == vertPos_0->size() && normCnt_1 == vertPos_1->size() ) {

				// This mesh has per-vertex normal
				outMesh.SetupPN(normCnt_0, isAnimating);

				for (size_t i = 0; i < normCnt_0; ++i) {
					outMesh.GetVertPN(i)[frame_i].pos[0] = nvmath::lerp(alpha, (*vertPos_0)[i].x, (*vertPos_1)[i].x);
					outMesh.GetVertPN(i)[frame_i].pos[1] = nvmath::lerp(alpha, (*vertPos_0)[i].y, (*vertPos_1)[i].y);
					outMesh.GetVertPN(i)[frame_i].pos[2] = nvmath::lerp(alpha, (*vertPos_0)[i].z, (*vertPos_1)[i].z);

					outMesh.GetVertPN(i)[frame_i].nor[0] = nvmath::lerp(alpha, (*normVal_0)[i].x, (*normVal_1)[i].x);
					outMesh.GetVertPN(i)[frame_i].nor[1] = nvmath::lerp(alpha, (*normVal_0)[i].y, (*normVal_1)[i].y);
					outMesh.GetVertPN(i)[frame_i].nor[2] = nvmath::lerp(alpha, (*normVal_0)[i].z, (*normVal_1)[i].z);
					outMesh.GetVertPN(i)[frame_i].nor.normalize();
				}

				if (frame_i == 0) {
					size_t triIter = 0;
					size_t fiIter = 0;
					for (size_t fi = 0; fi < faceCnt; ++fi) {
						for (int vi = 1; vi < (*faceCnts_0)[fi] - 1; ++vi) {
							outMesh.mFaces[triIter].pn_idx[0] = (*faceIdx_0)[fiIter];
							outMesh.mFaces[triIter].pn_idx[2] = (*faceIdx_0)[fiIter + vi];
							outMesh.mFaces[triIter].pn_idx[1] = (*faceIdx_0)[fiIter + vi + 1];
							++triIter;
						}
						fiIter += (*faceCnts_0)[fi];
					}
				}
			}
			else if (normCnt_0 == polyVertCnt && normCnt_1 == polyVertCnt &&
				normParam.getScope() == Alembic::AbcGeom::kFacevaryingScope) {
				// This mesh has per-face normal
				outMesh.SetupPN(triCnt*3, isAnimating);
				size_t triIter = 0;
				size_t fiIter = 0;
				size_t viIter = 0;
				for (size_t fi = 0; fi < faceCnt; ++fi) {
					for (int vi = 1; vi < (*faceCnts_0)[fi] - 1; ++vi) {
						int v0 = (*faceIdx_0)[fiIter];
						int v1  = (*faceIdx_0)[fiIter + vi];
						int v2  = (*faceIdx_0)[fiIter + vi + 1];

						int v[] = {v0, v1, v2};
						if (frame_i == 0) {
							outMesh.mFaces[triIter].pn_idx[0] = (UINT32)viIter;
							outMesh.mFaces[triIter].pn_idx[2] = (UINT32)viIter + 1;
							outMesh.mFaces[triIter].pn_idx[1] = (UINT32)viIter + 2;
						}

						for (int ii = 0; ii < 3; ++ii) {
							outMesh.GetVertPN(viIter)[frame_i].pos[0] = nvmath::lerp(alpha, (*vertPos_0)[v[ii]].x, (*vertPos_1)[v[ii]].x);
							outMesh.GetVertPN(viIter)[frame_i].pos[1] = nvmath::lerp(alpha, (*vertPos_0)[v[ii]].y, (*vertPos_1)[v[ii]].y);
							outMesh.GetVertPN(viIter)[frame_i].pos[2] = nvmath::lerp(alpha, (*vertPos_0)[v[ii]].z, (*vertPos_1)[v[ii]].z);

							outMesh.GetVertPN(viIter)[frame_i].nor[0] = nvmath::lerp(alpha, (*normVal_0)[viIter].x, (*normVal_1)[viIter].x);
							outMesh.GetVertPN(viIter)[frame_i].nor[1] = nvmath::lerp(alpha, (*normVal_0)[viIter].y, (*normVal_1)[viIter].y);
							outMesh.GetVertPN(viIter)[frame_i].nor[2] = nvmath::lerp(alpha, (*normVal_0)[viIter].z, (*normVal_1)[viIter].z);

							viIter++;
						}
						++triIter;
					}
					fiIter += (*faceCnts_0)[fi];
				}
			}
		}
		else {

			// There's no normal sample, I need to calcuate the normal from faces and the topology(shared vertices)

			size_t vertCnt = vertPos_0->size();
			std::vector<unsigned int> sharedCnts(vertCnt);
			std::vector<KVec3> averagedNormals(vertCnt);
			for (size_t i = 0; i < vertCnt; ++i) {
				sharedCnts[i] = 0;
				averagedNormals[i] = KVec3(0,0,0);
			}

			Abc::P3fArraySamplePtr tmpVertPos[2] = {vertPos_0, vertPos_1};
			float tmpWeight[2] = {alpha, 1.0f - alpha};
			size_t fiIter = 0;
			for (size_t fi = 0; fi < faceCnt; ++fi) {
				assert((*faceCnts_0)[fi] > 2);
				int v0 = (*faceIdx_0)[fiIter];
				int v1  = (*faceIdx_0)[fiIter + 1];
				int v2  = (*faceIdx_0)[fiIter + 2];

			

				for (size_t ni = 0; ni < 2; ++ni) {
					Imath::V3f vert0 = (*tmpVertPos[ni])[v0];
					Imath::V3f vert1 = (*tmpVertPos[ni])[v1];
					Imath::V3f vert2 = (*tmpVertPos[ni])[v2];
					Imath::V3f normTmp = (vert1 - vert0).cross(vert2 - vert0);
					KVec3 norm = KVec3(normTmp.x, normTmp.y, normTmp.z);
					norm.normalize();
					norm *= tmpWeight[ni];
			
					for (int vi = 0; vi < (*faceCnts_0)[fi]; ++vi) {
						size_t vIdx = (*faceIdx_0)[fiIter + vi];
						averagedNormals[vIdx] += norm;
						sharedCnts[vIdx]++;
					}
				}
				fiIter += (*faceCnts_0)[fi];
			}

			for (size_t i = 0; i < vertCnt; ++i) {
				if (sharedCnts[i] > 1)
					averagedNormals[i] /= (float)sharedCnts[i];
			}

			// Now fill the data to KTriMesh object
			outMesh.SetupPN(vertCnt, isAnimating);
			for (size_t i = 0; i < vertCnt; ++i) {
				outMesh.GetVertPN(i)[frame_i].pos[0] = nvmath::lerp(alpha, (*vertPos_0)[i].x, (*vertPos_0)[i].x);
				outMesh.GetVertPN(i)[frame_i].pos[1] = nvmath::lerp(alpha, (*vertPos_0)[i].y, (*vertPos_0)[i].y);
				outMesh.GetVertPN(i)[frame_i].pos[2] = nvmath::lerp(alpha, (*vertPos_0)[i].z, (*vertPos_0)[i].z);

				outMesh.GetVertPN(i)[frame_i].nor = -averagedNormals[i];
				outMesh.GetVertPN(i)[frame_i].nor.normalize();
			}

			if (frame_i == 0) {
				size_t triIter = 0;
				fiIter = 0;
				for (size_t fi = 0; fi < faceCnt; ++fi) {
					for (int vi = 1; vi < (*faceCnts_0)[fi] - 1; ++vi) {
						outMesh.mFaces[triIter].pn_idx[0] = (*faceIdx_0)[fiIter];
						outMesh.mFaces[triIter].pn_idx[2] = (*faceIdx_0)[fiIter + vi];
						outMesh.mFaces[triIter].pn_idx[1] = (*faceIdx_0)[fiIter + vi + 1];
						++triIter;
					}
					fiIter += (*faceCnts_0)[fi];
				}
			}
		}
	} // For PN data

	/*//
	// Now for UV data
	//
	AbcG::IV2fGeomParam uvParam = meshSchema.getUVsParam();
	if (uvParam.getNumSamples() > 0) {
		Alembic::AbcGeom::IV2fGeomParam::Sample uvSamp;
		uvParam.getIndexed(uvSamp);  // TODO: need to consider the time
		Alembic::AbcGeom::V2fArraySamplePtr uvVal = uvSamp.getVals();
		Alembic::Abc::UInt32ArraySamplePtr uvIdxVal = uvSamp.getIndices();

		size_t uvCnt = uvIdxVal->size();
		if (uvCnt == faceIdx->size()) {
			// per-vertex uv

		}
		else if (uvCnt != vertCnt ) {
			// per-polygon per-vertex uv

		}
		else {
			std::cout << " UVs aren't per-vertex or per-polygon per-vertex, skipping mesh: " << meshSchema.getName() << std::endl;
			return false;
		}
	}*/

	return true;
}

void AbcLoader::GetXformWorldTransform(const AbcG::IXform& xform, KMatrix4 trans[2])
{
	bool isAniminated = false;
	for (Abc::IObject node = xform; node.valid(); node = node.getParent()) {
		if (AbcG::IXform::matches(node.getHeader())) {
			AbcG::IXform xform(node, Abc::kWrapExisting);
			if (!xform.getSchema().isConstant()) {
				isAniminated = true;
				break;
			}
		}
	}

	if (isAniminated) {

		trans[0] = nvmath::cIdentity44f;
		trans[1] = nvmath::cIdentity44f;

		for (Abc::IObject node = xform; node.valid(); node = node.getParent()) {
			if (AbcG::IXform::matches(node.getHeader())) {
				AbcG::IXform xform(node, Abc::kWrapExisting);

				KMatrix4 localTransform;

				for (int i = 0; i < 2; ++i) {
					
					Abc::chrono_t sampleTime = mCurTime + mSampleDuration * (double)i;
					// Do two samples, one with floor index and the other with ceiling index, then
					// lerp between these two samples with the current time.
					Abc::ISampleSelector ss0(sampleTime, Abc::ISampleSelector::kFloorIndex);
					Abc::ISampleSelector ss1(sampleTime, Abc::ISampleSelector::kCeilIndex);
					Abc::TimeSamplingPtr iTsmp = xform.getSchema().getTimeSampling();
					size_t numSamps = xform.getSchema().getNumSamples();
					Abc::chrono_t t0 = iTsmp->getFloorIndex(sampleTime, numSamps).second;
					Abc::chrono_t t1 = iTsmp->getCeilIndex(sampleTime, numSamps).second;
				
					Imath::M44d mat0, mat1;
					mat0 = xform.getSchema().getValue(ss0).getMatrix();
					mat1 = xform.getSchema().getValue(ss1).getMatrix();

					if (sampleTime >= t1) {
						ConvertMatrix(mat1, localTransform);
					}
					else if (sampleTime <= t0) {
						ConvertMatrix(mat0, localTransform);
					}
					else {
						KMatrix4 kt0, kt1;
						ConvertMatrix(mat0, kt0);
						ConvertMatrix(mat1, kt1);
						nvmath::lerp( float((sampleTime-t0)/(t1-t0)), kt0, kt1, localTransform);
					}

					trans[i] = localTransform * trans[i];
				}
			}
		}
	}
	else {
		// For static xform...
		trans[0] = nvmath::cIdentity44f;
		for (Abc::IObject node = xform; node.valid(); node = node.getParent()) {
			if (AbcG::IXform::matches(node.getHeader())) {
				AbcG::IXform xform(node, Abc::kWrapExisting);
				Imath::M44d mat = xform.getSchema().getValue().getMatrix();
				KMatrix4 localTransform;
				ConvertMatrix(mat, localTransform);
				trans[0] = localTransform * trans[0];
			}

		}

		trans[1] = trans[0];
	}

	
}
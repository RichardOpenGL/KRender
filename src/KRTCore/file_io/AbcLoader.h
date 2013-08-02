#pragma once

#include "../base/geometry.h"
#include "../scene/KKDTreeScene.h"
#include <string>
#include <list>
#include <hash_map>
#include <Alembic/AbcGeom/All.h>
#include <Alembic/Abc/Foundation.h>
namespace Abc = Alembic::Abc::ALEMBIC_VERSION_NS;
namespace AbcG = Alembic::AbcGeom::ALEMBIC_VERSION_NS;
namespace AbcA = ::Alembic::AbcCoreAbstract::ALEMBIC_VERSION_NS;
class KSceneSet;



class AbcLoader
{
public:
	AbcLoader();
	~AbcLoader();

	bool Load(const char* filename, KSceneSet& scene);

private:
	void ProcessNode(const Abc::IObject& obj, int treeDepth = 0);

	// Functions to convert different types of Abc objects.
	// Each of these functions will return true if it needs to process its children, otherwise false is returned.
	void ProcessMesh(const AbcG::IPolyMesh& mesh);
	void ProcessCamera(const AbcG::ICamera& camera);

	void GetXformWorldTransform(const AbcG::IXform& xform, KMatrix4 trans[2]);
	KScene* GetXformStaticScene(const Abc::IObject& obj, KMatrix4& mat);

	bool ConvertMesh(const AbcG::IPolyMeshSchema& meshSchema, Abc::chrono_t t, KTriMesh& outMesh);

	static void ConvertMatrix(const Imath::M44d& ilmMat, KMatrix4& mat);

	template <typename ArrayType>
	static bool CompareAbcArrary(const ArrayType& left, const ArrayType& right) {
		if (left->size() != right->size())
			return false;
		for (size_t i = 0; i < left->size(); ++i) {
			if ((*left)[i] != (*right)[i]) return false;
		}
		return true;
	}

private:
	double mCurTime;
	double mSampleDuration;

	KSceneSet* mpScene;
	std::hash_map<AbcA::ObjectReader*, KScene*> mXformNodes;

};
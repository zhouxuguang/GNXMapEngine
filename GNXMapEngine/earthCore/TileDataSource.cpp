#include "TileDataSource.h"

EARTH_CORE_NAMESPACE_BEGIN

TileDataSource::TileDataSource(const std::string& dataPath, const std::string& extName)
{
    mDataPath = dataPath;
    mExtName = extName;
}

TileDataSource::~TileDataSource()
{
}

bool TileDataSource::ReadTile(TaskRunnerPtr task)
{
	/*TileLoadTaskPtr pTask = std::dynamic_pointer_cast<TileLoadTask>(task);
	if (pTask == nullptr)
	{
		return false;
	}

	if (pTask->layer->readTile(pTask))
	{
	}*/

	return false;
}

EARTH_CORE_NAMESPACE_END
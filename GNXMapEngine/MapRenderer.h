//
//  MapRenderer.h
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/9.
//

#ifndef MapRenderer_hpp
#define MapRenderer_hpp

#include "Runtime/RenderCore/include/RenderDevice.h"
#include "earthCore/EarthEngineDefine.h"
#include "earthCore/EarthCamera.h"
#include "Runtime/BaseLib/include/LruCache.h"
#include "Runtime/BaseLib/include/ThreadPool.h"
#include "WebMercator.h"

#include <string>
#include <vector>

using namespace RenderCore;

class TileData
{
public:
    Vector2i key;       //xy方向编号
    Vector2d     start;  //起始点
    Vector2d     end;    //结束点
    RCBufferPtr vertexBuffer;
};

typedef std::shared_ptr<TileData> TileDataPtr;

struct TileKey
{
    int x;
    int y;
    int level;
    
    bool operator == (const TileKey& other) const
    {
        return x == other.x && y == other.y && level == other.level;
    }
};

namespace std 
{
    template <> struct hash<TileKey>
    {
        size_t operator()(const TileKey& p) const
        {
            auto hash1 = std::hash<int>{}(p.x);
            auto hash2 = std::hash<int>{}(p.y);
            auto hash3 = std::hash<int>{}(p.level);
            return hash1 ^ (hash2 << 1) ^ (hash3 << 2);
        }
    };
}

typedef std::vector<TileDataPtr> TileDataArray;

class MapRenderer;

class TileLoadTask : public baselib::TaskRunner
{
public:
    TileLoadTask()
    {
    }
    
    ~TileLoadTask()
    {
    }
    
    virtual void Run();
    
    MapRenderer* mRender;
    TileKey tileKey;
};

class MapRenderer
{
public:
    MapRenderer();
    
    ~MapRenderer()
    {
    }
    
    void DrawFrame();
    
    void SetWindowSize(uint32_t width, uint32_t height);
    
    void Zoom(double deltaDistance);
    
    void Pan(float offsetX, float offsetY);

    // 鼠标拖拽改变姿态（目标点不变）：右键 = 横向方位角 + 纵向等比缩放，中键 = 纵向俯仰角。
    // dxPixels/dyPixels 为屏幕「物理像素」增量（逻辑坐标需先乘窗口 DPIScale）。
    void OrbitByDrag(double dxPixels, double dyPixels, earthcore::CameraDragMode mode);

    // 拖拽灵敏度（1.0 为默认手感，负值反向，非有限值按 0）
    void SetDragSensitivity(double azimuthSensitivity, double pitchSensitivity, double zoomSensitivity);

    // ==================== 视角控制：方位角 / 俯仰角 ====================

    // 同时设定方位角与俯仰角（单位：度）。
    // 目标点与视线距离保持不变，相机绕目标点旋转。
    // 设定值采用「目标点处量测」口径，与 GetAzimuthAngleAtTargetDegrees /
    // GetPitchAngleAtTargetDegrees 精确一致；Get*Degrees() 是按用户定义在
    // 视点处量测的值，两者在椭球曲率下存在系统性差异（见 EarthCamera.h）。
    void SetAzimuthPitchDegrees(double azimuthDegrees, double pitchDegrees);

    // 视线距离（视点 <-> 目标点），单位米
    void SetEyeDistance(double distance);
    double GetEyeDistance() const;

    // 方位角 / 俯仰角（度）：视点处量测（按用户定义）
    double GetAzimuthAngleDegrees() const;
    double GetPitchAngleDegrees() const;

    // 方位角 / 俯仰角（度）：目标点处量测（与设定值一致）
    double GetAzimuthAngleAtTargetDegrees() const;
    double GetPitchAngleAtTargetDegrees() const;

    // 目标点大地坐标（经度/纬度单位为度，高度为米）
    void SetTargetGeodeticDegrees(double longitudeDegrees, double latitudeDegrees, double heightMeters);

    // 视点 / 目标点的大地坐标（经度、纬度：度；高度：米）
    void GetEyeGeodeticDegrees(double& longitudeDegrees, double& latitudeDegrees, double& heightMeters) const;
    void GetTargetGeodeticDegrees(double& longitudeDegrees, double& latitudeDegrees, double& heightMeters) const;

    // 视线方向在视点处东北天坐标系下的分量（x=东, y=北, z=天）
    Vector3d GetViewDirectionInEyeEun() const;

    // 相机世界坐标位置
    Vector3f GetCameraPosition() const;

    // 记录当前视角参数（用于日志与验证）
    void LogCameraState(const char* tag) const;

    // ==================== 截图 ====================

    // 把当前场景离屏渲染一帧（含 UI 面板）并保存为 PNG。
    // 需要在 ImGui 帧已经构建并调用过 ImGui::Render() 之后调用。
    bool SaveScreenshot(const std::string& filePath);

    const earthcore::EarthCameraPtr& GetEarthCamera() const { return mCameraPtr; }

private:
    RenderCore::RenderDevicePtr mRenderdevice = nullptr;
    SceneManager* mSceneManager;
    
    double mWidth;
    double mHeight;
    
    uint64_t mLastTime = 0;
    
    earthcore::EarthCameraPtr mCameraPtr = nullptr;
    
    void BuildEarthNode();
};

#endif /* MapRenderer_hpp */

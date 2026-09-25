//
//  EarthCamera.h
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/7/2.
//
//  三维地球的相机类。
//
//  相机状态由四要素描述：
//     目标点 mTargetPos（椭球固连坐标系）+ 方位角 + 俯仰角 + 视线距离
//  视点位置由这四要素解析求出，因此：
//     * 只改方位角 / 俯仰角时，目标点不变、相机与目标点的距离不变，相机绕目标点旋转；
//     * 只改距离时为沿当前视线方向的推拉，目标点与角度都不变。
//
//  方位角 / 俯仰角的定义见 EarthCameraPose.h。注意「视点处量测」与「目标点处量测」
//  在椭球曲率下存在系统性差异，因此两套回读都提供：
//     * Set* 设定的值是「目标点处量测」的约定值，与 Get*AtTarget() 精确一致；
//     * Get*() 是按用户定义在「视点处量测」的值，与设定值存在约 1° 量级的差异（高空尤其明显）。
//

#ifndef GNX_EARTHENGINE_CORE_EARTHCAMERA_INCLUDE_JJHSGDFGFB
#define GNX_EARTHENGINE_CORE_EARTHCAMERA_INCLUDE_JJHSGDFGFB

#include "EarthEngineDefine.h"
#include "Ellipsoid.h"
#include "EarthCameraPose.h"

EARTH_CORE_NAMESPACE_BEGIN

// 三维地球的相机类
class EarthCamera : public Camera
{
public:
    EarthCamera(const Ellipsoid& ellipsoid, const std::string& name);

    ~EarthCamera();

    // ==================== 方位角（弧度，[0, 2π)，正北为 0、顺时针为正） ====================

    // 设定方位角（保持目标点/俯仰角/距离不变，视点绕目标点水平旋转）
    void SetAzimuthAngle(double azimuthRad);

    // 按用户定义在「视点处」量测的方位角。
    // 垂直下视（俯仰角为 0）时视线水平分量退化，方位角在数学上无定义，此处返回 0。
    double GetAzimuthAngle() const;

    // 目标点处的轨道方位角，即 SetAzimuthAngle 保存的命令状态。
    // 垂直下视时几何方位角不可观测，但该状态仍保留，用于画面朝向和后续倾斜。
    double GetAzimuthAngleAtTarget() const;

    double GetAzimuthAngleDegrees() const;
    double GetAzimuthAngleAtTargetDegrees() const;

    // ==================== 俯仰角（弧度，[0, π/2]，垂直下视为 0） ====================

    // 设定俯仰角（保持目标点/方位角/距离不变，视点沿竖直平面绕目标点旋转）
    void SetPitchAngle(double pitchRad);

    // 按用户定义在「视点处」量测的俯仰角（用视点处的大地法线）
    double GetPitchAngle() const;

    // 目标点处的轨道俯仰角，即 SetPitchAngle 保存的命令状态。
    double GetPitchAngleAtTarget() const;

    double GetPitchAngleDegrees() const;
    double GetPitchAngleAtTargetDegrees() const;

    // 同时设定方位角与俯仰角（只触发一次姿态重算）
    void SetAzimuthPitch(double azimuthRad, double pitchRad);
    void SetAzimuthPitchDegrees(double azimuthDegrees, double pitchDegrees);

    // ==================== 视线距离 ====================

    // 设定视点与目标点的距离（保持目标点与角度不变），内部按最小距离钳制
    void SetEyeDistance(double distance);

    double GetEyeDistance() const { return mEyeDistance; }

    // ==================== 视点 / 目标点 ====================

    // 直接设定视点大地坐标：目标点不变，俯仰角/方位角/距离按新视点反算
    void SetEyeGeodetic(const Geodetic3D& eyeGeodetic);

    // 直接设定注视点大地坐标：距离与角度保持不变，视点随之平移
    void SetEyeGeodeticTarget(const Geodetic3D& targetGeodetic);

    const Geodetic3D& GetEyeGeodetic() const { return mEyeGeodetic; }

    const Geodetic3D& GetEyeGeodeticTarget() const { return mEyeGeodeticTarget; }

    // 视点 / 目标点的空间直角坐标（椭球固连坐标系）
    const Vector3d& GetEyeCartesian() const { return mEyePos; }

    const Vector3d& GetTargetCartesian() const { return mTargetPos; }

    // 视线方向（视点 -> 目标点，单位向量）
    virtual Vector3f GetViewDirection() const override;

    // 视线方向在「视点处东北天坐标系」下的分量（x=东, y=北, z=天）。
    // 这是方位角/俯仰角定义的最直接读数：方位角 = atan2(东, 北)，
    // 俯仰角 = atan2(sqrt(东² + 北²), -天分量)。
    Vector3d GetViewDirectionInEyeEun() const;

    // ==================== 交互 ====================

    // 缩放地球：沿视线方向推拉，保持目标点与角度不变
    void Zoom(double deltaDistance);

    // 平移地球：offsetX, offsetY 是屏幕坐标增量
    void Pan(float offsetX, float offsetY);

    // 鼠标拖拽改变姿态（目标点不变，相机绕目标点旋转/推拉），映射规则见 EarthCameraPose::ApplyDragToPose。
    // dxPixels/dyPixels 必须是屏幕「物理像素」增量（逻辑坐标需先乘窗口 DPIScale），
    // 手感由相机 FOV 与视口高度决定。只触发一次 ApplyPose。
    void OrbitByDrag(double dxPixels, double dyPixels, CameraDragMode mode);

    // 拖拽灵敏度（1.0 为默认手感，负值反向，非有限值按 0）
    void SetDragSensitivity(double azimuthSensitivity, double pitchSensitivity, double zoomSensitivity);

    double GetAzimuthDragSensitivity() const { return mAzimuthDragSensitivity; }
    double GetPitchDragSensitivity() const { return mPitchDragSensitivity; }
    double GetZoomDragSensitivity() const { return mZoomDragSensitivity; }

private:
    // 唯一的姿态重算入口：把角度/距离钳制到合法区间，由「目标点 + 角度 + 距离」求视点，
    // 刷新视点大地坐标与视图矩阵。所有 setter 最终都汇聚到这里。
    void ApplyPose();

    Ellipsoid mEllipsoid;            // 椭球体
    Geodetic3D mEyeGeodetic;         // 视点的大地坐标
    Geodetic3D mEyeGeodeticTarget;   // 注视点的大地坐标

    Vector3d mEyePos;                // 视点的空间直角坐标
    Vector3d mTargetPos;             // 注视点的空间直角坐标

    double mEyeDistance = 0.0;       // 视点到注视点的距离

    // 角度状态。约定为「目标点处量测」的值（见文件头说明），保证 set/get 自洽。
    double mAzimuthAngle = 0.0;      // 方位角（弧度）
    double mPitchAngle = 0.0;        // 俯仰角（弧度）

    // 拖拽灵敏度（1.0 为基准手感）
    double mAzimuthDragSensitivity = 1.0;
    double mPitchDragSensitivity = 1.0;
    double mZoomDragSensitivity = 1.0;
};

using EarthCameraPtr = std::shared_ptr<EarthCamera>;

EARTH_CORE_NAMESPACE_END

#endif /* GNX_EARTHENGINE_CORE_EARTHCAMERA_INCLUDE_JJHSGDFGFB */

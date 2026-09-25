//
//  TestEarthCameraAngles.cpp
//  GNXMapEngine
//
//  角度姿态数学的「无窗口」数值验证程序。
//
//  只编译地球数学相关的少数源文件（EarthCameraPose / Ellipsoid / Geodetic3D /
//  GeoTransform），不引入 EarthCamera.cpp——后者的构造依赖渲染设备，必须开窗口
//  才能跑。因此本程序可以在没有任何 GPU / 窗口的环境下断言数学正确性。
//
//  验证内容：
//    1. 视线方向公式与方位角/俯仰角定义逐条吻合（北/东/南/西、垂直下视）
//    2. 轨道不变量：改角度时目标点不变、|视点-目标点| 不变、up 与视线正交（无滚转）
//    3. 方位角的方向语义（视点相对目标点在东南西北哪一侧）
//    4. 用户例子（北纬22/东经113、200km、方位角0、俯仰角 0→45）的新旧视点对照
//    5. 方位角规约与俯仰角钳制
//

#include "earthCore/EarthCameraPose.h"
#include "earthCore/Ellipsoid.h"
#include "earthCore/Geodetic3D.h"

#include <cstdio>
#include <cmath>
#include <algorithm>
#include <limits>
#include <vector>

using namespace earthcore;

namespace
{
    int gCheckCount = 0;
    int gFailureCount = 0;

    void CheckTrue(const char* what, bool condition)
    {
        ++gCheckCount;
        if (condition)
        {
            printf("  [PASS] %s\n", what);
        }
        else
        {
            ++gFailureCount;
            printf("  [FAIL] %s\n", what);
        }
    }

    void CheckNear(const char* what, double actual, double expected, double tolerance, const char* unit)
    {
        ++gCheckCount;
        const double diff = fabs(actual - expected);
        if (diff <= tolerance)
        {
            printf("  [PASS] %-46s actual=%.9f expected=%.9f diff=%.3e %s\n",
                   what, actual, expected, diff, unit);
        }
        else
        {
            ++gFailureCount;
            printf("  [FAIL] %-46s actual=%.9f expected=%.9f diff=%.3e > tol=%.3e %s\n",
                   what, actual, expected, diff, tolerance, unit);
        }
    }

    double ToDeg(double rad) { return EarthCameraPose::ToDegrees(rad); }
    double ToRad(double deg) { return EarthCameraPose::ToRadians(deg); }

    void PrintGeodetic(const char* label, const Geodetic3D& geodetic)
    {
        printf("  %s: 经度=%.9f 度, 纬度=%.9f 度, 高=%.3f m\n",
               label, ToDeg(geodetic.longitude), ToDeg(geodetic.latitude), geodetic.height);
    }

    // ------------------------------------------------------------------
    // 1. 视线方向公式与定义的对齐
    // ------------------------------------------------------------------
    void TestViewDirectionDefinition()
    {
        printf("\n[1] 视线方向公式 vs 方位角/俯仰角定义\n");

        const Vector3d nadir = EarthCameraPose::ViewDirectionFromAngles(0.0, 0.0);
        CheckNear("俯仰角0 视线 z 分量（应垂直向下）", nadir.z, -1.0, 1e-15, "");
        CheckNear("俯仰角0 视线 北分量（应退化）", nadir.y, 0.0, 1e-15, "");
        CheckNear("俯仰角0 视线 东分量（应退化）", nadir.x, 0.0, 1e-15, "");

        const Vector3d north45 = EarthCameraPose::ViewDirectionFromAngles(0.0, ToRad(45.0));
        CheckNear("方位角0/俯仰角45 北分量", north45.y, sin(ToRad(45.0)), 1e-15, "");
        CheckNear("方位角0/俯仰角45 东分量（应为0）", north45.x, 0.0, 1e-15, "");
        CheckNear("方位角0/俯仰角45 天分量", north45.z, -cos(ToRad(45.0)), 1e-15, "");

        const Vector3d east30 = EarthCameraPose::ViewDirectionFromAngles(ToRad(90.0), ToRad(30.0));
        CheckNear("方位角90/俯仰角30 东分量（应为正）", east30.x, sin(ToRad(30.0)), 1e-15, "");

        const Vector3d south30 = EarthCameraPose::ViewDirectionFromAngles(ToRad(180.0), ToRad(30.0));
        CheckNear("方位角180/俯仰角30 北分量（应为负）", south30.y, -sin(ToRad(30.0)), 1e-15, "");

        const Vector3d west30 = EarthCameraPose::ViewDirectionFromAngles(ToRad(270.0), ToRad(30.0));
        CheckNear("方位角270/俯仰角30 东分量（应为负）", west30.x, -sin(ToRad(30.0)), 1e-15, "");

        const Vector3d horizon90 = EarthCameraPose::ViewDirectionFromAngles(0.0, ToRad(90.0));
        CheckNear("俯仰角90 天分量（应为水平）", horizon90.z, 0.0, 1e-15, "");
    }

    // ------------------------------------------------------------------
    // 2. 轨道不变量：目标点固定 + 距离恒定 + 角度回读一致 + 无滚转
    // ------------------------------------------------------------------
    void TestOrbitInvariants()
    {
        printf("\n[2] 轨道不变量（多组经纬度/距离/方位角/俯仰角）\n");

        const Ellipsoid& ellipsoid = Ellipsoid::WGS84;
        const double azimuthDegrees[] = { 0.0, 45.0, 90.0, 135.0, 180.0, 225.0, 270.0, 315.0 };
        const double pitchDegrees[] = { 0.0, 5.0, 30.0, 45.0, 60.0, 80.0, 90.0 };
        const double distances[] = { 5000.0, 200000.0, 2000000.0 };
        const double targetLonLat[][2] = { {113.0, 22.0}, {-73.98, 40.71}, {151.2, -33.87}, {0.0, 0.0}, {10.0, 78.0} };

        double maxDistanceRelativeError = 0.0;
        double maxAzimuthErrorDeg = 0.0;
        double maxPitchErrorDeg = 0.0;
        double maxUpTiltDegrees = 0.0;      // up 与视线的夹角偏离 90 度的量
        double maxRollDegrees = 0.0;        // 相机右向量偏离当地水平面的量（滚转）
        size_t combinationCount = 0;

        for (const auto& lonLat : targetLonLat)
        {
            const Vector3d targetPos = ellipsoid.CartographicToCartesian(
                Geodetic3D::FromDegrees(lonLat[0], lonLat[1], 0.0));

            for (double distance : distances)
            {
                for (double azimuthDeg : azimuthDegrees)
                {
                    for (double pitchDeg : pitchDegrees)
                    {
                        const Vector3d eyePos = EarthCameraPose::EyeFromTargetAndAngles(
                            targetPos, ellipsoid, ToRad(azimuthDeg), ToRad(pitchDeg), distance);

                        // 不变量 1：目标点不变、视线距离恒定
                        const double measuredDistance = (eyePos - targetPos).Length();
                        maxDistanceRelativeError = std::max(maxDistanceRelativeError,
                            fabs(measuredDistance - distance) / distance);

                        // 不变量 2：角度回读（目标点处量测）与设定值一致
                        double readAzimuth = 0.0;
                        double readPitch = 0.0;
                        EarthCameraPose::AnglesFromEyeTarget(eyePos, targetPos, ellipsoid, false,
                                                             readAzimuth, readPitch);

                        maxPitchErrorDeg = std::max(maxPitchErrorDeg,
                            fabs(ToDeg(readPitch) - pitchDeg));

                        if (pitchDeg > 1.0)
                        {
                            double azimuthDiff = fabs(ToDeg(readAzimuth) - azimuthDeg);
                            azimuthDiff = std::min(azimuthDiff, 360.0 - azimuthDiff);
                            maxAzimuthErrorDeg = std::max(maxAzimuthErrorDeg, azimuthDiff);
                        }

                        // 不变量 3：LookAt 的 up 必须与视线正交
                        // （滚动角为 0 的等价条件；若 up 与视线不垂直就会看到地平线倾斜）
                        const Vector3d up = EarthCameraPose::LookUpForLookAt(
                            eyePos, targetPos, ellipsoid, ToRad(azimuthDeg));
                        Vector3d forward = targetPos - eyePos;
                        forward.Normalize();
                        const double perpendicularDeviationDeg =
                            ToDeg(asin(Clamp(fabs(up.DotProduct(forward)), 0.0, 1.0)));
                        maxUpTiltDegrees = std::max(maxUpTiltDegrees, perpendicularDeviationDeg);

                        // 不变量 4：相机右向量必须落在当地水平面内（地平线水平、无滚转）
                        Vector3d right = Vector3d::CrossProduct(forward, up);
                        right.Normalize();
                        const Vector3d eyeUp = EarthCameraPose::BuildEunFrame(eyePos, ellipsoid).up;
                        const double rollDeg =
                            ToDeg(asin(Clamp(fabs(right.DotProduct(eyeUp)), 0.0, 1.0)));
                        maxRollDegrees = std::max(maxRollDegrees, rollDeg);

                        ++combinationCount;
                    }
                }
            }
        }

        printf("  组合数 = %zu 组\n", combinationCount);
        CheckTrue("视线距离相对误差 < 1e-12", maxDistanceRelativeError < 1e-12);
        CheckTrue("角度回读（目标点处）俯仰角最大误差 < 1e-9 度", maxPitchErrorDeg < 1e-9);
        CheckTrue("角度回读（目标点处）方位角最大误差 < 1e-9 度", maxAzimuthErrorDeg < 1e-9);
        CheckTrue("up 与视线正交偏差 < 2 度（无滚转、无退化）", maxUpTiltDegrees < 2.0);
        CheckTrue("相机右向量偏离当地水平面 < 1e-6 度（地平线严格水平）", maxRollDegrees < 1e-6);

        printf("  视线距离最大相对误差 = %.3e\n", maxDistanceRelativeError);
        printf("  俯仰角回读最大误差 = %.3e 度\n", maxPitchErrorDeg);
        printf("  方位角回读最大误差 = %.3e 度\n", maxAzimuthErrorDeg);
        printf("  up 与视线正交最大偏差 = %.6f 度\n", maxUpTiltDegrees);
        printf("  相机滚转最大偏差 = %.3e 度\n", maxRollDegrees);
    }

    // ------------------------------------------------------------------
    // 3. 方位角的方向语义（北/东/南/西）
    // ------------------------------------------------------------------
    void TestAzimuthDirection()
    {
        printf("\n[3] 方位角方向语义（视点相对目标点的方位）\n");

        const Ellipsoid& ellipsoid = Ellipsoid::WGS84;
        const Vector3d targetPos = ellipsoid.CartographicToCartesian(
            Geodetic3D::FromDegrees(113.0, 22.0, 0.0));
        const double distance = 200000.0;
        const double pitch = ToRad(45.0);
        const double sinPitch = sin(pitch);

        const EunFrame frame = EarthCameraPose::BuildEunFrame(targetPos, ellipsoid);

        const struct { double azimuthDeg; const char* name; double expectEast; double expectNorth; } cases[] = {
            {   0.0, "方位角0  视点在目标点南侧（视线朝北）", 0.0, -1.0 },
            {  90.0, "方位角90 视点在目标点西侧（视线朝东）", -1.0, 0.0 },
            { 180.0, "方位角180 视点在目标点北侧（视线朝南）", 0.0, 1.0 },
            { 270.0, "方位角270 视点在目标点东侧（视线朝西）", 1.0, 0.0 },
        };

        for (const auto& item : cases)
        {
            const Vector3d eyePos = EarthCameraPose::EyeFromTargetAndAngles(
                targetPos, ellipsoid, ToRad(item.azimuthDeg), pitch, distance);

            const Vector3d targetToEye = eyePos - targetPos;
            const double east = targetToEye.DotProduct(frame.east);
            const double north = targetToEye.DotProduct(frame.north);

            const double horizontalLength = sqrt(east * east + north * north);
            const double unitEast = horizontalLength > 0.0 ? east / horizontalLength : 0.0;
            const double unitNorth = horizontalLength > 0.0 ? north / horizontalLength : 0.0;

            char buffer[256];
            snprintf(buffer, sizeof(buffer), "%s [东分量]", item.name);
            CheckNear(buffer, unitEast, item.expectEast, 1e-9, "");
            snprintf(buffer, sizeof(buffer), "%s [北分量]", item.name);
            CheckNear(buffer, unitNorth, item.expectNorth, 1e-9, "");

            // 视线方向的水平分量应与「视点相对目标点的方位」相反（相机看着目标点）
            const Vector3d viewDir = EarthCameraPose::ViewDirectionFromAngles(ToRad(item.azimuthDeg), pitch);
            const double viewHorizontalEast = viewDir.x / sinPitch;
            const double viewHorizontalNorth = viewDir.y / sinPitch;
            snprintf(buffer, sizeof(buffer), "%s [视线与视点方位相反]", item.name);
            CheckNear(buffer, -(unitEast * viewHorizontalEast + unitNorth * viewHorizontalNorth), 1.0, 1e-9, "");
        }
    }

    // ------------------------------------------------------------------
    // 4. 用户例子：北纬22/东经113、200km 上空、方位角 0、俯仰角 0 -> 45
    // ------------------------------------------------------------------
    void TestUserExample()
    {
        printf("\n[4] 用户例子：北纬22度/东经113度/200km、方位角0、俯仰角 0 -> 45\n");

        const Ellipsoid& ellipsoid = Ellipsoid::WGS84;

        const Geodetic3D initialEyeGeodetic = Geodetic3D::FromDegrees(113.0, 22.0, 200000.0);
        const Geodetic3D targetGeodetic = Geodetic3D::FromDegrees(113.0, 22.0, 0.0);

        const Vector3d initialEyePos = ellipsoid.CartographicToCartesian(initialEyeGeodetic);
        const Vector3d targetPos = ellipsoid.CartographicToCartesian(targetGeodetic);
        const double distance = (initialEyePos - targetPos).Length();

        PrintGeodetic("初始视点", initialEyeGeodetic);
        PrintGeodetic("目标点  ", targetGeodetic);
        printf("  初始视线距离 = %.6f m\n", distance);
        CheckNear("初始距离应为 200km", distance, 200000.0, 1e-6, "m");

        // 经纬度 -> 直角坐标 -> 经纬度 的往返应当无漂移
        PrintGeodetic("初始视点往返", ellipsoid.CartesianToCartographic(initialEyePos));
        CheckNear("往返经度误差（应无漂移）",
                  ToDeg(ellipsoid.CartesianToCartographic(initialEyePos).longitude) - 113.0, 0.0, 1e-9, "度");

        // 初始状态（俯仰角 0：视点在目标点正上方，视线垂直下视）
        double initialAzimuth = 0.0;
        double initialPitch = 0.0;
        EarthCameraPose::AnglesFromEyeTarget(initialEyePos, targetPos, ellipsoid, false,
                                             initialAzimuth, initialPitch);
        CheckNear("初始俯仰角（目标点处）", ToDeg(initialPitch), 0.0, 1e-9, "度");

        // 视点恰好位于目标点法线上时，两套基准量测的俯仰角都必须为 0。
        // 这一条同时守住了「视点处大地法线必须精确」（不能用离面点近似法线，
        // 否则此处会出现约 0.004 度的假偏差）。
        double initialEyeBasisAzimuth = 0.0;
        double initialEyeBasisPitch = 0.0;
        EarthCameraPose::AnglesFromEyeTarget(initialEyePos, targetPos, ellipsoid, true,
                                             initialEyeBasisAzimuth, initialEyeBasisPitch);
        CheckNear("初始俯仰角（视点处，应精确为 0）", ToDeg(initialEyeBasisPitch), 0.0, 1e-9, "度");

        // 俯仰角 0 -> 45（目标点与距离不变）
        const double azimuth = 0.0;
        const Vector3d newEyePos = EarthCameraPose::EyeFromTargetAndAngles(
            targetPos, ellipsoid, azimuth, ToRad(45.0), distance);
        const Geodetic3D newEyeGeodetic = ellipsoid.CartesianToCartographic(newEyePos);

        PrintGeodetic("新视点  ", newEyeGeodetic);
        printf("  新的视线距离 = %.6f m\n", (newEyePos - targetPos).Length());

        CheckNear("改俯仰角后距离不变", (newEyePos - targetPos).Length(), distance, 1e-6, "m");
        CheckTrue("改俯仰角后相机纬度变小（向目标点南侧移动）",
                  ToDeg(newEyeGeodetic.latitude) < ToDeg(initialEyeGeodetic.latitude));
        CheckTrue("改俯仰角后相机高度降低", newEyeGeodetic.height < initialEyeGeodetic.height);

        // 相机应仍留在「目标点所在的子午面」内：相对目标点没有东西向偏移
        const EunFrame targetFrame = EarthCameraPose::BuildEunFrame(targetPos, ellipsoid);
        CheckNear("改俯仰角后相机无东西向偏移（仍在同一子午面）",
                  (newEyePos - targetPos).DotProduct(targetFrame.east), 0.0, 1e-6, "m");
        CheckNear("改俯仰角后相机经度不变", ToDeg(newEyeGeodetic.longitude), 113.0, 1e-9, "度");

        double newAzimuth = 0.0;
        double newPitch = 0.0;
        EarthCameraPose::AnglesFromEyeTarget(newEyePos, targetPos, ellipsoid, false, newAzimuth, newPitch);
        CheckNear("新俯仰角回读（目标点处）", ToDeg(newPitch), 45.0, 1e-9, "度");

        // 两套基准的差异：视点处量测的俯仰角与设定值存在系统性偏差
        double eyeBasisAzimuth = 0.0;
        double eyeBasisPitch = 0.0;
        EarthCameraPose::AnglesFromEyeTarget(newEyePos, targetPos, ellipsoid, true, eyeBasisAzimuth, eyeBasisPitch);
        printf("  [信息] 视点处量测俯仰角 = %.6f 度，与设定值 45 度的偏差 = %.6f 度\n",
               ToDeg(eyeBasisPitch), ToDeg(eyeBasisPitch) - 45.0);
        CheckTrue("视点处量测与目标点处量测存在差异（椭球曲率的几何必然）",
                  fabs(ToDeg(eyeBasisPitch) - 45.0) > 0.1);
        CheckTrue("视点处量测偏差量级在 5 度以内", fabs(ToDeg(eyeBasisPitch) - 45.0) < 5.0);

        // 视线仍朝北下俯
        const EunFrame newEyeFrame = EarthCameraPose::BuildEunFrame(newEyePos, ellipsoid);
        const Vector3d viewDir = targetPos - newEyePos;
        CheckTrue("新视线方向含向北分量（仍朝北下俯）", viewDir.DotProduct(newEyeFrame.north) > 0.0);
        CheckTrue("新视线方向含向下分量（俯视地表）", viewDir.DotProduct(newEyeFrame.up) < 0.0);

        printf("\n  [说明] 需求例子中\"北纬11度、100km\"与\"距离保持不变(200km)\"在几何上不自洽：\n");
        printf("         11 度纬差约 1222km，远超 200km 的视线距离。上面按\"目标点不变 + 距离不变\"\n");
        printf("         求得的实际新视点如上所示（约南移 %.1f km、高度约 %.1f km），方向与例子一致。\n",
               (ToDeg(initialEyeGeodetic.latitude) - ToDeg(newEyeGeodetic.latitude)) * 111.32,
               newEyeGeodetic.height / 1000.0);
    }

    // ------------------------------------------------------------------
    // 5. 方位角规约与俯仰角钳制
    // ------------------------------------------------------------------
    void TestNormalizeAndClamp()
    {
        printf("\n[5] 方位角规约 [0,360) 与俯仰角钳制 [0,90]\n");

        CheckNear("NormalizeAzimuth(-90 度)", ToDeg(EarthCameraPose::NormalizeAzimuth(ToRad(-90.0))), 270.0, 1e-12, "度");
        CheckNear("NormalizeAzimuth(450 度)", ToDeg(EarthCameraPose::NormalizeAzimuth(ToRad(450.0))), 90.0, 1e-12, "度");
        CheckNear("NormalizeAzimuth(-1 度)", ToDeg(EarthCameraPose::NormalizeAzimuth(ToRad(-1.0))), 359.0, 1e-12, "度");
        CheckNear("NormalizeAzimuth(0 度)", ToDeg(EarthCameraPose::NormalizeAzimuth(0.0)), 0.0, 1e-12, "度");

        CheckNear("ClampPitch(-10 度)", ToDeg(EarthCameraPose::ClampPitch(ToRad(-10.0))), 0.0, 1e-12, "度");
        CheckNear("ClampPitch(100 度)", ToDeg(EarthCameraPose::ClampPitch(ToRad(100.0))), 90.0, 1e-12, "度");
        CheckNear("ClampPitch(45 度)", ToDeg(EarthCameraPose::ClampPitch(ToRad(45.0))), 45.0, 1e-12, "度");

        CheckNear("NormalizeAzimuth(NaN) 安全回退", EarthCameraPose::NormalizeAzimuth(
                      std::numeric_limits<double>::quiet_NaN()), 0.0, 0.0, "rad");
        CheckNear("NormalizeAzimuth(Inf) 安全回退", EarthCameraPose::NormalizeAzimuth(
                      std::numeric_limits<double>::infinity()), 0.0, 0.0, "rad");
        CheckNear("ClampPitch(NaN) 安全回退", EarthCameraPose::ClampPitch(
                      std::numeric_limits<double>::quiet_NaN()), 0.0, 0.0, "rad");

        // 垂直下视时几何方位不可观测：纯数学反算回到 0，但 EarthCamera
        // 的 GetAzimuthAngleAtTarget() 现在返回持久化的命令状态，不再用此反算值覆盖它。
        const Ellipsoid& ellipsoid = Ellipsoid::WGS84;
        const Vector3d target = ellipsoid.CartographicToCartesian(Geodetic3D::FromDegrees(113.0, 22.0));
        const Vector3d eye = EarthCameraPose::EyeFromTargetAndAngles(
            target, ellipsoid, ToRad(90.0), 0.0, 200000.0);
        double measuredAzimuth = -1.0;
        double measuredPitch = -1.0;
        EarthCameraPose::AnglesFromEyeTarget(eye, target, ellipsoid, false,
                                             measuredAzimuth, measuredPitch);
        CheckNear("垂直下视的几何方位退化为 0", measuredAzimuth, 0.0, 1e-12, "rad");
        CheckNear("垂直下视的俯仰角为 0", measuredPitch, 0.0, 1e-12, "rad");
    }
}

int main()
{
    printf("==================== EarthCamera 方位角/俯仰角 数值验证 ====================\n");

    TestViewDirectionDefinition();
    TestOrbitInvariants();
    TestAzimuthDirection();
    TestUserExample();
    TestNormalizeAndClamp();

    printf("\n==================== 结果 ====================\n");
    printf("检查项: %d, 失败: %d\n", gCheckCount, gFailureCount);

    if (gFailureCount == 0)
    {
        printf("全部通过。\n");
        return 0;
    }

    printf("存在失败项。\n");
    return 1;
}

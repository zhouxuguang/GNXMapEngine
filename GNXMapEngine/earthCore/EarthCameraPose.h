//
//  EarthCameraPose.h
//  GNXMapEngine
//
//  三维地球相机的「方位角 / 俯仰角」姿态数学。
//
//  设计约束（重要）：
//    本模块只依赖 Ellipsoid / GeoTransform / MathUtil，不引用 RenderDevice、
//    Camera 基类等渲染侧类型，也不调用任何 GPU 接口。这样它就能被「无窗口」
//    的数值验证程序直接调用并断言，而不必先创建渲染设备。
//
//  角度定义（与用户需求逐条对应）：
//    方位角 azimuth：EUN(x=东, y=北, z=天) 下视线方向(视点->目标点)与正北方向的
//                    夹角；正北为 0，顺时针为正，取值 [0, 360)。
//    俯仰角 pitch  ：视线方向的反方向(目标点->视点)与「过该点的大地法线(z 轴)」
//                    的夹角；直视正下方时为 0，取值 [0, 90]。
//
//  两套量测基准：
//    椭球面上不同点的法线方向不同，因此「在视点处量测」与「在目标点处量测」
//    会存在系统性差异（200km 高度、45° 俯仰角时约 1.2°）。二者都对外提供，
//    分别对应地球视角的两种常见理解，便于交叉核验。
//

#ifndef GNX_EARTHENGINE_CORE_EARTHCAMERAPOSE_INCLUDE_JKSDHG
#define GNX_EARTHENGINE_CORE_EARTHCAMERAPOSE_INCLUDE_JKSDHG

#include "Ellipsoid.h"

EARTH_CORE_NAMESPACE_BEGIN

// 局部东北天参考系（East - Up - North 的直角基），x=东, y=北, z=天(大地法线)。
// 基向量均已单位化，且为右手系：east × north = up。
struct EunFrame
{
    Vector3d east;
    Vector3d north;
    Vector3d up;

    // 把 EUN 下的方向/位移向量转换到椭球固连坐标系(ECEF)
    Vector3d ToWorld(const Vector3d& vectorInEun) const;

    // 把椭球固连坐标系(ECEF)下的向量转换到本 EUN 参考系。
    // 基是标准正交的，因此逆变换就是与三个基向量做点积。
    Vector3d ToEun(const Vector3d& vectorInWorld) const;
};

class EARTH_CORE_EXPORT EarthCameraPose
{
public:
    // 角度与弧度换算（MathUtil 自带的 degToRad 是 float 版本，这里统一用 double）
    static double ToRadians(double degrees) { return degrees * 0.01745329251994329576923690768489; }
    static double ToDegrees(double radians) { return radians * 57.295779513082320876798154814105; }

    // 构建 origin 处的东北天参考系
    static EunFrame BuildEunFrame(const Vector3d& origin, const Ellipsoid& ellipsoid);

    // EUN 下的视线方向（视点 -> 目标点），单位向量。
    //   ViewDirectionFromAngles(0, 0)          -> (0, 0, -1)   垂直下视
    //   ViewDirectionFromAngles(0, rad(45))    -> 正北且下俯 45°
    //   ViewDirectionFromAngles(rad(90), p)    -> 正东且下俯 p
    static Vector3d ViewDirectionFromAngles(double azimuthRad, double pitchRad);

    // 以 targetPos 处的大地法线为基准，由 (方位角, 俯仰角, 距离) 求视点空间直角坐标。
    // 恒等式：|返回值 - targetPos| == distance；pitch=0 时视点位于目标点正上方。
    // 视点绕目标点做轨道旋转（目标点与视线距离都不变），因此无需迭代求解。
    static Vector3d EyeFromTargetAndAngles(const Vector3d& targetPos,
                                           const Ellipsoid& ellipsoid,
                                           double azimuthRad,
                                           double pitchRad,
                                           double distance);

    // LookAt 所需的 up 向量：把「视点处」大地法线投影到与视线垂直的平面。
    //
    // 这是零滚转（地平线水平）的严格解：投影结果与视线精确正交，且与视点处大地法线
    // 正交，因此相机右向量落在当地水平面内，画面不会倾斜。
    //
    // 不能用「在视点处按方位角构造水平方向」来替代：俯仰角大、距离远时视点会明显偏离
    // 目标点的法线（2000km 距离、90 度俯仰时偏差可达 17 度），此时按方位角构造的水平方向
    // 与真实视线的水平分量不再一致，会造成肉眼可见的画面滚转。
    //
    // 垂直下视（俯仰角为 0）时法线与视线平行、投影退化为零向量，此时按方位角确定的
    // 水平方向作为约定值（正北朝上），与俯仰角 → 0 的极限一致，保证连续。
    static Vector3d LookUpForLookAt(const Vector3d& eyePos,
                                    const Vector3d& targetPos,
                                    const Ellipsoid& ellipsoid,
                                    double azimuthRad);

    // 由 (视点, 目标点) 反算方位角与俯仰角。
    //   measureAtEye = true ：严格按用户定义，在「视点处」用视点的大地法线量测
    //   measureAtEye = false：在「目标点处」用目标点的大地法线量测（与内部轨道基准一致）
    // 俯仰角输出 [0, π/2]；方位角输出 [0, 2π)。垂直下视时方位角在数学上无定义，返回 0。
    static void AnglesFromEyeTarget(const Vector3d& eyePos,
                                    const Vector3d& targetPos,
                                    const Ellipsoid& ellipsoid,
                                    bool measureAtEye,
                                    double& outAzimuthRad,
                                    double& outPitchRad);

    // 方位角规约到 [0, 2π)
    static double NormalizeAzimuth(double azimuthRad);

    // 俯仰角钳制到 [0, π/2]
    static double ClampPitch(double pitchRad);
};

EARTH_CORE_NAMESPACE_END

#endif /* GNX_EARTHENGINE_CORE_EARTHCAMERAPOSE_INCLUDE_JKSDHG */
